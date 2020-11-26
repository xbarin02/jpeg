#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include "imgproc.h"
#include "coeffs.h"

/* FIXME
 * At the beginning of the scan and at the beginning of each restart interval, the prediction for the DC coefficient prediction
 * is initialized to 0. */

int dequantize(struct context *context)
{
	assert(context != NULL);

	for (int i = 0; i < 256; ++i) {
		if (context->component[i].int_buffer != NULL) {
			printf("Dequantizing component %i...\n", i);

			size_t blocks = context->component[i].b_x * context->component[i].b_y;

			// remove differential DC coding
			for (size_t b = 1; b < blocks; ++b) {
				struct int_block *prev_block = &context->component[i].int_buffer[b - 1];
				struct int_block *this_block = &context->component[i].int_buffer[b];

				int32_t pred = prev_block->c[0];

				this_block->c[0] += pred;
			}

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
							buffer[(y + v) * b_x * 8 + (x + u) * 8] = flt_block->c[v][u];
						}
					}
				}
			}
		}
	}

	return RET_SUCCESS;
}
