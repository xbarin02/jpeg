#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include "common.h"
#include "frame.h"
#include "coeffs.h"
#include "imgproc.h"

int read_image(struct context *context, FILE *stream)
{
	int err;

	struct frame frame;

	assert(context != NULL);

	// load PPM/PGM header, detect X, Y, number of components, bpp
	err = read_frame_header(&frame, stream);
	RETURN_IF(err);

	printf("[DEBUG] header Nf=%" PRIu8 " Y=%" PRIu16 " X=%" PRIu16 " P=%" PRIu8 "\n", frame.components, frame.Y, frame.X, frame.precision);

	context->Nf = frame.components;
	context->Y = frame.Y;
	context->X = frame.X;
	context->P = frame.precision;

	switch (frame.components) {
		case 1:
			context->component[1].H = 1;
			context->component[1].V = 1;
			context->component[1].Tq = 0;
			context->max_H = 1;
			context->max_V = 1;
			break;
		case 3:
			context->component[1].H = 2;
			context->component[1].V = 2;
			context->component[2].H = 1;
			context->component[2].V = 1;
			context->component[3].H = 1;
			context->component[3].V = 1;
			context->component[1].Tq = 0;
			context->component[2].Tq = 1;
			context->component[3].Tq = 1;
			context->max_H = 2;
			context->max_V = 2;
			break;
		default:
			return RET_FAILURE_FILE_UNSUPPORTED;
	}

	err = frame_create_empty(context, &frame);
	RETURN_IF(err);

	// load frame body
	err = read_frame_body(&frame, stream);
	RETURN_IF(err);

	printf("[DEBUG] frame data loaded\n");

	err = compute_no_blocks_and_alloc_buffers(context);
	RETURN_IF(err);

	err = frame_to_ycc(&frame);
	RETURN_IF(err);

	// copy frame->data[] into context->component[]->frame_buffer[]
	transform_frame_to_components(context, &frame);

	frame_destroy(&frame);

	return RET_SUCCESS;
}

/* read_image(), conv_frame_to_blocks(), forward_dct(), quantize() */
int prologue(struct context *context, FILE *i_stream)
{
	int err;

	err = read_image(context, i_stream);
	RETURN_IF(err);

	err = conv_frame_to_blocks(context);
	RETURN_IF(err);

	err = forward_dct(context);
	RETURN_IF(err);

	err = quantize(context);
	RETURN_IF(err);

	return RET_SUCCESS;
}

int produce_DQT(struct context *context, uint8_t Tq, FILE *stream)
{
	int err;

	assert(context != NULL);

	err = write_marker(stream, 0xffdb);
	RETURN_IF(err);

	// length = 2 (len) + 1 (Pq, Tq) + 64 (Q[]) = 67
	err = write_length(stream, 67);
	RETURN_IF(err);

	uint8_t Pq;
	Pq = 0;

	err = write_nibbles(stream, Pq, Tq);
	RETURN_IF(err);

	struct qtable *qtable = &context->qtable[Tq];

	for (int i = 0; i < 64; ++i) {
		uint8_t byte = (uint8_t)qtable->Q[zigzag[i]];

		err = write_byte(stream, byte);
		RETURN_IF(err);
	}

	return RET_SUCCESS;
}

int produce_SOF0(struct context *context, FILE *stream)
{
	int err;

	assert(context != NULL);

	err = write_marker(stream, 0xffc0);
	RETURN_IF(err);

	uint8_t Nf = context->Nf;

	// length = 2 (len) + 1 (P) + 2 (Y) + 2 (X) + 1 (Nf) + Nf * ( 1 (C) + 1 (H, V) + 1 (Tq) ) = 8 + 3 * Nf
	err = write_length(stream, 8 + 3 * Nf);
	RETURN_IF(err);

	err = write_byte(stream, context->P);
	RETURN_IF(err);
	err = write_word(stream, context->Y);
	RETURN_IF(err);
	err = write_word(stream, context->X);
	RETURN_IF(err);
	err = write_byte(stream, context->Nf);
	RETURN_IF(err);

	for (int i = 0; i < 256; ++i) {
		if (context->component[i].H != 0) {
			err = write_byte(stream, (uint8_t)i);
			RETURN_IF(err);

			err = write_nibbles(stream, context->component[i].H, context->component[i].V);
			RETURN_IF(err);

			err = write_byte(stream, context->component[i].Tq);
			RETURN_IF(err);
		}
	}

	return RET_SUCCESS;
}

