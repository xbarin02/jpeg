#include <assert.h>
#include "bits.h"
#include "io.h"
#include "common.h"

int init_bits(struct bits *bits, FILE *stream)
{
	assert(bits != NULL);

	bits->count = 0;
	bits->stream = stream;

	return RET_SUCCESS;
}

/* F.2.2.5 The NEXTBIT procedure
 * Figure F.18 – Procedure for fetching the next bit of compressed data */
int next_bit(struct bits *bits, uint8_t *bit)
{
	int err;

	assert(bits != NULL);

	if (bits->count == 0) {
		/* refill bits->byte */
		err = read_ecs_byte(bits->stream, &bits->byte);
		RETURN_IF(err); /* incl. RET_FAILURE_NO_MORE_DATA */

		bits->count = 8;
	}

	assert(bit != NULL);

	/* output MSB */
	*bit = bits->byte >> 7;

	bits->byte <<= 1;
	bits->count--;

	return RET_SUCCESS;
}
