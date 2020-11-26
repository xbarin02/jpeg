#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
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

float C(int u)
{
	if (u == 0) {
		return 1.f / sqrtf(2.f);
	}

	return 1.f;
}

void idct(struct flt_block *flt_block)
{
	/* clone input into S */
	struct flt_block S = *flt_block;

	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			float *s = &flt_block->c[y][x];

			*s = 0.f;

			for (int u = 0; u < 8; ++u) {
				for (int v = 0; v < 8; ++v) {
					*s += C(u) * C(v) * S.c[v][u] * cosf((2*x+1)*u*M_PI/16) * cosf((2*y+1)*v*M_PI/16);
				}
			}
			*s *= 0.25f;
		}
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

				/* HACK */
// 				for (int y = 0; y < 8; ++y) {
// 					for (int x = 0; x < 8; ++x) {
// 						printf(" %f", flt_block->c[y][x]);
// // 						printf(" %i", int_block->c[8*y+x]);
// 					}
// 					printf("\n");
// 				}
// 				printf("\n");
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
							/* FIXME: not sure about this */
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
