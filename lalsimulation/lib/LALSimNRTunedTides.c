/*
 * Copyright (C) 2017 Tim Dietrich, Sebastiano Bernuzzi, Nathan Johnson-McDaniel,
 * Shasvath J Kapadia, Francesco Pannarale and Sebastian Khan, Michael Puerrer.
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


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <lal/LALStdlib.h>
#include <lal/LALConstants.h>
#include <lal/Date.h>
#include <lal/Units.h>
#include <lal/LALSimIMR.h>

#include "LALSimNRTunedTides.h"
#include "LALSimUniversalRelations.h"

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

/**
 * Planck taper window
 */
static REAL8 PlanckTaper(const REAL8 t, const REAL8 t1, const REAL8 t2) {
  REAL8 taper;
  if (t <= t1)
    taper = 0.;
  else if (t >= t2)
    taper = 1.;
  else
    taper = 1. / (exp((t2 - t1)/(t - t1) + (t2 - t1)/(t - t2)) + 1.);

  return taper;
}

/**
 * function to swap masses and lambda to enforce m1 >= m2
 */
static int EnforcePrimaryMassIsm1(REAL8 *m1, REAL8 *m2, REAL8 *lambda1, REAL8 *lambda2){
  if ((*m1 == *m2) && (*lambda1 != *lambda2))
    XLALPrintWarning("m1 == m2 (%g), but lambda1 != lambda2 (%g, %g).\n", *m1, *lambda1, *lambda2);

  REAL8 lambda1_tmp, lambda2_tmp, m1_tmp, m2_tmp;
  if (*m1>=*m2) {
    lambda1_tmp = *lambda1;
    lambda2_tmp = *lambda2;
    m1_tmp   = *m1;
    m2_tmp   = *m2;
  } else { /* swap spins and masses */
    lambda1_tmp = *lambda2;
    lambda2_tmp = *lambda1;
    m1_tmp   = *m2;
    m2_tmp   = *m1;
  }
  *m1 = m1_tmp;
  *m2 = m2_tmp;
  *lambda1 = lambda1_tmp;
  *lambda2 = lambda2_tmp;

  if (*m1 < *m2)
    XLAL_ERROR(XLAL_EDOM, "XLAL_ERROR in EnforcePrimaryMassIsm1. When trying\
 to enfore that m1 should be the larger mass.\
 After trying to enforce this m1 = %f and m2 = %f\n", *m1, *m2);

  return XLAL_SUCCESS;
}


/**
 * convenient function to compute tidal coupling constant. Eq. 2 in arXiv:1706.02969
 * given masses and lambda numbers
 */
double XLALSimNRTunedTidesComputeKappa2T(
					 REAL8 m1_SI, /**< Mass of companion 1 (kg) */
					 REAL8 m2_SI, /**< Mass of companion 2 (kg) */
					 REAL8 lambda1, /**< (tidal deformability of mass 1) / m1^5 (dimensionless) */
					 REAL8 lambda2 /**< (tidal deformability of mass 2) / m2^5 (dimensionless) */
					 )
{
  int errcode = EnforcePrimaryMassIsm1(&m1_SI, &m2_SI, &lambda1, &lambda2);
  XLAL_CHECK(XLAL_SUCCESS == errcode, errcode, "EnforcePrimaryMassIsm1 failed");

  const REAL8 m1 = m1_SI / LAL_MSUN_SI;
  const REAL8 m2 = m2_SI / LAL_MSUN_SI;
  const REAL8 mtot = m1 + m2;

  /* Xa and Xb are the masses normalised for a total mass = 1 */
  /* not the masses appear symmetrically so we don't need to switch them. */
  const REAL8 Xa = m1 / mtot;
  const REAL8 Xb = m2 / mtot;

  /* tidal coupling constant. This is the
    kappa^T_eff = 2/13 [  (1 + 12 X_B/X_A) (X_A/C_A)^5 k^A_2 +  [A <- -> B]  ]
    from Tim Dietrich */

  /* Note that 2*k_2^A/c_A^5 = 3*lambda1 */
  const REAL8 term1 = ( 1.0 + 12.0*Xb/Xa ) * pow(Xa, 5.0) * lambda1;
  const REAL8 term2 = ( 1.0 + 12.0*Xa/Xb ) * pow(Xb, 5.0) * lambda2;
  const REAL8 kappa2T = (3.0/13.0) * ( term1 + term2 );

  return kappa2T;
}

