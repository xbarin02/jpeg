#ifndef JPEG_HUFFMAN_H
#define JPEG_HUFFMAN_H

#include "common.h"

int generate_size_table(struct htable *htable, struct hcode *hcode);

int generate_code_table(struct htable *htable, struct hcode *hcode);

int order_codes(struct htable *htable, struct hcode *hcode);

int conv_htable_to_hcode(struct htable *htable, struct hcode *hcode);

#endif
