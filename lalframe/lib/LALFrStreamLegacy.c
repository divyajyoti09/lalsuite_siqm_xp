/*
*  Copyright (C) 2007 Duncan Brown, Jolien Creighton, Kipp Cannon
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

/* LEGACY CODE */

#include <math.h>
#include <stdio.h>
#include <lal/LALStdlib.h>
#include <lal/Date.h>
#include <lal/LALCache.h>
#include <lal/LALFrStream.h>

void
LALFrCacheOpen(LALStatus * status, LALFrStream ** output, LALCache * cache)
{
    LALFrStream *stream;

    XLAL_PRINT_DEPRECATION_WARNING("XLALFrStreamCacheOpen");
    INITSTATUS(status);
    ASSERT(cache, status, FRAMESTREAMH_ENULL, FRAMESTREAMH_MSGENULL);
    ASSERT(output, status, FRAMESTREAMH_ENULL, FRAMESTREAMH_MSGENULL);
    ASSERT(!*output, status, FRAMESTREAMH_ENNUL, FRAMESTREAMH_MSGENNUL);

    stream = *output = XLALFrStreamCacheOpen(cache);
    if (!stream) {
        int errnum = xlalErrno;
        XLALClearErrno();
        switch (errnum) {
        case XLAL_ENOMEM:
            ABORT(status, FRAMESTREAMH_EALOC, FRAMESTREAMH_MSGEALOC);
        case XLAL_EIO:
            ABORT(status, FRAMESTREAMH_EOPEN, FRAMESTREAMH_MSGEOPEN);
        default:
            ABORTXLAL(status);
        }
    }

    RETURN(status);
}

void
LALFrOpen(LALStatus * status,
    LALFrStream ** stream, const CHAR * dirname, const CHAR * pattern)
{
    XLAL_PRINT_DEPRECATION_WARNING("XLALFrStreamOpen");
    INITSTATUS(status);
    ATTATCHSTATUSPTR(status);
    ASSERT(stream, status, FRAMESTREAMH_ENULL, FRAMESTREAMH_MSGENULL);
    ASSERT(!*stream, status, FRAMESTREAMH_ENNUL, FRAMESTREAMH_MSGENNUL);

    *stream = XLALFrStreamOpen(dirname, pattern);

    DETATCHSTATUSPTR(status);
    RETURN(status);
}

void LALFrClose(LALStatus * status, LALFrStream ** stream)
{
    XLAL_PRINT_DEPRECATION_WARNING("XLALFrStreamClose");
    INITSTATUS(status);
    ASSERT(stream, status, FRAMESTREAMH_ENULL, FRAMESTREAMH_MSGENULL);
    ASSERT(*stream, status, FRAMESTREAMH_ENULL, FRAMESTREAMH_MSGENULL);
    XLALFrStreamClose(*stream);
    *stream = NULL;
    RETURN(status);
}

void LALFrSeek(LALStatus * status, const LIGOTimeGPS * epoch,
    LALFrStream * stream)
{
    CHAR frErrMsg[1024];
    int code;

    XLAL_PRINT_DEPRECATION_WARNING("XLALFrStreamSeek");
    INITSTATUS(status);
    ASSERT(stream, status, FRAMESTREAMH_ENULL, FRAMESTREAMH_MSGENULL);
    ASSERT(epoch, status, FRAMESTREAMH_ENULL, FRAMESTREAMH_MSGENULL);
    if (stream->state & LAL_FR_STREAM_ERR) {
        ABORT(status, FRAMESTREAMH_ERROR, FRAMESTREAMH_MSGERROR);
    }

    code = XLALFrStreamSeek(stream, epoch);
    if (code < 0) {
        XLALClearErrno();
        if (stream->state & LAL_FR_STREAM_ERR) {        /* a file error */
            if (stream->state & LAL_FR_STREAM_URL) {    /* must have failed to open a file */
                snprintf(frErrMsg, XLAL_NUM_ELEM(frErrMsg),
                    "Could not open URL %s\n",
                    stream->cache->list[stream->fnum].url);
                LALError(status, frErrMsg);
                ABORT(status, FRAMESTREAMH_EOPEN, FRAMESTREAMH_MSGEOPEN);
            }
            if (stream->state & LAL_FR_STREAM_TOC) {    /* must have failed to read a file */
                snprintf(frErrMsg, XLAL_NUM_ELEM(frErrMsg),
                    "Could not read TOC from %s\n",
                    stream->cache->list[stream->fnum].url);
                LALError(status, frErrMsg);
                ABORT(status, FRAMESTREAMH_EREAD, FRAMESTREAMH_MSGEREAD);
            }
        } else {        /* must be too early, too late, or in a gap */

            ABORT(status, FRAMESTREAMH_ETREQ, FRAMESTREAMH_MSGETREQ);
        }
    }

    RETURN(status);
}

/* GET SERIES FUNCTIONS */

