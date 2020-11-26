#ifndef JPEG_IMGPROC_H
#define JPEG_IMGPROC_H

#include "common.h"

/* for each component: remove quantization */
int dequantize(struct context *context);

int invert_dct(struct context *context);

int conv_blocks_to_frame(struct context *context);

int dump_components(struct context *context);

#endif
