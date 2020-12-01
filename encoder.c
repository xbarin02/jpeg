#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "common.h"
#include "frame.h"
#include "coeffs.h"
#include "imgproc.h"

int process_stream(FILE *stream)
{
	int err;

	struct frame frame;

	// load PPM/PGM header, detect X, Y, number of components, bpp
	err = read_frame_header(&frame, stream);
	RETURN_IF(err);

	printf("[DEBUG] header Nf=%" PRIu8 " Y=%" PRIu16 " X=%" PRIu16 " P=%" PRIu8 "\n", frame.components, frame.Y, frame.X, frame.precision);

	struct context *context = malloc(sizeof(struct context));

	err = init_context(context);
	RETURN_IF(err);

	context->Nf = frame.components;
	context->Y = frame.Y;
	context->X = frame.X;
	context->P = frame.precision;

	switch (frame.components) {
		case 1:
			context->component[1].H = 1;
			context->component[1].V = 1;
			context->component[1].Tq = 0;
			context->max_H = 1;
			context->max_V = 1;
			break;
		case 3:
			context->component[1].H = 2;
			context->component[1].V = 2;
			context->component[1].Tq = 0;
			context->component[2].H = 1;
			context->component[2].V = 1;
			context->component[1].Tq = 1;
			context->component[3].H = 1;
			context->component[3].V = 1;
			context->component[1].Tq = 1;
			context->max_H = 2;
			context->max_V = 2;
			break;
		default:
			return RET_FAILURE_FILE_UNSUPPORTED;
	}

	err = frame_create_empty(context, &frame);
	RETURN_IF(err);

	// load frame body
	err = read_frame_body(&frame, stream);
	RETURN_IF(err);

	printf("[DEBUG] frame data loaded\n");

	err = compute_no_blocks_and_alloc_buffers(context);
	RETURN_IF(err);

	err = frame_to_ycc(&frame);
	RETURN_IF(err);

	// copy frame->data[] into context->component[]->frame_buffer[]
	transform_frame_to_components(context, &frame);

	frame_destroy(&frame);

	err = conv_frame_to_blocks(context);
	RETURN_IF(err);

	err = forward_dct(context);
	RETURN_IF(err);

	err = quantize(context);
	RETURN_IF(err);

	// TODO
#if 0
	err = dequantize(context);
	RETURN_IF(err);
	err = inverse_dct(context);
	RETURN_IF(err);
	err = conv_blocks_to_frame(context);
	RETURN_IF(err);
	err = write_image(context, NULL);
	RETURN_IF(err);
#endif

	free_buffers(context);

	free(context);

	return RET_SUCCESS;
}

int main(int argc, char *argv[])
{
	const char *i_path = argc > 1 ? argv[1] : "Lenna.jpg";

	FILE *stream = fopen(i_path, "r");

	if (stream == NULL) {
		fprintf(stderr, "fopen failure\n");
		return 1;
	}

	int err = process_stream(stream);

	if (err) {
		fprintf(stderr, "Failure.\n");
	}

	fclose(stream);

	return 0;
}
