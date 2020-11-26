#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include "imgproc.h"
#include "coeffs.h"

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

				this_block->c[0] += pred;
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
