/*
 * Copyright (C) 2014,2015 Kipp Cannon
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with with program; see the file COPYING. If not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#include <math.h>


#include <lal/Date.h>
#include <lal/LALDatatypes.h>
#include <lal/LALMalloc.h>
#include <lal/TimeSeriesInterp.h>
#include <lal/Window.h>
#include <lal/XLALError.h>


/**
 * Default kernel function. A Welch-windowed sinc interpolating kernel is
 * used.  See
 *
 * Smith, Julius O. Digital Audio Resampling Home Page Center for Computer
 * Research in Music and Acoustics (CCRMA), Stanford University,
 * 2014-01-10.  Web published at
 * http://www-ccrma.stanford.edu/~jos/resample/.
 *
 * for more information, but note that that reference uses a Kaiser window
 * for the sinc kernel's envelope whereas we use a Welch window here.  The
 * Welch window (inverted parabola) is chosen because it yields results
 * similar in accuracy to the Lanczos window but is much less costly to
 * compute.
 */


struct default_kernel_data {
	double welch_factor;
};


static void default_kernel(double *cached_kernel, int kernel_length, double residual, void *data)
{
	/* kernel is Welch-windowed sinc function.  the sinc component
	 * takes the form
	 *
	 *	x = pi (i - x);
	 *	kern = sin(x) / x
	 *
	 * we don't check for 0/0 because that can only occur if x is an
	 * integer, which is trapped by no-op path above.  note that the
	 * argument of sin(x) increases by pi each iteration, so we just
	 * need to compute its value for the first iteration then flip sign
	 * for each subsequent iteration.  for numerical reasons, it's
	 * better to compute sin(x) from residual rather than from (start -
	 * x), i.e. what it's argument should be for the first iteration,
	 * so we also have to figure out how many factors of -1 to apply to
	 * get its sign right for the first iteration.
	 */

	double welch_factor = ((struct default_kernel_data *) data)->welch_factor;
	double *stop = cached_kernel + kernel_length;
	/* put a factor of welch_factor in this.  see below */
	double sinx_over_pi = sin(LAL_PI * residual) / LAL_PI * welch_factor;
	int i;
	if(kernel_length & 2)
		sinx_over_pi = -sinx_over_pi;
	for(i = -(kernel_length - 1) / 2; cached_kernel < stop; i++, sinx_over_pi = -sinx_over_pi) {
		double y = welch_factor * (i + residual);
		if(fabs(y) < 1.)
			/* the window is
			 *
			 * sinx_over_pi / i * (1. - y * y)
			 *
			 * but by putting an extra factor of welch_factor
			 * into sinx_over_pi we can replace i with y,
			 * and then move the factor of 1/y into the
			 * parentheses to reduce the total number of
			 * arithmetic operations in the loop
			 */
			*cached_kernel++ = sinx_over_pi * (1. / y - y);
		else
			*cached_kernel++ = 0.;
	}
}


/**
 * Opaque LALREAL8SequenceInterp instance structure.
 */


struct tagLALREAL8SequenceInterp {
	const REAL8Sequence *s;
	int kernel_length;
	double *cached_kernel;
	double residual;
	/* samples.  the length of the kernel sets the bandwidth of the
	 * interpolator:  the longer the kernel, the closer to an ideal
	 * interpolator it becomes.  we tie the interval at which the
	 * kernel is regenerated to this in a heuristic way to hide the
	 * sub-sample residual quantization in the filter's roll-off. */
	double noop_threshold;
	/* calling-code supplied kernel generator */
	void (*kernel)(double *, int, double, void *);
	void *kernel_data;
};


/**
 * Create a new REAL8Sequence interpolator associated with the given
 * REAL8Sequence object.  The kernel_length parameter sets the length of
 * the interpolating kernel in samples.  Use
 * XLALREAL8SequenceInterpDestroy() to free.
 *
 * The optional kernel() function has the signature
 *
 * void (*kernel)(double *kern, int length, double residual, void *data);
 *
 * The optional kernel_data parameter will be passed to kernel() as the
 * final argument.  It is an error to provide a non-NULL kernel_data
 * pointer without supplying a kernel() function.  If both are NULL, an
 * internal default Welch-windowed sinc kernel function will be used.  If
 * supplied, the kernel() function is responsible for computing the
 * time-domain interpolation kernel.  The kernel must be exactly length
 * samples long and be placed in the kernel array, which is guaranteed to
 * be non-NULL and large enough to contain length samples.  length is
 * guaranteed to be odd, and not less than 3.
 *
 * residual is the interval from the nearest available sample of source
 * data to the co-ordinate at which the user has requested the interpolated
 * function be evaluated.  The middle sample of the kernel will be the
 * weight applied to that sample of source data.  A purely sinc() kernel
 * function would be of the form
 *
 * static voic kernel(double *kern, int length, double res, void *ignored)
 * {
 *	int i;
 *
 *	for(i = 0; i < length; i++) {
 *		double x = i - (length - 1) / 2.;
 *		kern[i] = x ? sin(PI * x + res) / (PI * x + res) : 1.;
 *	}
 * }
 *
 * Notes:
 *
 * The LALREAL8SequenceInterp object is opaque.  Calling code should not
 * attempt to inspect nor manipulate its contents directly.
 *
 * The REAL8Sequence object with which the LALREAL8SequenceInterp is
 * associated must remain valid as long as the interpolator exists.  Do not
 * free the REAL8Sequence before freeing the LALREAL8SequenceInterp.
 */


