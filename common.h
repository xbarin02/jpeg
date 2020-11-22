#ifndef JPEG_COMMON_H
#define JPEG_COMMON_H

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

#endif
