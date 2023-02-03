/*
*  Copyright (C) 2007 Drew Keppel, George Birthisel, Patrick Brady, Peter Shawhan, Stephen Fairhurst
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

/*-----------------------------------------------------------------------
 *
 * File Name: SimInspiralUtils.c
 *
 * Author: Brady, P. R., Brown, D. A., and Fairhurst, S
 *
 *-----------------------------------------------------------------------
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <lal/XLALError.h>
#include <lal/LALError.h>
#include <lal/LALStdlib.h>
#include <lal/LALStdio.h>
#include <lal/LIGOMetadataTables.h>
#include <lal/LIGOMetadataUtils.h>
#include <lal/LIGOMetadataInspiralUtils.h>
#include <lal/Date.h>
#include <lal/SkyCoordinates.h>
#include <lal/GeneratePPNInspiral.h>
#include <lal/DetectorSite.h>
#include <lal/DetResponse.h>
#include <lal/TimeDelay.h>

/**
 * \author Brown, D. A.
 * \file
 *
 * \brief Provides a set of utilities for manipulating \c simInspiralTables.
 *
 * ### Description ###
 *
 * The function <tt>LALInspiralSiteTimeAndDist()</tt> calculates detector end
 * time (\c endTime) and effective distance (\c effDist) for an
 * inspiral signal from a specific location in the sky (\c skyPos) assumed
 * to be given in equatorial coordinates.  The detector end time is obtained by
 * using <tt>LALTimeDelayFromEarthCenter()</tt>, while the effective distance
 * requires calculation of the detector response, calculated using
 * <tt>LALComputeDetAMResponse()</tt>.
 *
 * The function <tt>LALPopulateSimInspiralSiteInfo()</tt> populates the end time
 * and effective distance for each of the interferometer sites.  The sky location
 * (in equatorial coordinates) is assumed to be already contained in the input
 * \c SimInspiralTable.  The end time and effective distance for each site
 * is calculated by calling <tt>LALInspiralSiteTimeAndDist()</tt> once for each
 * of the detectors, and setting the \c detector appropriately.
 *
 * ### Algorithm ###
 *
 * None.
 *
 * ### Uses ###
 *
 * LALGetInspiralParams(), LALGPStoGMST1(), LALTimeDelayFromEarthCenter(),
 * LALComputeDetAMResponse().
 *
 */

  /* a few useful static functions */
static INT8 geocent_end_time(const SimInspiralTable *x)
{
  return(XLALGPSToINT8NS(&x->geocent_end_time));
}


void
LALGalacticInspiralParamsToSimInspiralTable(
    LALStatus                  *status,
    SimInspiralTable           *output,
    GalacticInspiralParamStruc *input,
    RandomParams               *params
    )

