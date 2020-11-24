#ifndef JPEG_HUFFMAN_H
#define JPEG_HUFFMAN_H

#include "common.h"
#include "vlc.h"

int generate_size_table(struct htable *htable, struct hcode *hcode);

int generate_code_table(struct htable *htable, struct hcode *hcode);

int order_codes(struct htable *htable, struct hcode *hcode);

int conv_htable_to_hcode(struct htable *htable, struct hcode *hcode);

/*
 * query if the code is present in htable/hcode, and return its value
 */
int query_code(struct vlc *vlc, struct htable *htable, struct hcode *hcode, uint8_t *value);

#endif
