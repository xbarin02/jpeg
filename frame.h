#ifndef JPEG_FRAME_H
#define JPEG_FRAME_H

#include <stddef.h>
#include <stdint.h>
#include "common.h"

struct frame {
	uint8_t components;
	uint16_t Y, X;
	size_t size_x, size_y;
	uint8_t precision;

	float *data;
};

int frame_create(struct context *context, struct frame *frame);

void frame_destroy(struct frame *frame);

int frame_to_rgb(struct frame *frame);

int dump_frame(struct frame *frame);

#endif
