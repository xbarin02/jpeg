#include <stddef.h>
#include <assert.h>
#include "common.h"

int init_qtable(struct qtable *qtable)
{
	assert(qtable != NULL);

	qtable->precision = 0;

	for (int i = 0; i < 64; ++i) {
		qtable->element[i] = 0;
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

	context->precision = 0;

	context->Y = 0;
	context->X = 0;

	context->components = 0;

	for (int i = 0; i < 256; ++i) {
		init_component(&context->component[i]);
	}

	for (int j = 0; j < 2; ++j) {
		for (int i = 0; i < 4; ++i) {
			init_htable(&context->htable[j][i]);
		}
	}

	context->Ri = 0;

	context->m_x = 0;
	context->m_y = 0;

	context->mblocks = 0;

	return RET_SUCCESS;
}
