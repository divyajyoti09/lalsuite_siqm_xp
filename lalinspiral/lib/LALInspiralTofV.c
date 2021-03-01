/*
*  Copyright (C) 2007 David Churches, B.S. Sathyaprakash, Drew Keppel
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with with program; see the file COPYING. If not, write to the
*  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*  MA  02110-1301  USA
*/

/**
 * \author Sathyaprakash, B. S.
 * \file
 * \ingroup LALInspiral_h
 *
 * \brief NONE
 *
 * This module outputs
 * \f{equation}{
 * tofv = t - t_0 + m \int_{v_0}^{v} \frac{E'(v)}{{\cal F}(v)} \, dv\,.
 * \f}
 * where the constants \f$t,\f$ \f$t_0,\f$ \f$v_0,\f$ and functions in the integrand
 * \f$E'(v)\f$ and \f${\cal F}(v)\f$ are defined in the \c void structure <tt>params.</tt>
 *
 * ### Uses ###
 *
 * \code
 * LALDRombergIntegrate()
 * \endcode
 *
 */

#include <math.h>
#include <lal/Integrate.h>
#include <lal/LALAtomicDatatypes.h>
#include <lal/LALInspiral.h>
#include <lal/XLALError.h>

REAL8
XLALInspiralTofV (
   REAL8 v,
   void *params
   )
{
   void *funcParams;
   REAL8 (*funcToIntegrate)(REAL8, void *);
   REAL8 xmin, xmax;
   IntegralType type;
   TofVIntegrandIn in2;
   TofVIn *in1;
   REAL8 answer;
   REAL8 sign;


   if (params == NULL)
      XLAL_ERROR_REAL8(XLAL_EFAULT);
   if (v <= 0.)
      XLAL_ERROR_REAL8(XLAL_EDOM);
   if (v >= 1.)
      XLAL_ERROR_REAL8(XLAL_EDOM);

   sign = 1.0;


   in1 = (TofVIn *) params;

   funcToIntegrate = XLALInspiralTofVIntegrand;
   xmin = in1->v0;
   xmax = v;
   type = ClosedInterval;


   in2.dEnergy = in1->dEnergy;
   in2.flux = in1->flux;
   in2.coeffs = in1->coeffs;

   funcParams = (void *) &in2;

   if (v==in1->v0)
   {
     return in1->t - in1->t0;
   }

   if(in1->v0 > v)
   {
      xmin = v;
      xmax = in1->v0;
      sign = -1.0;
   }

   answer = XLALREAL8RombergIntegrate (funcToIntegrate, funcParams, xmin, xmax, type);
   if (XLAL_IS_REAL8_FAIL_NAN(answer))
      XLAL_ERROR_REAL8(XLAL_EFUNC);

   return in1->t - in1->t0 + in1->totalmass*answer*sign;
}
