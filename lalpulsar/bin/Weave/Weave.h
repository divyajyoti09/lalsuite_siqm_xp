//
// Copyright (C) 2016, 2017 Karl Wette
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with with program; see the file COPYING. If not, write to the
// Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
// MA 02110-1301 USA
//

#ifndef _WEAVE_H
#define _WEAVE_H

///
/// \defgroup lalpulsar_bin_Weave Weave Search Application
/// \ingroup lalpulsar_bin_Apps
/// \author Karl Wette
///

///
/// \file
/// \ingroup lalpulsar_bin_Weave
///

#include <stdlib.h>
#include <math.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>

#include <lal/LALStdlib.h>
#include <lal/LALStdio.h>
#include <lal/LALString.h>
#include <lal/PulsarDataTypes.h>
#include <lal/GSLHelpers.h>
#include <lal/FITSFileIO.h>
#include <lal/LALPulsarVCSInfo.h>

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tagWeaveSearchTimingSection WeaveSearchTimingSection;
typedef enum tagWeaveSimulationLevel WeaveSimulationLevel;
typedef enum tagWeaveStatisticType WeaveStatisticType;

typedef struct tagWeaveCache WeaveCache;
typedef struct tagWeaveCacheQueries WeaveCacheQueries;
typedef struct tagWeaveCohInput WeaveCohInput;
typedef struct tagWeaveCohResults WeaveCohResults;
typedef struct tagWeaveOutputResults WeaveOutputResults;
typedef struct tagWeaveResultsToplist WeaveResultsToplist;
typedef struct tagWeaveResultsToplistItem WeaveResultsToplistItem;
typedef struct tagWeaveSearchIterator WeaveSearchIterator;
typedef struct tagWeaveSearchTiming WeaveSearchTiming;
typedef struct tagWeaveSemiResults WeaveSemiResults;
typedef struct tagWeaveSetupData WeaveSetupData;
typedef struct tagWeaveStatisticsParams WeaveStatisticsParams;
typedef struct tagWeaveStatisticsValues WeaveStatisticsValues;

///
/// Bitflags representing search simulation levels
///
enum tagWeaveSimulationLevel {
  /// Simulate search (implicitly with full memory allocation)
  WEAVE_SIMULATE                        = 0001,
  /// Simulate search with minimal memory allocation
  WEAVE_SIMULATE_MIN_MEM                = 0002,
};

#ifdef __cplusplus
}
#endif

#endif // _WEAVE_H

// Local Variables:
// c-file-style: "linux"
// c-basic-offset: 2
// End:
