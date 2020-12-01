#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include "common.h"
#include "mjpeg.h"
#include "huffman.h"
#include "coeffs.h"

int init_qtable(struct qtable *qtable)
{
	assert(qtable != NULL);

	qtable->Pq = 0;

	for (int i = 0; i < 64; ++i) {
		qtable->Q[i] = 0;
	}

	return RET_SUCCESS;
}

int init_component(struct component *component)
{
	assert(component != NULL);

	component->H = 0;
	component->V = 0;

	component->Tq = 0;

	component->Td = 0;
	component->Ta = 0;

	component->b_x = 0;
	component->b_y = 0;

	component->int_buffer = NULL;
	component->flt_buffer = NULL;

	component->frame_buffer = NULL;

	return RET_SUCCESS;
}

int init_htable(struct htable *htable)
{
	assert(htable != NULL);

	for (int i = 0; i < 16; ++i) {
		htable->L[i] = 0;
	}

	for (int i = 0; i < 16; ++i) {
		for (int j = 0; j < 255; ++j) {
			htable->V[i][j] = 0;
		}
	}

	return RET_SUCCESS;
}

int init_context(struct context *context)
{
	assert(context != NULL);

	for (int i = 0; i < 4; ++i) {
		init_qtable(&context->qtable[i]);
	}

	context->P = 0;

	context->Y = 0;
	context->X = 0;

	context->Nf = 0;

	for (int i = 0; i < 256; ++i) {
		init_component(&context->component[i]);
	}

	for (int j = 0; j < 2; ++j) {
		for (int i = 0; i < 4; ++i) {
			init_htable(&context->htable[j][i]);
		}
	}

	/* implicit MJPEG tables */
	context->htable[0][0] = mjpg_htable_0_0;
	context->htable[0][1] = mjpg_htable_0_1;
	context->htable[1][0] = mjpg_htable_1_0;
	context->htable[1][1] = mjpg_htable_1_1;

	conv_htable_to_hcode(&context->htable[0][0], &context->hcode[0][0]);
	conv_htable_to_hcode(&context->htable[0][1], &context->hcode[0][1]);
	conv_htable_to_hcode(&context->htable[1][0], &context->hcode[1][0]);
	conv_htable_to_hcode(&context->htable[1][1], &context->hcode[1][1]);

	context->Ri = 0;

	context->m_x = 0;
	context->m_y = 0;

	context->mblocks = 0;

	return RET_SUCCESS;
}

size_t ceil_div(size_t n, size_t d)
{
	return (n + (d - 1)) / d;
}

int alloc_buffers(struct component *component, size_t size)
{
	component->int_buffer = malloc(sizeof(struct int_block) * size);

	if (component->int_buffer == NULL) {
		return RET_FAILURE_MEMORY_ALLOCATION;
	}

	component->flt_buffer = malloc(sizeof(struct flt_block) * size);

	if (component->flt_buffer == NULL) {
		return RET_FAILURE_MEMORY_ALLOCATION;
	}

	component->frame_buffer = malloc(sizeof(float) * 64 * size);

	if (component->frame_buffer == NULL) {
		return RET_FAILURE_MEMORY_ALLOCATION;
	}

	return RET_SUCCESS;
}

void free_buffers(struct context *context)
{
	for (int i = 0; i < 256; ++i) {
		free(context->component[i].int_buffer);
		free(context->component[i].flt_buffer);

		free(context->component[i].frame_buffer);
	}
}