LALREAL8SequenceInterp *XLALREAL8SequenceInterpCreate(const REAL8Sequence *s, int kernel_length, void (*kernel)(double *, int, double, void *), void *kernel_data)
{
	LALREAL8SequenceInterp *interp;
	double *cached_kernel;

	if(kernel_length < 3)
		XLAL_ERROR_NULL(XLAL_EDOM);
	if(!kernel && kernel_data) {
		/* FIXME:  print error message */
		XLAL_ERROR_NULL(XLAL_EINVAL);
	}
	/* interpolator induces phase shifts unless this is odd */
	kernel_length -= (~kernel_length) & 1;

	interp = XLALMalloc(sizeof(*interp));
	cached_kernel = XLALMalloc(kernel_length * sizeof(*cached_kernel));
	if(!interp || !cached_kernel) {
		XLALFree(interp);
		XLALFree(cached_kernel);
		XLAL_ERROR_NULL(XLAL_EFUNC);
	}

	interp->s = s;
	interp->kernel_length = kernel_length;
	interp->cached_kernel = cached_kernel;
	/* >= 1 --> impossible.  forces kernel init on first eval */
	interp->residual = 2.;
	/* set no-op threshold.  the kernel is recomputed when the residual
	 * changes by this much */
	interp->noop_threshold = 1. / (4 * interp->kernel_length);

	/* install interpolator, using default if needed */
	if(!kernel) {
		struct default_kernel_data *default_kernel_data = XLALMalloc(sizeof(*default_kernel_data));
		if(!default_kernel_data) {
			XLALFree(interp);
			XLALFree(cached_kernel);
			XLAL_ERROR_NULL(XLAL_EFUNC);
		}

		default_kernel_data->welch_factor = 1.0 / ((kernel_length - 1.) / 2. + 1.);

		kernel = default_kernel;
		kernel_data = default_kernel_data;
	} else if(kernel == default_kernel) {
		/* not allowed because destroy method checks for the
		 * default kernel to decide if it must free the kernel_data
		 * memory */
		XLALFree(interp);
		XLALFree(cached_kernel);
		XLAL_ERROR_NULL(XLAL_EINVAL);
	}
	interp->kernel = kernel;
	interp->kernel_data = kernel_data;

	return interp;
}


/**
 * Free a LALREAL8SequenceInterp object.  NULL is no-op.
 */


void XLALREAL8SequenceInterpDestroy(LALREAL8SequenceInterp *interp)
{
	if(interp) {
		XLALFree(interp->cached_kernel);
		/* unref the REAL8Sequence.  place-holder in case this code
		 * is ported to a language where this matters */
		interp->s = NULL;
		/* only free if we allocated it ourselves */
		if(interp->kernel == default_kernel)
			XLALFree(interp->kernel_data);
		/* unref kernel and kernel_data.  place-holder in case this
		 * code is ported to a language where this matters */
		interp->kernel = NULL;
		interp->kernel_data = NULL;
	}
	XLALFree(interp);
}


/**
 * Evaluate a LALREAL8SequenceInterp at the real-valued index x.  The data
 * beyond the domain of the input sequence are assumed to be 0 when
 * computing results near (or beyond) the boundaries.  An XLAL_EDOM domain
 * error is raised if x is not finite.  If bounds_check is non-zero then an
 * XLAL_EDOM domain error is also raised if x is not in [0, length) where
 * length is the sample count of the sequence to which the interpolator is
 * attached.
 *
 * Be aware that for performance reasons the interpolating kernel is cached
 * and only recomputed if the error estimated to arise from failing to
 * recompute it exceeds the error estimated to arise from using a finite
 * interpolating kernel.  Therefore, if a function is interpolated at very
 * high resolution with a short kernel the result will consist of intervals
 * of constant values in a stair-step pattern.  The stair steps should be a
 * small contribution to the interpolation error but numerical
 * differentiation of the result is likely to be unsatisfactory.  In that
 * case, consider interpolating the derivative or use a longer kernel to
 * force more frequent kernel updates.
 */


