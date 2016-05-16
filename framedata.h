/*
 * Copyright 2010 - 2015 Paul Chote
 * This file is part of pixelshift, which is free software. It is made available to
 * you under the terms of version 3 or later of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#ifndef FRAMEDATA_H
#define FRAMEDATA_H

#include <stdint.h>

typedef struct
{
	double *data;
	uint16_t width;
	uint16_t height;

	// Image region (ignoring overscan)
	uint16_t image_x;
	uint16_t image_y;
	uint16_t image_width;
	uint16_t image_height;
} framedata;

framedata *framedata_load(const char *filename);
void framedata_free(framedata *frame);
int framedata_subtract_background(framedata *frame, uint16_t min_tile_size);

#endif