{
  PPNParamStruc         ppnParams;
  SkyPosition           skyPos;
  LALSource             source;
  LALDetector           lho = lalCachedDetectors[LALDetectorIndexLHODIFF];
  LALDetector           llo = lalCachedDetectors[LALDetectorIndexLLODIFF];
  LALDetAndSource       detAndSource;
  LALDetAMResponse      resp;
  REAL8     time_diff;
  REAL4                 splus, scross, cosiota;

  INITSTATUS(status);
  ATTATCHSTATUSPTR( status );

  ASSERT( output, status,
      LAL_NULL_ERR, LAL_NULL_MSG );
  ASSERT( input, status,
      LAL_NULL_ERR, LAL_NULL_MSG );
  ASSERT( params, status,
      LAL_NULL_ERR, LAL_NULL_MSG );


  /*
   *
   * compute sky position and inspiral params
   *
   */


  /* generate the ppn inspiral params */
  memset( &ppnParams, 0, sizeof(PPNParamStruc) );
  LALGetInspiralParams( status->statusPtr, &ppnParams, input, params );
  CHECKSTATUSPTR( status );

  if ( ppnParams.position.system != COORDINATESYSTEM_EQUATORIAL )
  {
    ABORT( status, LAL_BADPARM_ERR, LAL_BADPARM_MSG );
  }

  /* copy the inspiral data into sim_inspiral table */
  output->mass1        = input->m1;
  output->mass2        = input->m2;
  output->eta          = ppnParams.eta;
  output->distance     = ppnParams.d / (1.0e6 * LAL_PC_SI); /* Mpc */
  output->longitude    = ppnParams.position.longitude;
  output->latitude     = ppnParams.position.latitude;
  output->inclination  = ppnParams.inc;
  output->coa_phase    = ppnParams.phi;
  output->polarization = ppnParams.psi;

  /* populate geocentric end time */
  output->geocent_end_time = input->geocentEndTime;

  /* populate gmst field (hours) */
  output->end_time_gmst = fmod(XLALGreenwichMeanSiderealTime(
      &output->geocent_end_time), LAL_TWOPI) * 24.0 / LAL_TWOPI;  /* hours*/
  ASSERT( !XLAL_IS_REAL8_FAIL_NAN(output->end_time_gmst), status, LAL_FAIL_ERR, LAL_FAIL_MSG );

  /* set up params for the site end times and detector response */
  memset( &skyPos, 0, sizeof(SkyPosition) );
  memset( &source, 0, sizeof(LALSource) );
  memset( &detAndSource, 0, sizeof(LALDetAndSource) );

  skyPos.longitude = output->longitude;
  skyPos.latitude  = output->latitude;
  skyPos.system    = COORDINATESYSTEM_EQUATORIAL;

  source.equatorialCoords = skyPos;
  source.orientation      = output->polarization;

  detAndSource.pSource = &source;


  /*
   *
   * compute site end times
   *
   */


  /* initialize end times with geocentric value */
  output->h_end_time = output->l_end_time = input->geocentEndTime;

  /* ligo hanford observatory */
  time_diff = XLALTimeDelayFromEarthCenter( lho.location, skyPos.longitude, skyPos.latitude, &(output->geocent_end_time) );
  XLALGPSAdd(&(output->h_end_time), time_diff);

  /* ligo livingston observatory */
  time_diff = XLALTimeDelayFromEarthCenter( llo.location, skyPos.longitude, skyPos.latitude, &(output->geocent_end_time) );
  XLALGPSAdd(&(output->l_end_time), time_diff);


  /*
   *
   * compute the effective distance of the inspiral
   *
   */


  /* initialize distances with real distance and compute splus and scross */
  output->eff_dist_h = output->eff_dist_l = 2.0 * output->distance;
  cosiota = cos( output->inclination );
  splus = -( 1.0 + cosiota * cosiota );
  scross = -2.0 * cosiota;

  /* compute the response of the LHO detectors */
  detAndSource.pDetector = &lho;
  LALComputeDetAMResponse( status->statusPtr, &resp, &detAndSource,
      &output->geocent_end_time );
  CHECKSTATUSPTR( status );

  /* compute the effective distance for LHO */
  output->eff_dist_h /= sqrt(
      splus*splus*resp.plus*resp.plus + scross*scross*resp.cross*resp.cross );

  /* compute the response of the LLO detector */
  detAndSource.pDetector = &llo;
  LALComputeDetAMResponse( status->statusPtr, &resp, &detAndSource,
      &output->geocent_end_time );
  CHECKSTATUSPTR( status );

  /* compute the effective distance for LLO */
  output->eff_dist_l /= sqrt(
      splus*splus*resp.plus*resp.plus + scross*scross*resp.cross*resp.cross );


  /*
   *
   * normal exit
   *
   */


  DETATCHSTATUSPTR (status);
  RETURN (status);
}


static void
LALInspiralSiteTimeAndDist(
    LALStatus         *status,
    SimInspiralTable  *output,
    LALDetector       *detector,
    LIGOTimeGPS       *endTime,
    REAL4             *effDist,
    SkyPosition       *skyPos
    )

{
  LALSource             source;
  LALDetAndSource       detAndSource;
  LALDetAMResponse      resp;
  REAL8                 time_diff;
  REAL4                 splus, scross, cosiota;

  INITSTATUS(status);
  ATTATCHSTATUSPTR( status );

  /* check that the arguments are not null */
  ASSERT( output, status,
      LAL_NULL_ERR, LAL_NULL_MSG );
  ASSERT( detector, status,
      LAL_NULL_ERR, LAL_NULL_MSG );
  ASSERT( endTime, status,
      LAL_NULL_ERR, LAL_NULL_MSG );
  ASSERT( effDist, status,
      LAL_NULL_ERR, LAL_NULL_MSG );
  ASSERT( skyPos, status,
      LAL_NULL_ERR, LAL_NULL_MSG );

  memset( &source, 0, sizeof(LALSource) );
  memset( &detAndSource, 0, sizeof(LALDetAndSource) );

  source.equatorialCoords = *skyPos;
  source.orientation      = output->polarization;

  detAndSource.pSource = &source;
  detAndSource.pDetector = detector;

  /* initialize end time with geocentric value */
  *endTime = output->geocent_end_time;

  /* calculate the detector end time */
  time_diff = XLALTimeDelayFromEarthCenter( detector->location, skyPos->longitude, skyPos->latitude, &(output->geocent_end_time) );
  XLALGPSAdd(endTime, time_diff);

  /* initialize distance with real distance and compute splus and scross */
  *effDist = 2.0 * output->distance;
  cosiota = cos( output->inclination );
  splus = -( 1.0 + cosiota * cosiota );
  scross = -2.0 * cosiota;

  /* compute the response of the detector */
  LALComputeDetAMResponse( status->statusPtr, &resp, &detAndSource,
      &output->geocent_end_time );
  CHECKSTATUSPTR( status );

  /* compute the effective distance */
  *effDist /= sqrt(
      splus*splus*resp.plus*resp.plus + scross*scross*resp.cross*resp.cross );

  /* normal exit */
  DETATCHSTATUSPTR (status);
  RETURN (status);
}




