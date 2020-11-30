#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <arpa/inet.h>
#include "frame.h"

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

static size_t ceil_div(size_t n, size_t d)
{
	return (n + (d - 1)) / d;
}

void frame_destroy(struct frame *frame)
{
	free(frame->data);
}

int frame_create(struct context *context, struct frame *frame)
{
	assert(context != NULL);
	assert(frame != NULL);

	frame->components = context->Nf;
	frame->Y = context->Y;
	frame->X = context->X;
	frame->precision = context->P;

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

	int shift = 1 << (frame->precision - 1);
	int denom = 1 << frame->precision;

	switch (frame->components) {
		case 4:
			for (size_t y = 0; y < frame->Y; ++y) {
				for (size_t x = 0; x < frame->X; ++x) {
					float Y_ = frame->data[y * frame->size_x * 4 + x * 4 + 0];
					float Cb = frame->data[y * frame->size_x * 4 + x * 4 + 1];
					float Cr = frame->data[y * frame->size_x * 4 + x * 4 + 2];
					float K  = frame->data[y * frame->size_x * 4 + x * 4 + 3];

					float C = Y_ + 1.402 * (Cr - shift);
					float M = Y_ - 0.34414 * (Cb - shift) - 0.71414 * (Cr - shift);
					float Y = Y_ + 1.772 * (Cb - shift);

					float R = K - (C * K) / denom;
					float G = K - (M * K) / denom;
					float B = K - (Y * K) / denom;

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

					float R = Y + 1.402 * (Cr - shift);
					float G = Y - 0.34414 * (Cb - shift) - 0.71414 * (Cr - shift);
					float B = Y + 1.772 * (Cb - shift);

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

size_t convert_maxval_to_size(int maxval)
{
	assert(maxval > 0);

	if (maxval <= UINT8_MAX)
		return sizeof(uint8_t);
	if (maxval <= UINT16_MAX)
		return sizeof(uint16_t);

	/* not supported */
	return 0;
}

int dump_frame_body(struct frame *frame, int components, FILE *stream)
{
	assert(frame != NULL);

	uint8_t Nf = frame->components;
	int maxval = (1 << frame->precision) - 1;
	size_t sample_size = convert_maxval_to_size(maxval);
	size_t width = (size_t)frame->X;
	size_t height = (size_t)frame->Y;
	size_t line_size = sample_size * components * width;

	void *line = malloc(line_size);

	if (line == NULL) {
		return RET_FAILURE_MEMORY_ALLOCATION;
	}

	for (size_t y = 0; y < height; ++y) {
		switch (sample_size) {
			case sizeof(uint8_t): {
				uint8_t *line_ = line;
				for (size_t x = 0; x < width; ++x) {
					for (int c = 0; c < components; ++c) {
						float sample = roundf(frame->data[y * frame->size_x * Nf + x * Nf + c]);
						*line_++ = (uint8_t)clamp(0, (int)sample, maxval);
					}
				}
				break;
			}
			case sizeof(uint16_t): {
				uint16_t *line_ = line;
				for (size_t x = 0; x < width; ++x) {
					for (int c = 0; c < components; ++c) {
						float sample = roundf(frame->data[y * frame->size_x * Nf + x * Nf + c]);
						*line_++ = htons((uint16_t)clamp(0, (int)sample, maxval));
					}
				}
				break;
			}
			default:
				return RET_FAILURE_LOGIC_ERROR;
		}
		/* write line */
		if (fwrite(line, 1, line_size, stream) < line_size) {
			free(line);
			return RET_FAILURE_FILE_IO;
		}
	}

	free(line);

	return RET_SUCCESS;
}

int dump_frame_header(struct frame *frame, int components, FILE *stream)
{
	assert(frame != NULL);

	int maxval = (1 << frame->precision) - 1;

	switch (components) {
		case 3:
			if (fprintf(stream, "P6\n%" PRIu16 " %" PRIu16 "\n%i\n", frame->X, frame->Y, maxval) < 0) {
				return RET_FAILURE_FILE_IO;
			}
			break;
		case 1:
			if (fprintf(stream, "P5\n%" PRIu16 " %" PRIu16 "\n%i\n", frame->X, frame->Y, maxval) < 0) {
				return RET_FAILURE_FILE_IO;
			}
			break;
		default:
			return RET_FAILURE_FILE_UNSUPPORTED;
	}

	return RET_SUCCESS;
}

int dump_frame(struct frame *frame)
{
	assert(frame != NULL);

	int err;

	switch (frame->components) {
		FILE *stream;
		case 4:
			stream = fopen("output.ppm", "w");
			if (stream == NULL) {
				return RET_FAILURE_FILE_OPEN;
			}
			err = dump_frame_header(frame, 3, stream);
			RETURN_IF(err);
			dump_frame_body(frame, 3, stream);
			fclose(stream);
			break;
		case 3:
			stream = fopen("output.ppm", "w");
			if (stream == NULL) {
				return RET_FAILURE_FILE_OPEN;
			}
			err = dump_frame_header(frame, 3, stream);
			RETURN_IF(err);
			dump_frame_body(frame, 3, stream);
			fclose(stream);
			break;
		case 1:
			stream = fopen("output.pgm", "w");
			if (stream == NULL) {
				return RET_FAILURE_FILE_OPEN;
			}
			err = dump_frame_header(frame, 1, stream);
			RETURN_IF(err);
			dump_frame_body(frame, 1, stream);
			fclose(stream);
			break;
		default:
			abort();
	}

	return RET_SUCCESS;
}