/**
 * compute the merger frequency of a BNS system.
 * Tim's new fit that incorporates mass-ratio and asymptotes to zero for large kappa2T.
 */
double XLALSimNRTunedTidesMergerFrequency(
					  const REAL8 mtot_MSUN, /**< total mass of system (solar masses) */
					  const REAL8 kappa2T,   /**< tidal coupling constant. Eq. 2 in arXiv:1706.02969 */
					  const REAL8 q          /**< mass-ratio q >= 1 */
					  )
{
  if (q < 1.0) {
    XLALPrintError("XLAL Error - %s: q (%f) should be greater or equal to unity!\n",
		   __func__, q);
    XLAL_ERROR( XLAL_EDOM );
  }

  const REAL8 a_0 = 0.3586;
  const REAL8 n_1 = 3.35411203e-2;
  const REAL8 n_2 = 4.31460284e-5;
  const REAL8 d_1 = 7.54224145e-2;
  const REAL8 d_2 = 2.23626859e-4;

  const REAL8 kappa2T2 = kappa2T * kappa2T;

  const REAL8 num = 1.0 + n_1*kappa2T + n_2*kappa2T2;
  const REAL8 den = 1.0 + d_1*kappa2T + d_2*kappa2T2;
  const REAL8 Q_0 = a_0 / sqrt(q);

  /* dimensionless angular frequency of merger */
  const REAL8 Momega_merger = Q_0 * (num / den);

  /* convert from angular frequency to frequency (divide by 2*pi)
   * and then convert from
   * dimensionless frequency to Hz (divide by mtot * LAL_MTSUN_SI)
   */
  const REAL8 fHz_merger = Momega_merger / (LAL_TWOPI) / (mtot_MSUN * LAL_MTSUN_SI);

  return fHz_merger;
}

/**
 * Internal function only.
 * Function to call the frequency domain tidal correction.
 * Equation (7) in arXiv:1706.02969
 */
static double SimNRTunedTidesFDTidalPhase(
					  const REAL8 fHz, /**< Gravitational wave frequency (Hz) */
					  const REAL8 Xa, /**< Mass of companion 1 divided by total mass */
					  const REAL8 Xb, /**< Mass of companion 2 divided by total mass */
					  const REAL8 mtot, /**< total mass (Msun) */
					  const REAL8 kappa2T /**< tidal coupling constant. Eq. 2 in arXiv:1706.02969 */
					  )
{
  /* NRTunedTidesFDTidalPhase is Eq 7 in arXiv:1706.02969
   * and is a function of x = angular_orb_freq^(2/3)
   */
  REAL8 M_omega = LAL_PI * fHz * (mtot * LAL_MTSUN_SI); //dimensionless angular GW frequency

  REAL8 PN_x = pow(M_omega, 2.0/3.0);
  REAL8 PN_x_2 = PN_x * PN_x;
  REAL8 PN_x_3over2 = pow(PN_x, 3.0/2.0);
  REAL8 PN_x_5over2 = pow(PN_x, 5.0/2.0);

  /* model parameters */
  const REAL8 c_Newt = 2.4375; /* 39.0 / 16.0 */

  const REAL8 n_1 = -17.428;
  const REAL8 n_3over2 = 31.867;
  const REAL8 n_2 = -26.414;
  const REAL8 n_5over2 = 62.362;

  const REAL8 d_1 = n_1 - 2.496; /* 3115.0/1248.0 */
  const REAL8 d_3over2 = 36.089;

  REAL8 tidal_phase = - kappa2T * c_Newt / (Xa * Xb) * PN_x_5over2;

  REAL8 num = 1.0 + (n_1 * PN_x) + (n_3over2 * PN_x_3over2) + (n_2 * PN_x_2) + (n_5over2 * PN_x_5over2) ;
  REAL8 den = 1.0 + (d_1 * PN_x) + (d_3over2 * PN_x_3over2) ;

  REAL8 ratio = num / den;

  tidal_phase *= ratio;

  return tidal_phase;
}

