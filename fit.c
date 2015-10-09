/*
 * Copyright 2010 - 2015 Paul Chote
 * This file is part of pixelshift, which is free software. It is made available to
 * you under the terms of version 3 or later of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

// Calculates the reduced row echelon form using Gauss-Jordan elimination of a matrix A.
// Returns non-zero on error
static int rref(double *A, size_t rows, size_t cols)
{
	double eps = 1e-10; // Limit for singular values

	// Don't reduce into a diagonal form directly; pick the column with the largest
	// value as the pivot for each row and keep track of the row for each column
	// so we can quickly reorder at the end of the process.
	// This reduces numerical problems while avoiding the need to swap columns.
	int *prows = malloc(rows*sizeof(int));
	if (!prows)
	{
		fprintf(stderr, "Error allocating memory for rref index array\n");
		return 1;
	}

	// Loop over diagonal index / column
	for (size_t i = 0; i < rows; i++)
	{
		// Pick the largest value in the remaining (leftmost square) rows as the next pivot
		size_t prow = i, pcol = i;
		double pval = -1;
		for (size_t j = i; j < rows; j++)
		{
			for (size_t k = 0; k < rows; k++)
			{
				if (fabs(A[j*cols + k]) >= pval)
				{
					pval = fabs(A[j*cols + k]);
					prow = j;
					pcol = k;
				}
			}
		}

		// Swap pivot into the current row
		if (prow != i)
		{
			for (size_t j = 0; j < cols; j++)
			{
				double t = A[i*cols + j];
				A[i*cols + j] = A[prow*cols + j];
				A[prow*cols + j] = t;
			}
			prow = i;
		}

		// Store the pivot row for later
		prows[pcol] = prow;

		// Check that we don't have a singular matrix
		if (fabs(A[prow*cols + pcol]) < eps)
		{
			free(prows);
			fprintf(stderr, "Cannot invert singular matrix\n");
			return 1;
		}

		// Normalize row by pivot
		for (size_t j = 0; j < cols; j++)
		{
			if (j == pcol)
				continue;

			A[prow*cols + j] /= A[prow*cols + pcol];
		}
		A[prow*cols + pcol] = 1.0;

		// Reduce column
		for (size_t l = 0; l < rows; l++)
		{
			if (l == prow)
				continue;

			for (size_t j = 0; j < cols; j++)
			{
				if (j == pcol)
					continue;

				A[l*cols + j] -= A[l*cols + pcol]*A[prow*cols + j];
			}
			A[l*cols + pcol] = 0;
		}
	}

	// Swap rows to make A the identity
	for (size_t i = 0; i < rows; i++)
	{
		// Don't swap with self or rows that have been completed
		if (prows[i] <= i)
			continue;

		int k = prows[i];
		for (size_t j = 0; j < cols; j++)
		{
			double t = A[i*cols + j];
			A[i*cols + j] = A[k*cols + j];
			A[k*cols + j] = t;
		}

		// Update remaining rows with new indices
		prows[i] = i;
		for (size_t j = i+1; j < cols; j++)
		{
			if (prows[j] == i)
			{
				prows[j] = k;
				break;
			}
		}
	}

	free(prows);
	return 0;
}

/*
 * Calculate a linear least-squares fit of numParams basis functions defined by evaluate_basis to (x,y)
 * e (required) specifies the estimated standard deviation in y, and is used to weight the fit
 */
static int fit(double *x, double *y, double *e, int n, double *params, size_t numParams,
	void (*evaluate_basis)(double x, double *basis, size_t n, void *user), void *user)
{
	size_t rows = numParams;
	size_t cols = numParams + 1;

	double *A = calloc(rows * cols, sizeof(double));
	if (A == NULL)
	{
		fprintf(stderr, "Error allocating memory for fit matrix\n");
		return 1;
	}

	double *basis = malloc(rows * sizeof(double));
	for (size_t i = 0; i < n; i++)
	{
		// Evaluate basis functions at x[i]
		evaluate_basis(x[i], basis, rows, user);

		// Estimated variance in y
		double var = e ? e[i] * e[i] : 1.0;

		for (size_t j = 0; j < rows; j++)
		{
			// Calculate Ajk contribution from i
			for (size_t k = 0; k < rows; k++)
				A[j*cols + k] += basis[j]*basis[k]/var;

			// Calculate b_j contribution from i
			A[j*cols + rows] += y[i]*basis[j]/var;
		}
	}

	// Solve for the coefficients
	if (rref(A, rows, cols))
	{
		free(A);
		fprintf(stderr, "Fit failed\n");
		return 1;
	}

	// Copy coeffs to output
	for (size_t i = 0; i < rows; i++)
		params[i] = A[i*cols + rows];

	free(A);
	return 0;
}

static void polynomial_fit(double x, double *basis, size_t n, void *user)
{
	basis[0] = 1;
	for (size_t j = 1; j < n; j++)
		basis[j] = basis[j-1]*x;
}

// Calculate a polynomial fit to the given x,y data
int fit_polynomial(double *x, double *y, double *e, size_t n, double *coeffs, size_t degree)
{
	return fit(x, y, e, n, coeffs, degree + 1, polynomial_fit, NULL);
}
