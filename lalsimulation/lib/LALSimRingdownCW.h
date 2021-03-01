#ifndef _LALSIM_RINGDOWN_CW_H
#define _LALSIM_RINGDOWN_CW_H
/* ************************************************************  */
/*
 * Copyright (C) 2016 Lionel London
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

#if defined(__cplusplus)
extern "C" {
#elif 0
} /* so that editors will match preceding brace */
#endif

/* Include the desired Libs */
#include <stdbool.h>
#include <math.h>
#include <complex.h>

/*
* Basic NOTE(s): Prototypes for LALSimRingdownCW.c
*/

/*
* Domain mapping for dimnesionless BH spin
*/
double SimRingdownCW_KAPPA(double jf, int l, int m);

/*
* Dimensionless QNM Frequencies: Note that name encodes date of writing
*/
complex double SimRingdownCW_CW07102016(double kappa, /* Domain mapping for  remnant BH's spin (Dimensionless) */
                                        int l,        /* Polar eigenvalue */
                                        int input_m,  /* Azimuthal eigenvalue*/
                                        int n);       /* Overtone Number*/

/* ************************************************************  */

#if 0
{ /* so that editors will match succeeding brace */
#elif defined(__cplusplus)
}
#endif

#endif /* _LALSIM_RINGDOWN_CW_H */