#define DEFINE_LAL_GET_TS_FUNCTION(laltype) \
    void LALFrGet ## laltype ## TimeSeries(LALStatus *status, laltype ## TimeSeries *series, FrChanIn *chanin, LALFrStream *stream) \
    { \
        int errnum; \
        int code; \
        XLAL_PRINT_DEPRECATION_WARNING("XLALFrStreamGet" #laltype "TimeSeries"); \
        INITSTATUS(status); \
        strcpy(series->name, chanin->name); \
        XLAL_TRY(code = XLALFrStreamGet ## laltype ## TimeSeries(series, stream), errnum); \
        if ((code < 0) || errnum) { \
            if (stream->state & LAL_FR_STREAM_END) { \
                ABORT(status, FRAMESTREAMH_EDONE, FRAMESTREAMH_MSGEDONE); \
            } \
            ABORT(status, FRAMESTREAMH_EREAD, FRAMESTREAMH_MSGEREAD); \
        } \
        RETURN(status); \
    }

#define DEFINE_LAL_GET_TSM_FUNCTION(laltype) \
    void LALFrGet ## laltype ## TimeSeriesMetadata(LALStatus *status, laltype ## TimeSeries *series, FrChanIn *chanin, LALFrStream *stream) \
    { \
        int code; \
        XLAL_PRINT_DEPRECATION_WARNING("XLALFrStreamGet" #laltype "TimeSeriesMetadata"); \
        INITSTATUS(status); \
        strcpy(series->name, chanin->name); \
        code = XLALFrStreamGet ## laltype ## TimeSeriesMetadata(series, stream); \
        if (code < 0) { \
            ABORT(status, FRAMESTREAMH_EREAD, FRAMESTREAMH_MSGEREAD); \
        } \
        RETURN(status); \
    }

#define DEFINE_LAL_GET_FS_FUNCTION(laltype) \
    void LALFrGet ## laltype ## FrequencySeries(LALStatus *status, laltype ## FrequencySeries *series, FrChanIn *chanin, LALFrStream *stream) \
    { \
        int code; \
        XLAL_PRINT_DEPRECATION_WARNING("XLALFrStreamGet" #laltype "FrequencySeries"); \
        INITSTATUS(status); \
        strcpy(series->name, chanin->name); \
        code = XLALFrStreamGet ## laltype ## FrequencySeries(series, stream); \
        if (code < 0) { \
            ABORT(status, FRAMESTREAMH_EREAD, FRAMESTREAMH_MSGEREAD); \
        } \
        RETURN(status); \
    }

/* *INDENT-OFF* */
DEFINE_LAL_GET_TS_FUNCTION(INT2)
DEFINE_LAL_GET_TS_FUNCTION(INT4)
DEFINE_LAL_GET_TS_FUNCTION(INT8)
DEFINE_LAL_GET_TS_FUNCTION(REAL4)
DEFINE_LAL_GET_TS_FUNCTION(REAL8)
DEFINE_LAL_GET_TS_FUNCTION(COMPLEX8)

DEFINE_LAL_GET_TSM_FUNCTION(INT2)
DEFINE_LAL_GET_TSM_FUNCTION(INT4)
DEFINE_LAL_GET_TSM_FUNCTION(INT8)
DEFINE_LAL_GET_TSM_FUNCTION(REAL4)
DEFINE_LAL_GET_TSM_FUNCTION(REAL8)
DEFINE_LAL_GET_TSM_FUNCTION(COMPLEX8)

DEFINE_LAL_GET_FS_FUNCTION(COMPLEX8)
/* *INDENT-ON* */

/* WRITE SERIES FUNCTIONS */

/* FIXME: now only supports one frame per file and ProcData channels only */
#define DEFINE_LAL_WRITE_TS_FUNCTION(laltype) \
    void LALFrWrite ## laltype ## TimeSeries(LALStatus *status, laltype ## TimeSeries *series, FrOutPar *params) \
    { \
        LALFrameH *frame; \
        char fname[FILENAME_MAX]; \
        double duration; \
        int t0, dt; \
        XLAL_PRINT_DEPRECATION_WARNING("XLALFrWrite" #laltype "TimeSeries"); \
        INITSTATUS(status); \
        duration = series->deltaT * series->data->length; \
        t0 = series->epoch.gpsSeconds; \
        dt = (int)ceil(XLALGPSGetREAL8(&series->epoch)+duration) - t0; \
        snprintf(fname, sizeof(fname), "%s-%s-%d-%d.gwf", \
             params->source ? params->source : "F", \
             params->description ? params->description : "UNKNOWN", \
             t0, dt); \
        frame = XLALFrameNew(&series->epoch, duration, "LAL", params->run, params->frame, 0); \
        XLALFrameAdd ## laltype ## TimeSeriesProcData(frame, series); \
        XLALFrameWrite(frame, fname); \
        XLALFrameFree(frame); \
        RETURN(status); \
    }

/* *INDENT-OFF* */
DEFINE_LAL_WRITE_TS_FUNCTION(INT4)
DEFINE_LAL_WRITE_TS_FUNCTION(REAL4)
/* *INDENT-ON* */
