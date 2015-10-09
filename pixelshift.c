/*
 * Copyright 2010 - 2015 Paul Chote
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

static double mean_exclude_sigma(double *data, size_t n, double sigma)
{
	double mean = 0;
	for (size_t i = 0; i < n; i++)
		mean += data[i];
	mean /= n;

	double std = 0;
	for (size_t i = 0; i < n; i++)
		std += (data[i] - mean)*(data[i] - mean);
	std = sqrt(std / n);

	double total = 0;
	size_t count = 0;
	for (size_t i = 0; i < n; i++)
	{
		if (fabs(data[i] - mean) < sigma*std)
		{
			total += data[i];
			count++;
		}
	}

	return total / count;
}

static void sum_into_axes(framedata *frame, double *x, double *y)
{
	// Sum into x and y axes
	for (uint16_t j = 0; j < frame->height; j++)
	{
		for (uint16_t i = 0; i < frame->width; i++)
		{
			double px = frame->data[frame->width*j + i];
			x[i] += px;
			y[j] += px;
		}
	}

	// Subtract the mean background level then clamp to zero to
	// discard the majority of the sky background
	double xm = mean_exclude_sigma(x, frame->width, 1);
	for (uint16_t i = 0; i < frame->width; i++)
		x[i] = fmax(x[i] - xm, 0);

	double ym = mean_exclude_sigma(y, frame->height, 1);
	for (uint16_t j = 0; j < frame->height; j++)
		y[j] = fmax(y[j] - ym, 0);
}

static double find_max_correlation(double *a, double *b, uint16_t n)
{
	int32_t best_idx = 0;
	double best = -1;
	for (int32_t j = -n; j < n; j++)
	{
		double ret = 0;
		for (uint16_t i = 0; i < n; i++)
		{
			// Assume data outside the array bounds are zero
			uint16_t k = i + j;
			ret += (k >= 0 && k < n) ? a[i]*b[k] : 0;
		}

		if (ret > best)
		{
			best = ret;
			best_idx = j;
		}
	}

	// Best fit is at the edge of the correlation.
	// Can't improve our estimate!
	if (best_idx == -n || best_idx == n - 1)
		return best_idx;

	// Improve estimate by doing a quadratic fit of
	// the three points around the peak
	double offset[3] = { -1, 0, 1 };
	double corr[3] = { 0, 1, 0 };
	double p[3] = { 0, 0, 0 };
	for (uint16_t i = 0; i < n; i++)
	{
		uint16_t k1 = i + best_idx - 1;
		corr[0] += (k1 > 0 && k1 < n) ? a[i]*b[k1] : 0;

		uint16_t k2 = i + best_idx + 1;
		corr[2] += (k2 > 0 && k2 < n) ? a[i]*b[k2] : 0;
	}

	corr[0] /= best;
	corr[2] /= best;

	if (fit_polynomial(offset, corr, NULL, 3, p, 2))
		return best_idx;

	return best_idx - p[1] / (2 * p[2]);
}

int main(int argc, char *argv[])
{
	int ret = 1;

	if (argc != 4)
	{
		printf("usage: pixelshift frame.fit[x1:x2,y1:y2] reference.fit[x1:x2,y1:y2] mintilesize\n\n");
		printf("Calculates the sub-pixel x,y shift between two frames.\n");
		printf("mintilesize specifies the minimum size (in px) of the background sky tiles. This should be much larger than the FWHM of stars in the field.\n");

		return ret;
	}

	framedata *frame = framedata_load(argv[1]);
	if (!frame)
	{
		fprintf(stderr, "Error loading frame %s\n", argv[1]);
		goto frame_error;
	}

	framedata *reference = framedata_load(argv[2]);
	if (!reference)
	{
		fprintf(stderr, "Error loading frame %s\n", argv[2]);
		goto reference_error;
	}

	if (frame->width != reference->width || frame->height != reference->height)
	{
		fprintf(stderr, "Frame sizes [%d,%d], [%d,%d] don't match.\n", frame->width, frame->height, reference->width, reference->height);
		goto process_error;
	}

	uint16_t tile_size = atoi(argv[3]);
	if (framedata_subtract_background(frame, tile_size))
	{
		fprintf(stderr, "Error background subtracting %s\n", argv[1]);
		goto process_error;
	}

	if (framedata_subtract_background(reference, tile_size))
	{
		fprintf(stderr, "Error background subtracting %s\n", argv[2]);
		goto process_error;
	}

	double *fx = calloc(frame->width, sizeof(double));
	double *fy = calloc(frame->height, sizeof(double));
	double *rx = calloc(frame->width, sizeof(double));
	double *ry = calloc(frame->height, sizeof(double));
	if (!fx || !fy || !rx || !ry)
	{
		fprintf(stderr, "Error allocating memory for frame correlation\n");
		goto correlation_error;
	}

	sum_into_axes(frame, fx, fy);
	sum_into_axes(reference, rx, ry);

	double xt = find_max_correlation(rx, fx, reference->width);
	double yt = find_max_correlation(ry, fy, reference->height);

	printf("%.2f %.2f\n", xt, yt);
	ret = 0;

correlation_error:
	free(fx);
	free(fy);
	free(rx);
	free(ry);
process_error:
	framedata_free(reference);
reference_error:
	framedata_free(frame);
frame_error:
	return ret;
}
