#ifndef JPEG_COMMON_H
#define JPEG_COMMON_H

#include <stdint.h>

/**
 * \brief Error codes
 *
 * The standard (C89, 6.1.3.3 Enumeration constants) states that
 * an identifier declared as an enumeration constant has type \c int.
 * Therefore, it is fine if the function returning these constants
 * has return type \c int.
 */
enum {
	/* 0x0000 successful completion */
	RET_SUCCESS                   = 0x0000, /**< success */
	/* 0x1xxx input/output errors */
	RET_FAILURE_FILE_IO           = 0x1000, /**< I/O error */
	RET_FAILURE_FILE_UNSUPPORTED  = 0x1001, /**< unsupported feature or file type */
	RET_FAILURE_FILE_OPEN         = 0x1002, /**< file open failure */
	RET_FAILURE_FILE_SEEK         = 0x1003,
	/* 0x2xxx memory errors */
	RET_FAILURE_MEMORY_ALLOCATION = 0x2000, /**< unable to allocate dynamic memory */
	/* 0x3xxx general exceptions */
	RET_FAILURE_LOGIC_ERROR       = 0x3000, /**< faulty logic within the program */
	RET_FAILURE_OVERFLOW_ERROR    = 0x3001, /**< result is too large for the destination type */
	/* 0x4xxx other */
	RET_FAILURE_NO_MORE_DATA      = 0x4000,
	RET_LAST
};

#define RETURN_IF(err) \
	do { \
		if (err) { \
			return (err); \
		} \
	} while (0)

struct qtable {
	/* Value 0 indicates 8-bit Qk values; value 1 indicates 16-bit Qk values. */
	uint8_t precision;
	/* in zig-zag scan order */
	uint16_t element[64];
};

struct component {
	/* Horizontal sampling factor, Vertical sampling factor */
	uint8_t H, V;
	/* Quantization table destination selector */
	uint8_t Tq;

	/* DC entropy coding table destination selector
	 * AC entropy coding table destination selector */
	uint8_t Td, Ta;
};

struct htable {
	/* Table class – 0 = DC table or lossless table, 1 = AC table */
	uint8_t Tc;

	/* Number of Huffman codes of length i */
	uint8_t L[16];

	/*  Value associated with each Huffman code */
	uint8_t V[16][255];

	uint8_t V_[16 * 255];
};

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

int init_qtable(struct qtable *qtable);

int init_component(struct component *component);

int init_htable(struct htable *htable);

int init_context(struct context *context);

#endif
