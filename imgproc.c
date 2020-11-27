#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "imgproc.h"
#include "coeffs.h"

int dequantize(struct context *context)
{
	assert(context != NULL);

	for (int i = 0; i < 256; ++i) {
		if (context->component[i].int_buffer != NULL) {
			printf("Dequantizing component %i...\n", i);

			size_t blocks = context->component[i].b_x * context->component[i].b_y;

			uint8_t Tq = context->component[i].Tq;
			struct qtable *qtable = &context->qtable[Tq];

			// for each block, for each coefficient, c[] *= Q[]
			for (size_t b = 0; b < blocks; ++b) {
				struct int_block *int_block = &context->component[i].int_buffer[b];

				for (int j = 0; j < 64; ++j) {
					int_block->c[j] *= (int32_t)qtable->element[j];
				}
			}
		}
	}

	return RET_SUCCESS;
}

static float C(int u)
{
	if (u == 0) {
		return 1.f / sqrtf(2.f);
	}

	return 1.f;
}

float lut[8][8];

void init_lut()
{
	for (int x = 0; x < 8; ++x) {
		for (int u = 0; u < 8; ++u) {
			lut[x][u] = 0.5f * C(u) * cosf((2 * x + 1) * u * M_PI / 16);
		}
	}
}

void dct1(const float in[8], float out[8], size_t stride)
{
	for (int x = 0; x < 8; ++x) {
		float s = 0.f;

		for (int u = 0; u < 8; ++u) {
			s += in[u * stride] * lut[x][u];
		}

		out[x * stride] = s;
	}
}

void idct(struct flt_block *flt_block)
{
	static int init = 0;

	// init look-up table
	if (init == 0) {
		init_lut();
		init = 1;
	}

	struct flt_block b;

	for (int y = 0; y < 8; ++y) {
		dct1(flt_block->c[y], b.c[y], 1);
	}

	for (int x = 0; x < 8; ++x) {
		dct1(&b.c[0][x], &flt_block->c[0][x], 8);
	}
}

int invert_dct(struct context *context)
{
	assert(context != NULL);

	for (int i = 0; i < 256; ++i) {
		if (context->component[i].int_buffer != NULL) {
			printf("IDCT on component %i...\n", i);

			size_t blocks = context->component[i].b_x * context->component[i].b_y;

			for (size_t b = 0; b < blocks; ++b) {
				struct int_block *int_block = &context->component[i].int_buffer[b];

				struct flt_block *flt_block = &context->component[i].flt_buffer[b];

				for (int j = 0; j < 64; ++j) {
					flt_block->c[j / 8][j % 8] = (float)int_block->c[j];
				}

				idct(flt_block);

				uint8_t P = context->precision;
				int shift = 1 << (P - 1);

				assert(shift == 128);

				// level shift
				for (int j = 0; j < 64; ++j) {
					flt_block->c[j / 8][j % 8] += shift;
				}
			}
		}
	}

	return RET_SUCCESS;
}

/* convert floating-point blocks to frame buffers (for each component) */
int conv_blocks_to_frame(struct context *context)
{
	assert(context != NULL);

	for (int i = 0; i < 256; ++i) {
		if (context->component[i].frame_buffer != NULL) {
			printf("converting component %i...\n", i);

			float *buffer = context->component[i].frame_buffer;

			size_t b_x = context->component[i].b_x;
			size_t b_y = context->component[i].b_y;

			for (size_t y = 0; y < b_y; ++y) {
				for (size_t x = 0; x < b_x; ++x) {
					/* copy from... */
					struct flt_block *flt_block = &context->component[i].flt_buffer[y * b_x + x];

					for (int v = 0; v < 8; ++v) {
						for (int u = 0; u < 8; ++u) {
							buffer[y * b_x * 8 * 8 + v * b_x * 8 + x * 8 + u] = flt_block->c[v][u];
						}
					}
				}
			}
		}
	}

	return RET_SUCCESS;
}

int clamp(int min, int val, int max)
{
	if (val < min) {
		return min;
	}

	if (val > max) {
		return max;
	}

	return val;
}

int dump_components(struct context *context)
{
	assert(context != NULL);

	for (int i = 0; i < 256; ++i) {
		if (context->component[i].frame_buffer != NULL) {
			printf("store component %i...\n", i);

			char path[4096];
			sprintf(path, "comp%i.pgm", i);

			FILE *stream = fopen(path, "w");

			if (stream == NULL) {
				return RET_FAILURE_FILE_OPEN;
			}

			float *buffer = context->component[i].frame_buffer;

			size_t b_x = context->component[i].b_x;
			size_t b_y = context->component[i].b_y;

			size_t size_x = b_x * 8;
			size_t size_y = b_y * 8;

			fprintf(stream, "P2\n%zu %zu\n255\n", size_x, size_y);

			for (size_t y = 0; y < size_y; ++y) {
				for (size_t x = 0; x < size_x; ++x) {
					fprintf(stream, "%i ", clamp(0, (int)buffer[y * size_x + x], 255));
				}
				fprintf(stream, "\n");
			}

			fclose(stream);
		}
	}

	return RET_SUCCESS;
}

struct frame {
	uint8_t components;
	uint16_t Y, X;
	size_t size_x, size_y;

	float *data;
};

void frame_destroy(struct frame *frame)
{
	free(frame->data);
}

static size_t ceil_div(size_t n, size_t d)
{
	return (n + (d - 1)) / d;
}

int frame_create(struct context *context, struct frame *frame)
{
	assert(context != NULL);
	assert(frame != NULL);

	frame->components = context->components;
	frame->Y = context->Y;
	frame->X = context->X;

	size_t size_x = ceil_div(frame->X, 8 * context->max_H) * 8 * context->max_H;
	size_t size_y = ceil_div(frame->Y, 8 * context->max_V) * 8 * context->max_V;

	frame->size_x = size_x;
	frame->size_y = size_y;

	printf("[DEBUG] frame components=%i X=%zu Y=%zu\n", (int)frame->components, size_x, size_y);

	// alloc frame->data[]
	frame->data = malloc(sizeof(float) * frame->components * size_x * size_y);

	if (frame->data == NULL) {
		return RET_FAILURE_MEMORY_ALLOCATION;
	}

	// component id
	int compno = 0;

	for (int i = 0; i < 256; ++i) {
		if (context->component[i].frame_buffer != NULL) {
			size_t b_x = context->component[i].b_x;
			size_t b_y = context->component[i].b_y;

			size_t c_x = b_x * 8;
			size_t c_y = b_y * 8;

			size_t step_x = size_x / c_x;
			size_t step_y = size_y / c_y;

			float *buffer = context->component[i].frame_buffer;

			// iterate over component raster (smaller than frame raster)
			for (size_t y = 0; y < c_y; ++y) {
				for (size_t x = 0; x < c_x; ++x) {
					// (i,y,x) to index component
					// (compno,step*y,step*x) to index frame

					float px = buffer[y * c_x + x];

					// copy patch
					for (size_t yy = 0; yy < step_y; ++yy) {
						for (size_t xx = 0; xx < step_x; ++xx) {
							frame->data[(step_y * y + yy) * size_x * frame->components + frame->components * (step_x * x + xx) + compno] = px;
						}
					}
				}
			}

			compno++;
		}
	}

	return RET_SUCCESS;
}

int frame_to_rgb(struct frame *frame)
{
	assert(frame != NULL);

	switch (frame->components) {
		case 4:
			for (size_t y = 0; y < frame->Y; ++y) {
				for (size_t x = 0; x < frame->X; ++x) {
					float Y_ = frame->data[y * frame->size_x * 4 + x * 4 + 0];
					float Cb = frame->data[y * frame->size_x * 4 + x * 4 + 1];
					float Cr = frame->data[y * frame->size_x * 4 + x * 4 + 2];
					float K  = frame->data[y * frame->size_x * 4 + x * 4 + 3];

					float C = Y_ + 1.402 * (Cr - 128);
					float M = Y_ - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128);
					float Y = Y_ + 1.772 * (Cb - 128);

					float R = K - (C * K) / 256;
					float G = K - (M * K) / 256;
					float B = K - (Y * K) / 256;

					frame->data[y * frame->size_x * 4 + x * 4 + 0] = R;
					frame->data[y * frame->size_x * 4 + x * 4 + 1] = G;
					frame->data[y * frame->size_x * 4 + x * 4 + 2] = B;
					frame->data[y * frame->size_x * 4 + x * 4 + 3] = 0xff;
				}
			}
			break;
		case 3:
			for (size_t y = 0; y < frame->Y; ++y) {
				for (size_t x = 0; x < frame->X; ++x) {
					float Y  = frame->data[y * frame->size_x * 3 + x * 3 + 0];
					float Cb = frame->data[y * frame->size_x * 3 + x * 3 + 1];
					float Cr = frame->data[y * frame->size_x * 3 + x * 3 + 2];

					float R = Y + 1.402 * (Cr - 128);
					float G = Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128);
					float B = Y + 1.772 * (Cb - 128);

					frame->data[y * frame->size_x * 3 + x * 3 + 0] = R;
					frame->data[y * frame->size_x * 3 + x * 3 + 1] = G;
					frame->data[y * frame->size_x * 3 + x * 3 + 2] = B;
				}
			}
			break;
		case 1:
			/* nothing to do */
			break;
		default:
			abort();
	}

	return RET_SUCCESS;
}

int dump_frame(struct frame *frame)
{
	assert(frame != NULL);

	switch (frame->components) {
		FILE *stream;
		case 4:
			stream = fopen("frame.ppm", "w");
			if (stream == NULL) {
				return RET_FAILURE_FILE_OPEN;
			}
			fprintf(stream, "P3\n%zu %zu\n255\n", (size_t)frame->X, (size_t)frame->Y);
			for (size_t y = 0; y < frame->Y; ++y) {
				for (size_t x = 0; x < frame->X; ++x) {
					for (int c = 0; c < 3; ++c) {
						fprintf(stream, "%i ", clamp(0, (int)roundf(frame->data[y * frame->size_x * 4 + x * 4 + c]), 255));
					}
				}
				fprintf(stream, "\n");
			}
			fclose(stream);
			break;
		case 3:
			stream = fopen("frame.ppm", "w");
			if (stream == NULL) {
				return RET_FAILURE_FILE_OPEN;
			}
			fprintf(stream, "P3\n%zu %zu\n255\n", (size_t)frame->X, (size_t)frame->Y);
			for (size_t y = 0; y < frame->Y; ++y) {
				for (size_t x = 0; x < frame->X; ++x) {
					for (int c = 0; c < 3; ++c) {
						fprintf(stream, "%i ", clamp(0, (int)roundf(frame->data[y * frame->size_x * 3 + x * 3 + c]), 255));
					}
				}
				fprintf(stream, "\n");
			}
			fclose(stream);
			break;
		case 1:
			stream = fopen("frame.pgm", "w");
			if (stream == NULL) {
				return RET_FAILURE_FILE_OPEN;
			}
			fprintf(stream, "P2\n%zu %zu\n255\n", (size_t)frame->X, (size_t)frame->Y);
			for (size_t y = 0; y < frame->Y; ++y) {
				for (size_t x = 0; x < frame->X; ++x) {
					fprintf(stream, "%i ", clamp(0, (int)roundf(frame->data[y * frame->size_x + x]), 255));
				}
				fprintf(stream, "\n");
			}
			fclose(stream);
			break;
		default:
			abort();
	}

	return RET_SUCCESS;
}

int dump_image(struct context *context)
{
	int err;

	struct frame frame;

	err = frame_create(context, &frame);
	RETURN_IF(err);
	err = frame_to_rgb(&frame);
	RETURN_IF(err);
	err = dump_frame(&frame);
	RETURN_IF(err);
	frame_destroy(&frame);

	return RET_SUCCESS;
}
