#include "common.h"
#include "frame.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>

int read_image(struct frame *frame, FILE *stream)
{
	int err;

	struct context context;

	err = init_context(&context);
	RETURN_IF(err);

	err = read_frame_header(frame, stream);
	RETURN_IF(err);

	context.Nf = frame->components;
	context.Y = frame->Y;
	context.X = frame->X;
	context.P = frame->precision;
	context.max_H = 1;
	context.max_V = 1;

	err = frame_create_empty(&context, frame);
	RETURN_IF(err);

	err = read_frame_body(frame, stream);
	RETURN_IF(err);

	return RET_SUCCESS;
}

int compare_frames(struct frame *i_frame, struct frame *j_frame)
{
	assert(i_frame != NULL);
	assert(j_frame != NULL);

	if (i_frame->X != j_frame->X || i_frame->Y != j_frame->Y || i_frame->components != j_frame->components || i_frame->precision != j_frame->precision) {
		return -1;
	}

	uint8_t Nf = i_frame->components;
	size_t X = i_frame->X;
	size_t Y = i_frame->Y;

	float mse = 0.f;

	for (size_t c = 0; c < Nf; ++c) {
		for (size_t y = 0; y < Y; ++y) {
			for (size_t x = 0; x < X; ++x) {
				float i_px = i_frame->data[y * i_frame->size_x * Nf + x * Nf + c];
				float j_px = j_frame->data[y * j_frame->size_x * Nf + x * Nf + c];

				float e = i_px - j_px;
				float se = e * e;
				mse += se;
			}
		}
	}

	mse /= Nf * Y * X;

	printf("MSE = %f\n", mse);

	int maxval = (1 << i_frame->precision) - 1;

	float psnr = 10 * log10f(maxval * maxval / mse);

	printf("PSNR = %f\n", psnr);

	// block size
	size_t B = 8;

	assert(X % B == 0 && Y % B == 0);

	float D_B = 0.f; // Eq. (19)
	size_t N_B = 0;

	float D_CB = 0.f; // Eq. (20)
	size_t N_CB = 0;

	for (size_t c = 0; c < Nf; ++c) {
		// for each block except the last one
		for (size_t b_y = 0; b_y < Y / B - 1; ++b_y) {
			for (size_t b_x = 0; b_x < X / B - 1; ++b_x) {
#define P(y,x) (j_frame->data[(y) * j_frame->size_x * Nf + (x) * Nf + c])
				// horizontal
				for (size_t yy = 0; yy < 8; ++yy) {
					float e = P(8 * b_y + yy, 8 * b_x + 7) - P(8 * b_y + yy, 8 * b_x + 8);
					D_B += e * e;
					N_B++;
				}
				// vertical
				for (size_t xx = 0; xx < 8; ++xx) {
					float e = P(8 * b_y + 7, 8 * b_x + xx) - P(8 * b_y + 8, 8 * b_x + xx);
					D_B += e * e;
					N_B++;
				}

				// horizontal
				for (size_t yy = 0; yy < 8; ++yy) {
					for (size_t xx = 0; xx < 7; ++xx) {
						float e = P(8 * b_y + yy, 8 * b_x + xx) - P(8 * b_y + yy, 8 * b_x + xx + 1);
						D_CB += e * e;
						N_CB++;
					}
				}
				// vertical
				for (size_t yy = 0; yy < 7; ++yy) {
					for (size_t xx = 0; xx < 8; ++xx) {
						float e = P(8 * b_y + yy, 8 * b_x + xx) - P(8 * b_y + yy + 1, 8 * b_x + xx);
						D_CB += e * e;
						N_CB++;
					}
				}
#undef P
			}
		}
	}

	D_B /= N_B;
	D_CB /= N_CB;

	float eta = 0.f;
	if (D_B > D_CB) {
#define MIN(a,b) ((a) < (b) ? (a) : (b))
		eta = 3.f / log2f(MIN(X, Y));
#undef MIN
	}

	float BEF = eta * (D_B - D_CB);

	printf("BEF = %f\n", BEF);

	float mse_b = mse + BEF;

	printf("MSE-B = %f\n", mse_b);

	float psnr_b = 10 * log10f(maxval * maxval / mse_b);

	printf("PSNR-B = %f\n", psnr_b);

	return RET_SUCCESS;
}

int compare(FILE *i_stream, FILE *j_stream)
{
	int err;

	struct frame i_frame;
	struct frame j_frame;

	err = read_image(&i_frame, i_stream);
	RETURN_IF(err);
	err = read_image(&j_frame, j_stream);
	RETURN_IF(err);

	err = compare_frames(&i_frame, &j_frame);
	RETURN_IF(err);

	frame_destroy(&i_frame);
	frame_destroy(&j_frame);

	return RET_SUCCESS;
}

int main(int argc, char *argv[])
{
	const char *i_path = 1 < argc ? argv[1] : "Lenna.ppm";
	const char *j_path = 2 < argc ? argv[2] : "output.ppm";

	FILE *i_stream = fopen(i_path, "r");
	FILE *j_stream = fopen(j_path, "r");

	if (i_stream == NULL) {
		fprintf(stderr, "fopen failure\n");
		return 1;
	}

	if (j_stream == NULL) {
		fprintf(stderr, "fopen failure\n");
		return 1;
	}

	int err = compare(i_stream, j_stream);

	fclose(i_stream);
	fclose(j_stream);

	if (err) {
		printf("Failure.\n");
	}

	return err;
}
