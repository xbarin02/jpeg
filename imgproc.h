#ifndef JPEG_IMGPROC_H
#define JPEG_IMGPROC_H

#include "common.h"

/* for each component: remove differential DC encoding & quantization */
int dequantize(struct context *context);

#endif
