#ifndef JPEG_COEFFS_H
#define JPEG_COEFFS_H

#include <stdint.h>
#include "common.h"
#include "bits.h"

struct block {
	int32_t c[64];
};

int read_block(struct bits *bits, struct context *context, uint8_t Cs, struct block *block);

#endif
