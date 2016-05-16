/*
 * Copyright 2010 - 2016 Paul Chote
 * This file is part of pixelshift, which is free software. It is made available to
 * you under the terms of version 3 or later of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fitsio2.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <stdarg.h>
#include "framedata.h"
#include "fit.h"

int main(int argc, char *argv[])
{
	int ret = 1;

	if (argc != 4)
	{
		printf("usage: backgroundsubtract in.fit out.fit mintilesize\n\n");
		printf("Subtracts low-order sky variations from a frame.\n");
		printf("mintilesize specifies the minimum size (in px) of the background sky tiles. This should be much larger than the FWHM of stars in the field.\n");

		return ret;
	}

	framedata *frame = framedata_load(argv[1]);
	if (!frame)
	{
		fprintf(stderr, "Error loading frame %s\n", argv[1]);
		goto frame_error;
	}

	uint16_t tile_size = atoi(argv[3]);
	if (framedata_subtract_background(frame, tile_size))
	{
		fprintf(stderr, "Error background subtracting %s\n", argv[1]);
		goto process_error;
	}
	
	framedata_save(frame, argv[2]);
	ret = 0;

process_error:
	framedata_free(frame);
frame_error:
	return ret;
}
