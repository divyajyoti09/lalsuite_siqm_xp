/*
*  Copyright (C) 2007 Alexander Dietz, Drew Keppel, Duncan Brown, Eirini Messaritaki, Gareth Jones, George Birthisel, Jolien Creighton, Kipp Cannon, Lisa M. Goggin, Patrick Brady, Peter Shawhan, Saikat Ray-Majumder, Stephen Fairhurst, Xavier Siemens, Craig Robinson , Thomas Cokelaer
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
 * File Name: LIGOMetadataUtils.h
 *
 * Author: Brown, D. A. and Fairhurst, S.
 *
 *-----------------------------------------------------------------------
 */

/**
 * \author Brown, D. A. and Fairhurst, S.
 * \file
 * \ingroup lalmetaio_general
 * \brief Provides functions for manipulating the LAL structures that correspond
 * to the LIGO metadata database tables defined in \ref LIGOMetadataTables.h.
 *
 * ### Synopsis ###
 *
 * \code
 * #include <lal/LIGOMetadataUtils.h>
 * \endcode
 *
 * This header provides prototypes for routines that perform processing
 * on the LAL structures that correspond to the LIGO metadata database tables
 * defined in \ref LIGOMetadataTables.h, such as sorting and eliminating
 * duplictaes. The functions specific to a particular metadata table (e.g.
 * \c sngl_inspiral, \c sngl_burst, etc.) are all prototyped in
 * this header.
 *
 * ### Types ###
 *
 * None.
 *
 */

#ifndef _LIGOMETADATAUTILS_H
#define _LIGOMETADATAUTILS_H

#if defined(__cplusplus)
extern "C" {
#elif 0
} /* so that editors will match preceding brace */
#endif

#include <lal/LIGOMetadataTables.h>
#include <lal/LALDetectors.h>


/*
 *
 * general manipulation functions
 *
 */

ProcessTable *XLALCreateProcessTableRow(void);
#ifndef SWIG   // exclude from SWIG interface
ProcessTable *XLALDestroyProcessTableRow(ProcessTable *);
#endif   // SWIG
void XLALDestroyProcessTable(ProcessTable *);
long XLALProcessTableGetNextID(ProcessTable *);
int XLALPopulateProcessTable(
	ProcessTable *ptable,
	const char *program_name,
	const char *cvs_revision,
	const char *cvs_source,
	const char *cvs_date,
	long process_id
);

ProcessParamsTable *XLALCreateProcessParamsTableRow(const ProcessTable *);
#ifndef SWIG   // exclude from SWIG interface
ProcessParamsTable *XLALDestroyProcessParamsTableRow(ProcessParamsTable *);
#endif   // SWIG
void XLALDestroyProcessParamsTable(ProcessParamsTable *);

TimeSlide *XLALCreateTimeSlide(void);
#ifndef SWIG   // exclude from SWIG interface
TimeSlide *XLALDestroyTimeSlide(TimeSlide *);
#endif   // SWIG
void XLALDestroyTimeSlideTable(TimeSlide *);
const TimeSlide *XLALTimeSlideConstGetByIDAndInstrument(const TimeSlide *, long, const char *);
TimeSlide *XLALTimeSlideGetByIDAndInstrument(TimeSlide *, long, const char *);

SearchSummaryTable *XLALCreateSearchSummaryTableRow(const ProcessTable *);
#ifndef SWIG   // exclude from SWIG interface
SearchSummaryTable *XLALDestroySearchSummaryTableRow(SearchSummaryTable *);
#endif   // SWIG
void XLALDestroySearchSummaryTable(SearchSummaryTable *);

SegmentTable *XLALCreateSegmentTableRow(const ProcessTable *process);
#ifndef SWIG   // exclude from SWIG interface
SegmentTable *XLALDestroySegmentTableRow(SegmentTable *row);
#endif   // SWIG
void XLALDestroySegmentTable(SegmentTable *head);

SnglInspiralTable *XLALCreateSnglInspiralTableRow(const ProcessTable *process);
#ifndef SWIG   // exclude from SWIG interface
SnglInspiralTable *XLALDestroySnglInspiralTableRow(SnglInspiralTable *row);
#endif   // SWIG
void XLALDestroySnglInspiralTable(SnglInspiralTable *head);

SimInspiralTable *XLALCreateSimInspiralTableRow(const ProcessTable *process);
#ifndef SWIG   // exclude from SWIG interface
SimInspiralTable *XLALDestroySimInspiralTableRow(SimInspiralTable *row);
#endif   // SWIG
void XLALDestroySimInspiralTable(SimInspiralTable *head);

SnglRingdownTable *XLALCreateSnglRingdownTableRow(const ProcessTable *process);
#ifndef SWIG   // exclude from SWIG interface
SnglRingdownTable *XLALDestroySnglRingdownTableRow(SnglRingdownTable *row);
#endif   // SWIG
void XLALDestroySnglRingdownTable(SnglRingdownTable *head);

SimRingdownTable *XLALCreateSimRingdownTableRow(const ProcessTable *process);
#ifndef SWIG   // exclude from SWIG interface
SimRingdownTable *XLALDestroySimRingdownTableRow(SimRingdownTable *row);
#endif   // SWIG
void XLALDestroySimRingdownTable(SimRingdownTable *head);


SnglBurst *XLALCreateSnglBurst(void);
#ifndef SWIG   // exclude from SWIG interface
SnglBurst *XLALDestroySnglBurst(SnglBurst *event);
#endif   // SWIG
void XLALDestroySnglBurstTable(SnglBurst *head);

SimBurst *XLALCreateSimBurst(void);
#ifndef SWIG   // exclude from SWIG interface
SimBurst *XLALDestroySimBurst(SimBurst *row);
#endif   // SWIG
void XLALDestroySimBurstTable(SimBurst *head);

int
XLALCountProcessTable(
    ProcessTable *head
    );

int
XLALCountProcessParamsTable(
    ProcessParamsTable *head
    );

long
XLALSimInspiralAssignIDs (
    SimInspiralTable *head,
    long process_id,
    long simulation_id
    );

long
XLALSnglBurstAssignIDs(
    SnglBurst *head,
    long process_id,
    long event_id
    );

long
XLALSimBurstAssignIDs(
    SimBurst *head,
    long process_id,
    long time_slide_id,
    long event_id
    );

void
XLALReturnIFO(
    char                *ifo,
    InterferometerNumber IFONumber
    );

void
XLALReturnDetector(
    LALDetector           *det,
    InterferometerNumber   IFONumber
    );

int
XLALCompareSearchSummaryByOutTime (
    const void *a,
    const void *b
    );

int
XLALTimeSortSearchSummary(
    SearchSummaryTable  **summHead,
    int(*comparfunc)    (const void *, const void *)
    );

SearchSummaryTable *
XLALIfoScanSearchSummary(
    SearchSummaryTable         *input,
    CHAR                       *ifos
    );

#if 0
{ /* so that editors will match succeeding brace */
#elif defined(__cplusplus)
}
#endif

#endif /* _LIGOMETADATAUTILS_H */
