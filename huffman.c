#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "huffman.h"
#include "io.h"
#include "common.h"

int init_vlc(struct vlc *vlc)
{
	assert(vlc != NULL);

	vlc->code = 0;
	vlc->size = 0;

	return RET_SUCCESS;
}

int vlc_add_bit(struct vlc *vlc, uint16_t bit)
{
	assert(vlc != NULL);

	vlc->code <<= 1;
	vlc->code |= bit & 1;
	vlc->size++;

	return RET_SUCCESS;
}

/* Figure C.1 – Generation of table of Huffman code sizes */
int generate_size_table(struct htable *htable, struct hcode *hcode)
{
	assert(htable != NULL);
	assert(hcode != NULL);

#define BITS(I)     (htable->L[(I) - 1])
#define HUFFSIZE(K) (hcode->huff_size[(K)])
#define LASTK       (hcode->last_k)

	size_t K = 0;
	size_t I = 1;
	size_t J = 1;

	do {
		while (J <= BITS(I)) {
			assert(K < 256);
			HUFFSIZE(K) = I;
			K++;
			J++;
		}
		I++;
		J = 1;
	} while (I <= 16);
	assert(K < 256);
	HUFFSIZE(K) = 0;
	LASTK = K;

#undef BITS
#undef HUFFSIZE
#undef LASTK

// 	printf("[DEBUG] last_k = %zu\n", hcode->last_k);

	return RET_SUCCESS;
}

/* Figure C.2 – Generation of table of Huffman codes */
int generate_code_table(struct htable *htable, struct hcode *hcode)
{
	assert(htable != NULL);
	assert(hcode != NULL);

#define HUFFSIZE(K) (hcode->huff_size[(K)])
#define HUFFCODE(K) (hcode->huff_code[(K)])

	size_t K = 0;
	uint16_t CODE = 0;
	size_t SI = HUFFSIZE(0);

	do {
		do {
			assert(K < 256);
			HUFFCODE(K) = CODE;
			CODE++;
			K++;
			assert(K < 256);
		} while (HUFFSIZE(K) == SI);

		assert(K < 256);
		if (HUFFSIZE(K) == 0) {
			return RET_SUCCESS;
		}

		do {
			CODE <<= 1;
			SI++;
			assert(K < 256);
		} while (HUFFSIZE(K) != SI);
	} while (1);

#undef HUFFSIZE
#undef HUFFCODE
}

/* Figure C.3 – Ordering procedure for encoding procedure code tables */
int order_codes(struct htable *htable, struct hcode *hcode)
{
	assert(htable != NULL);
	assert(hcode != NULL);

#define HUFFVAL(K)  (hcode->huff_val[(K)])
#define EHUFCO(I)   (hcode->e_huf_co[(I)])
#define EHUFSI(I)   (hcode->e_huf_si[(I)])
#define LASTK       (hcode->last_k)
#define HUFFSIZE(K) (hcode->huff_size[(K)])
#define HUFFCODE(K) (hcode->huff_code[(K)])

	size_t K = 0;

	do {
		uint8_t I = HUFFVAL(K);
		EHUFCO(I) = HUFFCODE(K);
		EHUFSI(I) = HUFFSIZE(K);
// 		printf("[DEBUG] value=%i cat=%i size=%zu code=%" PRIu16 "\n", I, I & 15, EHUFSI(I), EHUFCO(I));
		K++;
	} while (K < LASTK);

#undef HUFFVAL
#undef EHUFCO
#undef EHUFSI
#undef LASTK
#undef HUFFSIZE
#undef HUFFCODE

	return RET_SUCCESS;
}

int conv_htable_to_hcode(struct htable *htable, struct hcode *hcode)
{
	int err;

	assert(htable != NULL);
	assert(hcode != NULL);

	uint8_t *v = hcode->huff_val;

	for (int i = 0; i < 16; ++i) {
		uint8_t L = htable->L[i];

		for (int l = 0; l < L; ++l) {
			*v = htable->V[i][l];
			v++;
		}
	}

	err = generate_size_table(htable, hcode);
	RETURN_IF(err);

	err = generate_code_table(htable, hcode);
	RETURN_IF(err);

	err = order_codes(htable, hcode);
	RETURN_IF(err);

	return RET_SUCCESS;
}

/*
 * query if the code is present in htable/hcode, and return its value
 *
 * Usage:
 *
 * do {
 *     next_bit(&bits, &bit); // read next bit
 *     vlc_add_bit(vlc, bit); // add this bit to VLC
 * } while (query_code(vlc, htable, hcode, value) == -1); // query Huffman table
 *
 * // value ... category code
 * // read extra bits
 */
int query_code(struct vlc *vlc, struct hcode *hcode, uint8_t *value)
{
	assert(vlc != NULL);
	assert(hcode != NULL);
	assert(value != NULL);

#define HUFFVAL(K)  (hcode->huff_val[(K)])
#define LASTK       (hcode->last_k)
#define HUFFSIZE(K) (hcode->huff_size[(K)])
#define HUFFCODE(K) (hcode->huff_code[(K)])

	size_t K = 0;

	do {
		uint16_t code = HUFFCODE(K);
		size_t size = HUFFSIZE(K);

		if (vlc->size == size && vlc->code == code) {
			uint8_t I = HUFFVAL(K);
			*value = I;
			return RET_SUCCESS;
		}

		K++;
	} while (K < LASTK);

#undef HUFFVAL
#undef LASTK
#undef HUFFSIZE
#undef HUFFCODE

	return -1; /* not found */
}

int read_code(struct bits *bits, struct hcode *hcode, uint8_t *value)
{
	int err;
	struct vlc vlc;

	init_vlc(&vlc);

	do {
		uint8_t bit;
		err = next_bit(bits, &bit);
		RETURN_IF(err);
		vlc_add_bit(&vlc, (uint16_t)bit); // add this bit to VLC
	} while (query_code(&vlc, hcode, value) == -1); // query Huffman table

	return RET_SUCCESS;
}

int read_extra_bits(struct bits *bits, uint8_t count, uint16_t *value)
{
	int err;
	uint16_t v = 0;

	for (int i = 0; i < count; ++i) {
		uint8_t bit;
		err = next_bit(bits, &bit);
		RETURN_IF(err);
		v <<= 1;
		v |= bit & 1;
	}

	assert(value != NULL);

	*value = v;

	return RET_SUCCESS;
}
