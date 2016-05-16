/*
 * Copyright 2010 - 2015 Paul Chote
 * This file is part of pixelshift, which is free software. It is made available to
 * you under the terms of version 3 or later of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fitsio2.h>
#include <math.h>
#include <inttypes.h>
#include "framedata.h"

framedata *framedata_load(const char *filename)
{
	int status = 0;
	fitsfile *input;

	framedata *frame = malloc(sizeof(framedata));
	if (!frame)
		return NULL;

	if (fits_open_image(&input, filename, READONLY, &status))
	{
		fprintf(stderr, "fits_open_image failed with status %d for %s\n", status, filename);
		goto error;
	}

	// Query the image size
	fits_read_key(input, TINT, "NAXIS1", &frame->width, NULL, &status);
	fits_read_key(input, TINT, "NAXIS2", &frame->height, NULL, &status);
	if (status)
	{
		fprintf(stderr, "querying NAXIS failed with status %d for %s\n", status, filename);
		goto error;
	}

	frame->data = malloc(frame->width*frame->height*sizeof(double));
	if (!frame->data)
	{
		fprintf(stderr, "Failed allocating memory for frame data for %s\n", filename);
		goto error;
	}

	if (fits_read_pix(input, TDOUBLE, (long []){1, 1}, frame->width*frame->height, 0, frame->data, NULL, &status))
	{
		fprintf(stderr, "fits_read_pix failed with status %d for %s\n", status, filename);
		goto error;
	}

	frame->image_x = 0;
	frame->image_y = 0;
	frame->image_width = frame->width;
	frame->image_height = frame->height;

	char region[FLEN_VALUE];
	fits_read_key(input, TSTRING, "IMAG-RGN", region, NULL, &status);
	if (!status)
	{
		uint16_t x1, x2, y1, y2;
		if (sscanf(region, "[%hu:%hu,%hu:%hu]", &x1, &x2, &y1, &y2) == 4)
		{
			frame->image_x = x1 - 1;
			frame->image_y = y1 - 1;
			frame->image_width = x2 - x1;
			frame->image_height = y2 - y1;
		}
	}

	fits_close_file(input, &status);
	return frame;

error:
	fits_close_file(input, &status);
	framedata_free(frame);
	return NULL;
}


int framedata_save(framedata *fd, const char *path)
{
    fitsfile *out;
    int status = 0;

    // Prepending path with a '!' tells cfitsio to
    // overwrite any existing file.
    size_t filepath_len = strlen(path) + 2;
    char *filepath = malloc(filepath_len*sizeof(char));
    snprintf(filepath, filepath_len, "!%s", path);

    fits_create_file(&out, filepath, &status);
    free(filepath);

    // Create the primary array image (16-bit short integer pixels
    fits_create_img(out, DOUBLE_IMG, 2, (long []){fd->width, fd->height}, &status);

    // Write the frame data to the image
    if (fits_write_img(out, TDOUBLE, 1, fd->width*fd->height, fd->data, &status))
	fprintf(stderr, "fits_write_img failed with status %d\n", status);

    fits_close_file(out, &status);
    return 0;
}

void framedata_free(framedata *frame)
{
	if (!frame)
		return;

	free(frame->data);
	free(frame);
}

static double cubic_interpolate(double* p, double x)
{
	return p[1] + 0.5f*x*(p[2] - p[0] + x*(2*p[0] - 5*p[1] + 4*p[2] - p[3] + x*(3*(p[1] - p[2]) + p[3] - p[0])));
}

static double bicubic_interpolate(double p[16], double x, double y)
{
	// Interpolate along the x axis
	double py[4] = {
		cubic_interpolate(&p[0], x),
		cubic_interpolate(&p[4], x),
		cubic_interpolate(&p[8], x),
		cubic_interpolate(&p[12], x)
	};

	// Interpolate the interpolated control points along the y axis
	return cubic_interpolate(py, y);
}

static int compare_double(const void *a, const void *b)
{
	double da = *(const double *)a;
	double db = *(const double *)b;

	return (da > db) - (da < db);
}

#define clamp(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

int framedata_subtract_background(framedata *frame, uint16_t min_tile_size)
{
	// Extend tile width/height by a few px if necessary to
	// reduce the margin of unused pixels on the right/top edge
	uint16_t x_tile_count = frame->image_width / min_tile_size;
	uint16_t y_tile_count = frame->image_height / min_tile_size;
	uint16_t tile_width = frame->image_width / x_tile_count;
	uint16_t tile_height = frame->image_height / y_tile_count;

	// Extra margin accounts for the edge tiles that are extended to fill the remaining gap
	double *tile_buf = calloc(tile_width * tile_height, sizeof(double));
	double *tile_median = calloc(x_tile_count * y_tile_count, sizeof(double));

	if (!tile_buf || !tile_median)
	{
		free(tile_buf);
		free(tile_median);
		fprintf(stderr, "Error allocating memory for background map\n");
		return 1;
	}

	// Calculate tile medians
	for (uint16_t ty = 0; ty < y_tile_count; ty++)
	{
		for (uint16_t tx = 0; tx < x_tile_count; tx++)
		{
			uint16_t x_start = tx * tile_width + frame->image_x;
			uint16_t y_start = ty * tile_height + frame->image_y;

			size_t i = 0;
			for (uint16_t dy = 0; dy < tile_height; dy++)
				for (uint16_t dx = 0; dx < tile_width; dx++)
					tile_buf[i++] = frame->data[(y_start + dy) * frame->width + x_start + dx];

			qsort(tile_buf, tile_width * tile_height, sizeof(double), compare_double);

			tile_median[ty * x_tile_count + tx] = tile_buf[tile_width * tile_height / 2];
		}
	}

	// Bicubic interpolate between the tiles to generate the background map
	// The coarse points are in the middle of the tiles, so we must enumerate
	// over an extra set of tiles on each border in order to evaluate the outer
	// half tile edges of the image. The pixels outside the image are filtered
	// when interpolating
	for (int32_t ty = -1; ty < y_tile_count; ty++)
	{
		for (int32_t tx = -1; tx < x_tile_count; tx++)
		{
			// Calculate the 16 control points for interpolating over this tile
			double p[16];
			for (uint8_t j = 0; j < 4; j++)
				for (uint8_t i = 0; i < 4; i++)
					p[j * 4 + i] = tile_median[clamp(ty - 1 + j, 0, y_tile_count - 1) * x_tile_count + clamp(tx - 1 + i, 0, x_tile_count - 1)];

			// Interpolate over all the pixels inside the image area inside this tile
			for (uint16_t dy = 0; dy < tile_height; dy++)
			{
				int32_t y = ty * tile_height + tile_height / 2 + dy + frame->image_y;
				if (y < frame->image_y || y >= frame->image_height + frame->image_y)
					continue;

				for (uint16_t dx = 0; dx < tile_width; dx++)
				{
					int32_t x = tx * tile_width + tile_width / 2 + dx + frame->image_x;
					if (x < frame->image_x || x >= frame->image_width + frame->image_x)
						continue;

					double bg = (uint16_t)bicubic_interpolate(p, dx * 1.0 / tile_width, dy * 1.0 / tile_height);
					frame->data[y * frame->width + x] -= bg;
				}
			}
		}
	}

	// Zero out any pixels outside the image region
	if (frame->image_width != frame->width || frame->image_height != frame->height)
	{
		for (int32_t y = 0; y < frame->height; y++)
			for (int32_t x = 0; x < frame->width; x++)
				if (y < frame->image_y || y >= frame->image_y + frame->image_height || x < frame->image_x || x >= frame->image_x + frame->image_width)
					frame->data[y * frame->width + x] = 0;
	}

	free(tile_buf);
	free(tile_median);
	return 0;
}
