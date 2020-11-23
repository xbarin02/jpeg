#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "common.h"
#include "io.h"

struct qtable {
	/* Value 0 indicates 8-bit Qk values; value 1 indicates 16-bit Qk values. */
	uint8_t precision;
	/* in zig-zag scan order */
	uint16_t element[64];
};

int init_qtable(struct qtable *qtable)
{
	assert(qtable != NULL);

	qtable->precision = 0;

	for (int i = 0; i < 64; ++i) {
		qtable->element[i] = 0;
	}

	return RET_SUCCESS;
}

struct component {
	/* Horizontal sampling factor, Vertical sampling factor */
	uint8_t H, V;
	/* Quantization table destination selector */
	uint8_t Tq;

	/* DC entropy coding table destination selector
	 * AC entropy coding table destination selector */
	uint8_t Td, Ta;
};

int init_component(struct component *component)
{
	assert(component != NULL);

	component->H = 0;
	component->V = 0;

	component->Tq = 0;

	component->Td = 0;
	component->Ta = 0;

	return RET_SUCCESS;
}

struct htable {
	/* Table class – 0 = DC table or lossless table, 1 = AC table */
	uint8_t Tc;

	/* Number of Huffman codes of length i */
	uint8_t L[16];

	/*  Value associated with each Huffman code */
	uint8_t V[16][255];
};

int init_htable(struct htable *htable)
{
	assert(htable != NULL);

	htable->Tc = 0;

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

struct context {
	/* Specifies one of four possible destinations at the decoder into
	 * which the quantization table shall be installed */
	struct qtable qtable[4];

	/*  Sample precision */
	uint8_t precision;

	/* Number of lines, Number of samples per line */
	uint16_t Y, X;

	/* Number of image components in frame */
	uint8_t components;

	struct component component[256];

	struct htable htable[4];

	/* Restart interval */
	uint16_t Ri;
};

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

	for (int i = 0; i < 4; ++i) {
		init_htable(&context->htable[i]);
	}

	context->Ri = 0;

	return RET_SUCCESS;
}

/* zig-zag scan to raster scan */
const uint8_t zigzag[64] = {
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

/* B.2.4.1 Quantization table-specification syntax */
int parse_qtable(FILE *stream, struct context *context)
{
	int err;
	uint8_t Pq, Tq;
	struct qtable *qtable;

	assert(context != NULL);

	err = read_nibbles(stream, &Pq, &Tq);
	RETURN_IF(err);

	printf("Pq = %" PRIu8 " Tq = %" PRIu8 "\n", Pq, Tq);

	assert(Tq < 4);
	assert(Pq < 2);

	qtable = &context->qtable[Tq];

	qtable->precision = Pq;

	for (int i = 0; i < 64; ++i) {
		if (Pq == 0) {
			uint8_t byte;
			err = read_byte(stream, &byte);
			RETURN_IF(err);
			qtable->element[zigzag[i]] = (uint16_t)byte;
		} else {
			uint16_t word;
			err = read_word(stream, &word);
			RETURN_IF(err);
			qtable->element[zigzag[i]] = word;
		}
	}

	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			printf("%3" PRIu16 " ", qtable->element[y * 8 + x]);
		}
		printf("\n");
	}

	return RET_SUCCESS;
}

int parse_frame_header(FILE *stream, struct context *context)
{
	int err;
	/* Sample precision */
	uint8_t P;
	/* Number of lines, Number of samples per line */
	uint16_t Y, X;
	/* Number of image components in frame */
	uint8_t Nf;

	assert(context != NULL);

	err = read_byte(stream, &P);
	RETURN_IF(err);
	err = read_word(stream, &Y);
	RETURN_IF(err);
	err = read_word(stream, &X);
	RETURN_IF(err);
	err = read_byte(stream, &Nf);
	RETURN_IF(err);

	assert(P == 8);
	assert(X > 0);
	assert(Nf > 0);

	printf("P = %" PRIu8 " Y = %" PRIu16 " X = %" PRIu16 " Nf = %" PRIu8 "\n", P, Y, X, Nf);

	context->precision = P;
	context->Y = Y;
	context->X = X;
	context->components = Nf;

	for (int i = 0; i < Nf; ++i) {
		uint8_t C;
		uint8_t H, V;
		uint8_t Tq;

		err = read_byte(stream, &C);
		RETURN_IF(err);
		err = read_nibbles(stream, &H, &V);
		RETURN_IF(err);
		err = read_byte(stream, &Tq);
		RETURN_IF(err);

		printf("C = %" PRIu8 " H = %" PRIu8 " V = %" PRIu8 " Tq = %" PRIu8 "\n", C, H, V, Tq);

		context->component[C].H = H;
		context->component[C].V = V;
		context->component[C].Tq = Tq;
	}

	return RET_SUCCESS;
}

int parse_huffman_tables(FILE *stream, struct context *context)
{
	int err;
	uint8_t Tc, Th;

	assert(context != NULL);

	err = read_nibbles(stream, &Tc, &Th);
	RETURN_IF(err);

	printf("Tc = %" PRIu8 " Th = %" PRIu8 "\n", Tc, Th);

	struct htable *htable = &context->htable[Th];

	htable->Tc = Tc;

	for (int i = 0; i < 16; ++i) {
		err = read_byte(stream, &htable->L[i]);
		RETURN_IF(err);
	}

	for (int i = 0; i < 16; ++i) {
		uint8_t L = htable->L[i];

		for (int l = 0; l < L; ++l) {
			err = read_byte(stream, &htable->V[i][l]);
			RETURN_IF(err);
		}
	}

	return RET_SUCCESS;
}