void
LALPopulateSimInspiralSiteInfo(
    LALStatus                  *status,
    SimInspiralTable           *output
    )

{
  SkyPosition           skyPos;
  LALDetector           detector;
  REAL4                *eff_dist;
  LIGOTimeGPS          *end_time;


  INITSTATUS(status);
  ATTATCHSTATUSPTR( status );

  ASSERT( output, status,
      LAL_NULL_ERR, LAL_NULL_MSG );

  /* set up params for the geocent end time and source location */
  memset( &skyPos, 0, sizeof(SkyPosition) );

  skyPos.longitude = output->longitude;
  skyPos.latitude  = output->latitude;
  skyPos.system    = COORDINATESYSTEM_EQUATORIAL;

  /* LIGO Hanford observatory*/
  detector = lalCachedDetectors[LALDetectorIndexLHODIFF];
  end_time = &(output->h_end_time);
  eff_dist = &(output->eff_dist_h);
  LALInspiralSiteTimeAndDist(status->statusPtr, output, &detector, end_time,
      eff_dist, &skyPos);

  /* LIGO Livingston observatory*/
  detector = lalCachedDetectors[LALDetectorIndexLLODIFF];
  end_time = &(output->l_end_time);
  eff_dist = &(output->eff_dist_l);
  LALInspiralSiteTimeAndDist(status->statusPtr, output, &detector, end_time,
      eff_dist, &skyPos);

  /* GEO observatory*/
  detector = lalCachedDetectors[LALDetectorIndexGEO600DIFF];
  end_time = &(output->g_end_time);
  eff_dist = &(output->eff_dist_g);
  LALInspiralSiteTimeAndDist(status->statusPtr, output, &detector, end_time,
      eff_dist, &skyPos);

  /* TAMA observatory*/
  detector = lalCachedDetectors[LALDetectorIndexTAMA300DIFF];
  end_time = &(output->t_end_time);
  eff_dist = &(output->eff_dist_t);
  LALInspiralSiteTimeAndDist(status->statusPtr, output, &detector, end_time,
      eff_dist, &skyPos);

  /* Virgo observatory*/
  detector = lalCachedDetectors[LALDetectorIndexVIRGODIFF];
  end_time = &(output->v_end_time);
  eff_dist = &(output->eff_dist_v);
  LALInspiralSiteTimeAndDist(status->statusPtr, output, &detector, end_time,
      eff_dist, &skyPos);

  /*
   *
   * normal exit
   *
   */


  DETATCHSTATUSPTR (status);
  RETURN (status);
}


void
XLALSortSimInspiral(
    SimInspiralTable **head,
    int (*comparefunc)(const SimInspiralTable * const *,
      const SimInspiralTable * const *)
    )

{
  INT4 i;
  INT4 length;
  SimInspiralTable *event;
  SimInspiralTable **array;

  /* empty list --> no-op */
  if(!head || !*head)
    return;

  /* count the number of events in the list */
  for(length = 0, event = *head; event; event = event->next)
    length++;

  /* construct an array of pointers into the list */
  array = LALCalloc(length, sizeof(*array));
  for(i = 0, event = *head; event; event = event->next)
    array[i++] = event;

  /* sort the array using the specified function */
  qsort(array, length, sizeof(*array),
      (int(*)(const void *, const void *)) comparefunc);

  /* re-link the list according to the sorted array */
  for(i = 0; i < length; i++, head = &(*head)->next)
    *head = array[i];
  *head = NULL;

  /* free the array */
  LALFree(array);
}


int
XLALCompareSimInspiralByGeocentEndTime(
    const SimInspiralTable * const *a,
    const SimInspiralTable * const *b
    )

{
  INT8 ta, tb;
  INT8 epsilon = 10;	/* nanoseconds */

  ta = geocent_end_time(*a);
  tb = geocent_end_time(*b);

  if(ta > tb + epsilon)
    return(1);
  if(ta < tb - epsilon)
    return(-1);
  return(0);
}