/** 
 * Tidal amplitude corrections; only available for NRTidalv2;
 * Eq 24 of arxiv:1905.06011
 */
static REAL8 SimNRTunedTidesFDTidalAmplitude(
					     const REAL8 fHz, /**< Gravitational wave frequency (Hz) */
					     const REAL8 mtot, /**< Total mass in solar masses */
					     const REAL8 kappa2T /**< tidal coupling constant. Eq. 2 in arXiv:1706.02969 */
					     )
{
  const REAL8 M_sec   = (mtot * LAL_MTSUN_SI);

  REAL8 prefac = 0.0;
  prefac = 9.0*kappa2T;

  REAL8 x = pow(LAL_PI*M_sec*fHz, 2.0/3.0);
  REAL8 ampT = 0.0;
  REAL8 poly = 1.0;
  const REAL8 n1   = 4.157407407407407;
  const REAL8 n289 = 2519.111111111111;
  const REAL8 d    = 13477.8073677; 

  poly = (1.0 + n1*x + n289*pow(x, 2.89))/(1+d*pow(x,4.));
  ampT = - prefac*pow(x,3.25)*poly;

  return ampT;
}

/**
 * Set the NRTidalv2 phase coefficients in an array for use here and in the IMRPhenomX*_NRTidalv2 implementation
 */
int XLALSimNRTunedTidesSetFDTidalPhase_v2_Coeffs(REAL8 *NRTidalv2_coeffs)
{
  NRTidalv2_coeffs[0] =   2.4375; // c_Newt
  NRTidalv2_coeffs[1] = -12.615214237993088; // n_1
  NRTidalv2_coeffs[2] =  19.0537346970349; // n_3over2
  NRTidalv2_coeffs[3] = -21.166863146081035; // n_2
  NRTidalv2_coeffs[4] =  90.55082156324926; // n_5over2
  NRTidalv2_coeffs[5] = -60.25357801943598; // n_3
  NRTidalv2_coeffs[6] = -15.111207827736678; // d_1
  NRTidalv2_coeffs[7] =  22.195327350624694; // d_3over2
  NRTidalv2_coeffs[8] =   8.064109635305156; // d_2

  return XLAL_SUCCESS;
}

/** 
 * NRTunedTidesFDTidalPhase is Eq 22 of https://arxiv.org/abs/1905.06011 
 * and is a function of x = angular_orb_freq^(2/3)
 */
static double SimNRTunedTidesFDTidalPhase_v2(
					     const REAL8 fHz, /**< Gravitational wave frequency (Hz) */
					     const REAL8 Xa, /**< Mass of companion 1 divided by total mass */
					     const REAL8 Xb, /**< Mass of companion 2 divided by total mass */
					     const REAL8 mtot, /**< total mass (Msun) */
					     const REAL8 kappa2T /**< tidal coupling constant. Eq. 2 in arXiv:1706.02969 */
					     )
{

  REAL8 M_omega = LAL_PI * fHz * (mtot * LAL_MTSUN_SI); //dimensionless angular GW frequency
  REAL8 PN_x = pow(M_omega, 2.0/3.0);
  REAL8 PN_x_2 = PN_x * PN_x;
  REAL8 PN_x_3 = PN_x * PN_x_2;
  REAL8 PN_x_3over2 = pow(PN_x, 3.0/2.0);
  REAL8 PN_x_5over2 = pow(PN_x, 5.0/2.0);
  /* model parameters */
  REAL8 NRTidalv2_coeffs[9];

  int errcode;
  errcode = XLALSimNRTunedTidesSetFDTidalPhase_v2_Coeffs(NRTidalv2_coeffs);
  XLAL_CHECK(XLAL_SUCCESS == errcode, errcode, "Setting NRTidalv2 coefficients failed.\n");

  const REAL8 c_Newt   = NRTidalv2_coeffs[0];
  const REAL8 n_1      = NRTidalv2_coeffs[1];
  const REAL8 n_3over2 = NRTidalv2_coeffs[2];
  const REAL8 n_2      = NRTidalv2_coeffs[3];
  const REAL8 n_5over2 = NRTidalv2_coeffs[4];
  const REAL8 n_3      = NRTidalv2_coeffs[5];
  const REAL8 d_1      = NRTidalv2_coeffs[6];
  const REAL8 d_3over2 = NRTidalv2_coeffs[7];
  const REAL8 d_2      = NRTidalv2_coeffs[8];
  REAL8 tidal_phase = - kappa2T * c_Newt / (Xa * Xb) * PN_x_5over2;
  REAL8 num = 1.0 + (n_1 * PN_x) + (n_3over2 * PN_x_3over2) + (n_2 * PN_x_2) + (n_5over2 * PN_x_5over2) + (n_3 * PN_x_3);
  REAL8 den = 1.0 + (d_1 * PN_x) + (d_3over2 * PN_x_3over2) + (d_2 * PN_x_2) ;
  REAL8 ratio = num / den;
  tidal_phase *= ratio;
  return tidal_phase;
}

