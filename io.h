#ifndef JPEG_IO_H
#define JPEG_IO_H

#include <stdio.h>
#include <stdint.h>

int read_nibbles(FILE *stream, uint8_t *first, uint8_t *second);

int read_byte(FILE *stream, uint8_t *byte);

int read_word(FILE *stream, uint16_t *word);

int read_length(FILE *stream, uint16_t *len);

int skip_segment(FILE *stream, uint16_t len);

int read_marker(FILE *stream, uint16_t *marker);

#endif