REAL8 XLALREAL8SequenceInterpEval(LALREAL8SequenceInterp *interp, double x, int bounds_check)
{
	const REAL8 *data = interp->s->data;
	double *cached_kernel = interp->cached_kernel;
	double *stop = cached_kernel + interp->kernel_length;
	/* split the real-valued sample index into integer and fractional
	 * parts.  the fractional part (residual) is the offset in samples
	 * from where we want to evaluate the function to where we know its
	 * value.  the interpolating kernel depends only on this quantity.
	 * when we compute a kernel, we record the value of this quantity,
	 * and only recompute the kernel if this quantity differs from the
	 * one for which the kernel was computed by more than the no-op
	 * threshold */
	int start = lround(x);
	double residual = start - x;
	REAL8 val;

	if(!isfinite(x) || (bounds_check && (x < 0 || x >= interp->s->length)))
		XLAL_ERROR_REAL8(XLAL_EDOM);

	/* special no-op case for default kernel */
	if(fabs(residual) < interp->noop_threshold && interp->kernel == default_kernel)
		return 0 <= start && start < (int) interp->s->length ? data[start] : 0.0;

	/* need new kernel? */
	if(fabs(residual - interp->residual) >= interp->noop_threshold) {
		interp->kernel(cached_kernel, interp->kernel_length, residual, interp->kernel_data);
		interp->residual = residual;
	}

	/* inner product of kernel and samples */
	start -= (interp->kernel_length - 1) / 2;
	if(start + interp->kernel_length > (signed) interp->s->length)
		stop -= start + interp->kernel_length - interp->s->length;
	if(start < 0)
		cached_kernel -= start;
	else
		data += start;
	for(val = 0.0; cached_kernel < stop;)
		val += *cached_kernel++ * *data++;

	return val;
}


struct tagLALREAL8TimeSeriesInterp {
	const REAL8TimeSeries *series;
	LALREAL8SequenceInterp *seqinterp;
};


/**
 * Create a new REAL8TimeSeries interpolator associated with the given
 * REAL8TimeSeries object.  The kernel_length parameter sets the length of
 * the interpolating kernel in samples.  Use
 * XLALREAL8TimeSeriesInterpDestroy() to free.
 *
 * See XLALREAL8SequenceInterpCreate() for the definition of the kernel and
 * kernel_data parameters.  These are optional;  set both to NULL to use
 * the internal default interpolation kernel.
 *
 * Notes:
 *
 * The LALREAL8TimeSeriesInterp object is opaque.  Calling code should not
 * attempt to inspect nor manipulate its contents directly.
 *
 * The REAL8TimeSeries object with which the LALREAL8TimeSeriesInterp is
 * associated must remain valid as long as the interpolator exists.  Do not
 * free the REAL8TimeSeries before freeing the LALREAL8TimeSeriesInterp.
 */


LALREAL8TimeSeriesInterp *XLALREAL8TimeSeriesInterpCreate(const REAL8TimeSeries *series, int kernel_length, void (*kernel)(double *, int, double, void *), void *kernel_data)
{
	LALREAL8TimeSeriesInterp *interp;
	LALREAL8SequenceInterp *seqinterp;

	interp = XLALMalloc(sizeof(*interp));
	seqinterp = XLALREAL8SequenceInterpCreate(series->data, kernel_length, kernel, kernel_data);
	if(!interp || !seqinterp) {
		XLALFree(interp);
		XLALREAL8SequenceInterpDestroy(seqinterp);
		XLAL_ERROR_NULL(XLAL_EFUNC);
	}

	interp->series = series;
	interp->seqinterp = seqinterp;

	return interp;
}


/**
 * Free a LALREAL8TimeSeriesInterp object.  NULL is no-op.
 */


void XLALREAL8TimeSeriesInterpDestroy(LALREAL8TimeSeriesInterp *interp)
{
	if(interp) {
		XLALREAL8SequenceInterpDestroy(interp->seqinterp);
		/* unref the REAL8TimeSeries.  place-holder in case this
		 * code is ported to a language where this matters */
		interp->series = NULL;
	}
	XLALFree(interp);
}


/**
 * Evaluate a LALREAL8TimeSeriesInterp at the LIGOTimeGPS t.  Raises a
 * XLAL_EDOM domain error if t is not in [epoch, epoch + length * deltaT)
 * where epoch, length, and deltaT are the start time, sample count, and
 * sample period, respectively, of the time series to which the
 * interpolator is attached.
 *
 * See XLALREAL8SequenceInterpEval() for information about the
 * interpolation kernel that is used and performance enhancements that can
 * give rise to numerical artifacts.
 */


REAL8 XLALREAL8TimeSeriesInterpEval(LALREAL8TimeSeriesInterp *interp, const LIGOTimeGPS *t, int bounds_check)
{
	return XLALREAL8SequenceInterpEval(interp->seqinterp, XLALGPSDiff(t, &interp->series->epoch) / interp->series->deltaT, bounds_check);
}