/** Function to call amplitude tidal series only; 
 * done for convenience to use for PhenomD_NRTidalv2 and 
 * SEOBNRv4_ROM_NRTidalv2
 */

int XLALSimNRTunedTidesFDTidalAmplitudeFrequencySeries(
						       const REAL8Sequence *amp_tidal, /**< [out] tidal amplitude frequency series */
						       const REAL8Sequence *fHz, /**< list of input Gravitational wave Frequency [Hz or dimensionless] */
						       REAL8 m1, /**< Mass of companion 1 in solar masses */
						       REAL8 m2, /**< Mass of companion 2 in solar masses */
						       REAL8 lambda1, /**< (tidal deformability of mass 1) / m1^5 (dimensionless) */
						       REAL8 lambda2 /**< (tidal deformability of mass 2) / m2^5 (dimensionless) */
						       )
{
  REAL8 m1_SI = m1 * LAL_MSUN_SI;
  REAL8 m2_SI = m2 * LAL_MSUN_SI;
  REAL8 f_dim_to_Hz;
  int errcode = EnforcePrimaryMassIsm1(&m1_SI, &m2_SI, &lambda1, &lambda2);
  XLAL_CHECK(XLAL_SUCCESS == errcode, errcode, "EnforcePrimaryMassIsm1 failed");
  
  if( lambda1 < 0 || lambda2 < 0 )
  XLAL_ERROR(XLAL_EFUNC, "lambda1 = %f, lambda2 = %f. Both should be greater than zero for NRTidal models", lambda1, lambda2);


  const REAL8 mtot = m1 + m2;
  /* SEOBNRv4ROM_NRTidalv2 and IMRPhenomD_NRTidalv2 deal with dimensionless freqs and freq in Hz;
   * If the value corresponding to the last index is above 1, we are safe to assume a frequency given in Hz,
   * otherwise a dimensionless frequency
   */

  if ((*fHz).data[(*fHz).length - 1] > 1.)
    f_dim_to_Hz = 1.;
  else 
    f_dim_to_Hz = mtot*LAL_MTSUN_SI;

  /* tidal coupling constant.*/
  const REAL8 kappa2T = XLALSimNRTunedTidesComputeKappa2T(m1_SI, m2_SI, lambda1, lambda2);

  for(UINT4 i = 0; i < (*fHz).length; i++)
    (*amp_tidal).data[i] = SimNRTunedTidesFDTidalAmplitude((*fHz).data[i]/f_dim_to_Hz, mtot, kappa2T);

  return XLAL_SUCCESS;
}

/**
 * Function to call the frequency domain tidal correction
 * over an array of input frequencies. This is
 * Equation (7) in arXiv:1706.02969 when NRTidal_version is NRTidal_V, 
 * or Equations (17)-(21) (for phasing) and Equation (24) (for amplitude) 
 * in arXiv:1905.06011 when NRTidal_version is NRTidalv2_V, 
 * or Equations (17)-(21) in arXiv:1905.06011 when NRTidal_version is NRTidalv2NoAmpCorr_V.
 * NoNRT_V specifies NO tidal phasing or amplitude is being added.
 * Note internally we use m1>=m2 - this is enforced in the code.
 * So any can be supplied
 *
 * The model for the tidal phase correction in NRTidal_V/NRTidalv2_V was calibrated
 * up to mass-ratio q=1.5 and kappa2T in [40, 5000].
 * The upper kappa2T limit is reached roughly for a
 * 1.4+1.4 BNS with lambda  = 2700 on both NSs.
 * In the high mass-ratio limit, the BNS merger frequency defined in
 * XLALSimNRTunedTidesMergerFrequency() asymptotes to zero. The waveform
 * amplitude should be tapered away starting at this frequency.
 * Therefore, no explicit limits are enforced.
 */

