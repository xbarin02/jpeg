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
		if (context->component[i].buffer != NULL) {
			printf("Dequantizing component %i...\n", i);

			size_t blocks = context->component[i].b_x * context->component[i].b_y;

			// remove differential DC coding
			for (size_t b = 1; b < blocks; ++b) {
				struct block *prev_block = &context->component[i].buffer[b - 1];
				struct block *this_block = &context->component[i].buffer[b];

				int32_t pred = prev_block->c[0];

				// FIXME what is wrong?
// 				this_block->c[0] += pred;
			}

			uint8_t Tq = context->component[i].Tq;
			struct qtable *qtable = &context->qtable[Tq];

			// for each block, for each coefficient, c[] *= Q[]
			for (size_t b = 0; b < blocks; ++b) {
				struct block *block = &context->component[i].buffer[b];

				for (int j = 0; j < 64; ++j) {
					block->c[j] *= (int32_t)qtable->element[j];
				}
			}
		}
	}

	return RET_SUCCESS;
}

struct flt_block {
	float c[8][8];
};

float C(int u)
{
	if (u == 0) {
		return 1.f / sqrtf(2.f);
	}

	return 1.f;
}

void idct(struct flt_block *fb)
{
	/* clone input into S */
	struct flt_block S = *fb;

	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			float *s = &fb->c[y][x];

			*s = 0.f;

			for (int u = 0; u < 8; ++u) {
				for (int v = 0; v < 8; ++v) {
					*s = C(u) * C(v) * S.c[v][u] * cosf((2*x+1)*u*M_PI/16) * cosf((2*y+1)*v*M_PI/16);
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
		if (context->component[i].buffer != NULL) {
			printf("IDCT on component %i...\n", i);

			size_t blocks = context->component[i].b_x * context->component[i].b_y;

			for (size_t b = 0; b < blocks; ++b) {
				struct block *block = &context->component[i].buffer[b];

				/* HACK */
				struct flt_block fb;

				for (int j = 0; j < 64; ++j) {
					fb.c[j / 8][j % 8] = (float)block->c[j];
				}

// 				idct(&fb);

				uint8_t P = context->precision;
				int shift = 1 << (P - 1);

				// level shift
// 				for (int j = 0; j < 64; ++j) {
// 					fb.c[j / 8][j % 8] += shift;
// 				}

				/* HACK */
// 				for (int y = 0; y < 8; ++y) {
// 					for (int x = 0; x < 8; ++x) {
// // 						printf(" %f", fb.c[y][x]);
// 						printf(" %i", block->c[8*y+x]);
// 					}
// 					printf("\n");
// 				}
// 				printf("\n");
			}
		}
	}

	return RET_SUCCESS;
}