int parse_scan_header(FILE *stream, struct context *context)
{
	int err;
	/* Number of image components in scan */
	uint8_t Ns;

	err = read_byte(stream, &Ns);
	RETURN_IF(err);

	printf("Ns = %" PRIu8 "\n", Ns);

	for (int j = 0; j < Ns; ++j) {
		uint8_t Cs;
		uint8_t Td, Ta;

		err = read_byte(stream, &Cs);
		RETURN_IF(err);
		err = read_nibbles(stream, &Td, &Ta);
		RETURN_IF(err);

		printf("Cs%i = %" PRIu8 " Td%i = %" PRIu8 " Ta%i = %" PRIu8 "\n", j, Cs, j, Td, j, Ta);

		context->component[Cs].Td = Td;
		context->component[Cs].Ta = Ta;
	}

	uint8_t Ss;
	uint8_t Se;
	uint8_t Ah, Al;

	err = read_byte(stream, &Ss);
	RETURN_IF(err);
	err = read_byte(stream, &Se);
	RETURN_IF(err);
	err = read_nibbles(stream, &Ah, &Al);
	RETURN_IF(err);

	assert(Ss == 0);
	assert(Se == 63);
	assert(Ah == 0);
	assert(Al == 0);

	return RET_SUCCESS;
}

int parse_restart_interval(FILE *stream, struct context *context)
{
	int err;
	uint16_t Ri;

	err = read_word(stream, &Ri);
	RETURN_IF(err);

	context->Ri = Ri;

	return RET_SUCCESS;
}

int parse_format(FILE *stream, struct context *context)
{
	int err;

	while (1) {
		uint16_t marker;

		err = read_marker(stream, &marker);
		RETURN_IF(err);

		/* An asterisk (*) indicates a marker which stands alone,
		 * that is, which is not the start of a marker segment. */
		switch (marker) {
			uint16_t len;
			long pos;
			/* SOI* Start of image */
			case 0xffd8:
				printf("SOI\n");
				break;
			/* APP0 */
			case 0xffe0:
				printf("APP0\n");
				err = read_length(stream, &len);
				RETURN_IF(err);
				err = skip_segment(stream, len);
				RETURN_IF(err);
				break;
			/* DQT Define quantization table(s) */
			case 0xffdb:
				printf("DQT\n");
				err = read_length(stream, &len);
				RETURN_IF(err);
				err = parse_qtable(stream, context);
				RETURN_IF(err);
				break;
			/* SOF0 Baseline DCT */
			case 0xffc0:
				printf("SOF0\n");
				err = read_length(stream, &len);
				RETURN_IF(err);
				err = parse_frame_header(stream, context);
				RETURN_IF(err);
				break;
			/* DHT Define Huffman table(s) */
			case 0xffc4:
				printf("DHT\n");
				pos = ftell(stream);
				err = read_length(stream, &len);
				RETURN_IF(err);
				/* parse multiple tables in single DHT */
				do {
					err = parse_huffman_tables(stream, context);
					RETURN_IF(err);
				} while (ftell(stream) < pos + len);
				break;
			/* SOS Start of scan */
			case 0xffda:
				printf("SOS\n");
				err = read_length(stream, &len);
				RETURN_IF(err);
				err = parse_scan_header(stream, context);
				RETURN_IF(err);
				break;
			/* EOI* End of image */
			case 0xffd9:
				printf("EOI\n");
				return RET_SUCCESS;
			/* DRI Define restart interval */
			case 0xffdd:
				printf("DRI\n");
				err = read_length(stream, &len);
				RETURN_IF(err);
				err = parse_restart_interval(stream, context);
				RETURN_IF(err);
				break;
			/* RSTm* Restart with modulo 8 count “m” */
			case 0xffd0:
			case 0xffd1:
			case 0xffd2:
			case 0xffd3:
			case 0xffd4:
			case 0xffd5:
			case 0xffd6:
			case 0xffd7:
				break;
			default:
				fprintf(stderr, "unhandled marker 0x%" PRIx16 "\n", marker);
				return RET_FAILURE_FILE_UNSUPPORTED;
		}
	}
}

int process_jpeg_stream(FILE *stream)
{
	int err;

	struct context *context = malloc(sizeof(struct context));

	if (context == NULL) {
		fprintf(stderr, "malloc failure\n");
		return RET_FAILURE_MEMORY_ALLOCATION;
	}

	err = init_context(context);

	if (err) {
		goto end;
	}

	err = parse_format(stream, context);
end:
	free(context);

	return err;
}

int process_jpeg_file(const char *path)
{
	FILE *stream = fopen(path, "r");

	if (stream == NULL) {
		fprintf(stderr, "fopen failure\n");
		return RET_FAILURE_FILE_OPEN;
	}

	int err = process_jpeg_stream(stream);

	fclose(stream);

	return err;
}

int main(int argc, char *argv[])
{
	const char *path = argc > 1 ? argv[1] : "Car3.jpg";

	process_jpeg_file(path);

	return 0;
}