int XLALSimNRTunedTidesFDTidalPhaseFrequencySeries(
						   const REAL8Sequence *phi_tidal, /**< [out] tidal phase frequency series */
						   const REAL8Sequence *amp_tidal, /**< [out] tidal amplitude frequency series */
						   const REAL8Sequence *planck_taper, /**< [out] planck tapering to be applied on overall signal */
						   const REAL8Sequence *fHz, /**< list of input Gravitational wave Frequency in Hz to evaluate */
						   REAL8 m1_SI, /**< Mass of companion 1 (kg) */
						   REAL8 m2_SI, /**< Mass of companion 2 (kg) */
						   REAL8 lambda1, /**< (tidal deformability of mass 1) / m1^5 (dimensionless) */
						   REAL8 lambda2, /**< (tidal deformability of mass 2) / m2^5 (dimensionless) */
						   NRTidal_version_type NRTidal_version /** < one of NRTidal_V, NRTidalv2_V or NRTidalv2NoAmpCorr_V or NoNRT_V */
						   )
{
  /* NOTE: internally m1 >= m2
   * This is enforced in the code below and we swap the lambda's
   * accordingly.
   */
  int errcode = EnforcePrimaryMassIsm1(&m1_SI, &m2_SI, &lambda1, &lambda2);
  XLAL_CHECK(XLAL_SUCCESS == errcode, errcode, "EnforcePrimaryMassIsm1 failed");
  
  if( lambda1 < 0 || lambda2 < 0 )
  XLAL_ERROR(XLAL_EFUNC, "lambda1 = %f, lambda2 = %f. Both should be greater than zero for NRTidal models", lambda1, lambda2);

  const REAL8 m1 = m1_SI / LAL_MSUN_SI;
  const REAL8 m2 = m2_SI / LAL_MSUN_SI;
  const REAL8 mtot = m1 + m2;
  const REAL8 q = m1 / m2;

  /* Xa and Xb are the masses normalised for a total mass = 1 */
  const REAL8 Xa = m1 / mtot;
  const REAL8 Xb = m2 / mtot;

  /* tidal coupling constant.*/
  const REAL8 kappa2T = XLALSimNRTunedTidesComputeKappa2T(m1_SI, m2_SI, lambda1, lambda2);

  /* Prepare tapering of amplitude beyond merger frequency */
  const REAL8 fHz_mrg = XLALSimNRTunedTidesMergerFrequency(mtot, kappa2T, q);

  const REAL8 fHz_end_taper = 1.2*fHz_mrg;
  if (NRTidal_version == NRTidal_V) {
    for(UINT4 i = 0; i < (*fHz).length; i++){
      (*phi_tidal).data[i] = SimNRTunedTidesFDTidalPhase((*fHz).data[i], Xa, Xb, mtot, kappa2T);
      (*planck_taper).data[i] = 1.0 - PlanckTaper((*fHz).data[i], fHz_mrg, fHz_end_taper);
    }
  }
  else if (NRTidal_version == NRTidalv2_V) {
    for(UINT4 i = 0; i < (*fHz).length; i++) {
      (*phi_tidal).data[i] = SimNRTunedTidesFDTidalPhase_v2((*fHz).data[i], Xa, Xb, mtot, kappa2T);
      (*amp_tidal).data[i] = SimNRTunedTidesFDTidalAmplitude((*fHz).data[i], mtot, kappa2T);
      (*planck_taper).data[i] = 1.0 - PlanckTaper((*fHz).data[i], fHz_mrg, fHz_end_taper);
    }
  }
  else if (NRTidal_version == NRTidalv2NSBH_V) {
    for(UINT4 i = 0; i < (*fHz).length; i++) {
      (*phi_tidal).data[i] = SimNRTunedTidesFDTidalPhase_v2((*fHz).data[i], Xa, Xb, mtot, kappa2T);
      (*planck_taper).data[i] = 1.0;
    }
  }
  else if (NRTidal_version == NRTidalv2NoAmpCorr_V) {
    for(UINT4 i = 0; i < (*fHz).length; i++) {
      (*phi_tidal).data[i] = SimNRTunedTidesFDTidalPhase_v2((*fHz).data[i], Xa, Xb, mtot, kappa2T);
      (*planck_taper).data[i] = 1.0 - PlanckTaper((*fHz).data[i], fHz_mrg, fHz_end_taper);
    }
  }
  else if (NRTidal_version == NoNRT_V)
    XLAL_ERROR( XLAL_EINVAL, "Trying to add NRTides to a BBH waveform!" );
  else
    XLAL_ERROR( XLAL_EINVAL, "Unknown version of NRTidal being used! At present, NRTidal_V, NRTidalv2_V, NRTidalv2NSBH_V, NRTidalv2NoAmpCorr_V and NoNRT_V are the only known ones!" );

  return XLAL_SUCCESS;
}

