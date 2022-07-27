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

#ifndef _COMPUTE_RESULTS_H
#define _COMPUTE_RESULTS_H

///
/// \file
/// \ingroup lalpulsar_bin_Weave
/// \brief Module which computes coherent and semicoherent results
///

#include "Weave.h"
#include "Statistics.h"
#include "SearchTiming.h"

#include <lal/VectorMath.h>
#include <lal/ComputeFstat.h>

#ifdef __cplusplus
extern "C" {
#endif

///
/// Final results of a semicoherent computation over many segments
///
struct tagWeaveSemiResults {
  /// Bitflag representing search simulation level
  WeaveSimulationLevel simulation_level;
  /// Struct holding all parameters for which statistics to output and compute, when, and how
  const WeaveStatisticsParams *statistics_params;
  /// Number of detectors
  UINT4 ndetectors;
  /// Number of segments
  UINT4 nsegments;
  /// Frequency spacing for semicoherent results
  double dfreq;
  /// Number of frequencies
  UINT4 nfreqs;
  /// Per-segment coherent template index (optional)
  UINT8 *coh_index;
  /// Per-segment coherent template parameters of the first frequency bin (optional)
  PulsarDopplerParams *coh_phys;
  /// Per-segment multi-detector F-statistics per frequency (optional)
  const REAL4 **coh2F;
  /// Per-segment multi-detector F-statistics per frequency (optional), stored in CUDA device memory
  const REAL4 **coh2F_CUDA;
  /// Per-segment per-detector F-statistics per frequency (optional)
  const REAL4 **coh2F_det[PULSAR_MAX_DETECTORS];
  /// Number of coherent results processed thus far
  UINT4 ncoh_res;
  /// Semicoherent template index
  UINT8 semi_index;
  /// Semicoherent template parameters of the first frequency bin
  PulsarDopplerParams semi_phys;
  /// Maximized-over-segments multi-detector F-statistics per frequency
  REAL4VectorAligned *max2F;
  /// Maximized-over-segments per-detector F-statistics per frequency
  REAL4VectorAligned *max2F_det[PULSAR_MAX_DETECTORS];
  /// Summed multi-detector F-statistics per frequency
  REAL4VectorAligned *sum2F;
  /// Summed per-detector F-statistics per frequency
  REAL4VectorAligned *sum2F_det[PULSAR_MAX_DETECTORS];
  /// Mean multi-detector F-statistics per frequency
  REAL4VectorAligned *mean2F;
  /// Line-robust log10(B_S/GL) statistic
  REAL4VectorAligned *log10BSGL;
  /// Transient Line-robust log10(B_S/GLtL) statistic
  REAL4VectorAligned *log10BSGLtL;
  /// Transient Signal Line-robust log10(B_tS/GLtL) statistic
  REAL4VectorAligned *log10BtSGLtL;
};

WeaveCohInput *XLALWeaveCohInputCreate(
  const LALStringVector *setup_detectors,
  const WeaveSimulationLevel simulation_level,
  const SFTCatalog *sft_catalog,
  const UINT4 segment_index,
  const LALSeg *segment,
  const PulsarDopplerParams *min_phys,
  const PulsarDopplerParams *max_phys,
  const double dfreq,
  const EphemerisData *ephemerides,
  const LALStringVector *sft_noise_sqrtSX,
  const LALStringVector *Fstat_assume_sqrtSX,
  FstatOptionalArgs *Fstat_opt_args,
  WeaveStatisticsParams *statistics_params,
  BOOLEAN recalc_stage
  );
void XLALWeaveCohInputDestroy(
  WeaveCohInput *coh_input
  );
int XLALWeaveCohInputWriteInfo(
  FITSFile *file,
  const size_t ncoh_input,
  WeaveCohInput *const *coh_input
  );
int XLALWeaveCohInputWriteSegInfo(
  FITSFile *file,
  const size_t ncoh_input,
  WeaveCohInput *const *coh_input
  );
int XLALWeaveCohResultsCompute(
  WeaveCohResults **coh_res,
  WeaveCohInput *coh_input,
  const PulsarDopplerParams *coh_phys,
  const UINT4 coh_nfreqs,
  WeaveSearchTiming *tim
  );
void XLALWeaveCohResultsDestroy(
  WeaveCohResults *coh_res
  );
int XLALWeaveSemiResultsInit(
  WeaveSemiResults **semi_res,
  const WeaveSimulationLevel simulation_level,
  const UINT4 ndetectors,
  const UINT4 nsegments,
  const UINT8 semi_index,
  const PulsarDopplerParams *semi_phys,
  const double dfreq,
  const UINT4 semi_nfreqs,
  const WeaveStatisticsParams *statistics_params
  );
int XLALWeaveSemiResultsComputeSegs(
  WeaveSemiResults *semi_res,
  const UINT4 nsegments,
  const WeaveCohResults **coh_res,
  const UINT8 *coh_index,
  const UINT4 *coh_offset,
  WeaveSearchTiming *tim
  );
int XLALWeaveSemiResultsComputeMain(
  WeaveSemiResults *semi_res,
  WeaveSearchTiming *tim
  );
void XLALWeaveSemiResultsDestroy(
  WeaveSemiResults *semi_res
  );

int XLALWeaveCohResultsExtract(
  REAL4Vector **coh2F,
  REAL4Vector *coh2F_det[PULSAR_MAX_DETECTORS],
  BOOLEAN *have_coh2F_det,
  WeaveCohResults *coh_res,
  const WeaveCohInput *coh_input
  );
int XLALWeaveSemiCoh2FExtract(
  REAL4 *coh2F,
  const WeaveSemiResults *semi_res,
  const UINT4 freq_idx
  );

#ifdef __cplusplus
}
#endif

#endif // _COMPUTE_RESULTS_H

// Local Variables:
// c-file-style: "linux"
// c-basic-offset: 2
// End:
