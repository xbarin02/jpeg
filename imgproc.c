#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "imgproc.h"
#include "coeffs.h"
#include "frame.h"

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
					int_block->c[j] *= (int32_t)qtable->Q[j];
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

				/* precision */
				uint8_t P = context->P;
				int shift = 1 << (P - 1);

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

int write_image(struct context *context, const char *path)
{
	int err;

	struct frame frame;

	err = frame_create(context, &frame);
	RETURN_IF(err);
	err = frame_to_rgb(&frame);
	RETURN_IF(err);
	err = write_frame(&frame, path);
	RETURN_IF(err);
	frame_destroy(&frame);

	return RET_SUCCESS;
}