/**
 * Function to add 3.5PN spin-squared and 3.5PN spin-cubed terms. 
 * The spin-squared terms occur with the spin-induced quadrupole moment terms
 * while the spin-cubed terms occur with both spin-induced quadrupole as well as 
 * octupole moments. The terms are computed in arXiv:1806.01772 and are 
 * explicitly written out in Eq 27 of arXiv:1905.06011. The following terms
 * are specifically meant for BNS systems, and are added to the NRTidalv2
 * extensions of the approximants IMRPhenomPv2, IMRPhenomD and SEOBNRv4_ROM. 
 */

void XLALSimInspiralGetHOSpinTerms(
				   REAL8 *SS_3p5PN, /**< 3.5PN spin-spin tail term containing spin-induced quadrupole moment */
				   REAL8 *SSS_3p5PN, /**< 3.5 PN spin cubed term containing spin-induced octupole moment */
				   REAL8 X_A, /**< Mass fraction m_1/M for first component of binary */
				   REAL8 X_B, /**< Mass fraction m_2/M for second component of binary */
				   REAL8 chi1, /**< Aligned component of spin vector of first component of binary */
				   REAL8 chi2, /**< Aligned component of spin vector of second component of binary */
				   REAL8 quadparam1, /**< Spin-induced quadrupole moment parameter for component 1 */
				   REAL8 quadparam2 /**< Spin-induced quadrupole moment parameter for component 2 */
				   )
{
  REAL8 chi1_sq = 0., chi2_sq = 0.;
  REAL8 X_Asq = 0., X_Bsq = 0.;
  REAL8 octparam1 = 0, octparam2 = 0.;

  X_Asq = X_A*X_A;
  X_Bsq = X_B*X_B;

  chi1_sq = chi1*chi1;
  chi2_sq = chi2*chi2;

  /* Remove -1 to account for BBH baseline*/
  octparam1 = XLALSimUniversalRelationSpinInducedOctupoleVSSpinInducedQuadrupole(quadparam1)-1.;
  octparam2 = XLALSimUniversalRelationSpinInducedOctupoleVSSpinInducedQuadrupole(quadparam2)-1.;

  *SS_3p5PN = - 400.*LAL_PI*(quadparam1-1.)*chi1_sq*X_Asq - 400.*LAL_PI*(quadparam2-1.)*chi2_sq*X_Bsq;
  *SSS_3p5PN = 10.*((X_Asq+308./3.*X_A)*chi1+(X_Bsq-89./3.*X_B)*chi2)*(quadparam1-1.)*X_Asq*chi1_sq
    + 10.*((X_Bsq+308./3.*X_B)*chi2+(X_Asq-89./3.*X_A)*chi1)*(quadparam2-1.)*X_Bsq*chi2_sq
    - 440.*octparam1*X_A*X_Asq*chi1_sq*chi1 - 440.*octparam2*X_B*X_Bsq*chi2_sq*chi2;
}
