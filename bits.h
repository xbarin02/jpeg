#ifndef JPEG_BITS_H
#define JPEG_BITS_H

#include <stdint.h>
#include <stdio.h>

struct bits {
	uint8_t byte;
	size_t count;
	FILE *stream;
};

int init_bits(struct bits *bits, FILE *stream);

/* F.2.2.5 The NEXTBIT procedure */
int next_bit(struct bits *bits, uint8_t *bit);

#endif