int produce_DHT(struct context *context, uint8_t Tc, uint8_t Th, FILE *stream)
{
	int err;

	assert(context != NULL);

	err = write_marker(stream, 0xffc4);
	RETURN_IF(err);

	struct htable *htable = &context->htable[Tc][Th];

	// compute "mt (V)"
	uint16_t mt = 0;
	for (int i = 0; i < 16; ++i) {
		uint8_t L = htable->L[i];
		mt += L;
	}

	// length = 2 (len) + 1 (Tc, Th) + 16 * 1 (L) + mt (V) = 2 + 17 + mt (V)
	err = write_length(stream, 2 + 17 + mt);
	RETURN_IF(err);

	err = write_nibbles(stream, Tc, Th);
	RETURN_IF(err);

	for (int i = 0; i < 16; ++i) {
		err = write_byte(stream, htable->L[i]);
		RETURN_IF(err);
	}

	for (int i = 0; i < 16; ++i) {
		uint8_t L = htable->L[i];

		for (int l = 0; l < L; ++l) {
			err = write_byte(stream, htable->V[i][l]);
			RETURN_IF(err);
		}
	}

	return RET_SUCCESS;
}

int produce_codestream(struct context *context, FILE *stream)
{
	int err;

	/* SOI */
	err = write_marker(stream, 0xffd8);
	RETURN_IF(err);

	/* DQT */
	err = produce_DQT(context, 0, stream);
	RETURN_IF(err);
	err = produce_DQT(context, 1, stream);
	RETURN_IF(err);

	/* SOF0 */
	err = produce_SOF0(context, stream);
	RETURN_IF(err);

	/* DHT */
	err = produce_DHT(context, 0, 0, stream);
	RETURN_IF(err);
	err = produce_DHT(context, 1, 0, stream);
	RETURN_IF(err);
	err = produce_DHT(context, 0, 1, stream);
	RETURN_IF(err);
	err = produce_DHT(context, 1, 1, stream);
	RETURN_IF(err);

	/* TODO SOS */

	/* TODO loop over macroblocks */

	/* TODO EOI */

	return RET_SUCCESS;
}

int process_stream(FILE *i_stream, FILE *o_stream)
{
	int err;

	struct context *context = malloc(sizeof(struct context));

	err = init_context(context);
	RETURN_IF(err);

	err = prologue(context, i_stream);
	RETURN_IF(err);

	err = produce_codestream(context, o_stream);
	RETURN_IF(err);

	// TODO
#if 0
	err = dequantize(context);
	RETURN_IF(err);
	err = inverse_dct(context);
	RETURN_IF(err);
	err = conv_blocks_to_frame(context);
	RETURN_IF(err);
	err = write_image(context, NULL);
	RETURN_IF(err);
#endif

	free_buffers(context);

	free(context);

	return RET_SUCCESS;
}

int main(int argc, char *argv[])
{
	const char *i_path = argc > 1 ? argv[1] : "Lenna.ppm";
	const char *o_path = argc > 2 ? argv[2] : "output.jpg";

	FILE *i_stream = fopen(i_path, "r");
	FILE *o_stream = fopen(o_path, "w");

	if (i_stream == NULL) {
		fprintf(stderr, "fopen failure\n");
		return 1;
	}

	if (o_stream == NULL) {
		fprintf(stderr, "fopen failure\n");
		return 1;
	}

	int err = process_stream(i_stream, o_stream);

	if (err) {
		fprintf(stderr, "Failure.\n");
	}

	fclose(o_stream);
	fclose(i_stream);

	return 0;
}
