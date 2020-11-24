#ifndef JPEG_HUFFMAN_H
#define JPEG_HUFFMAN_H

#include "common.h"
#include "vlc.h"
#include "bits.h"

int generate_size_table(struct htable *htable, struct hcode *hcode);

int generate_code_table(struct htable *htable, struct hcode *hcode);

int order_codes(struct htable *htable, struct hcode *hcode);

int conv_htable_to_hcode(struct htable *htable, struct hcode *hcode);

/*
 * query if the code is present in htable/hcode, and return its value
 */
int query_code(struct vlc *vlc, struct htable *htable, struct hcode *hcode, uint8_t *value);

int read_code(struct bits *bits, struct htable *htable, struct hcode *hcode, uint8_t *value);

int read_extra_bits(struct bits *bits, uint8_t count, uint16_t *value);

#endif
