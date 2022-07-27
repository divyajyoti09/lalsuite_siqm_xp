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

#ifndef _CACHE_RESULTS_H
#define _CACHE_RESULTS_H

///
/// \file
/// \ingroup lalpulsar_bin_Weave
/// \brief Module which caches computed coherent results
///

#include "Weave.h"
#include "ComputeResults.h"
#include "SearchTiming.h"

#include <lal/LatticeTiling.h>
#include <lal/SuperskyMetrics.h>

#ifdef __cplusplus
extern "C" {
#endif

WeaveCacheQueries *XLALWeaveCacheQueriesCreate(
  const LatticeTiling *semi_tiling,
  const SuperskyTransformData *semi_rssky_transf,
  const double dfreq,
  const UINT4 nqueries,
  const UINT4 nfreq_partitions
  );
void XLALWeaveCacheQueriesDestroy(
  WeaveCacheQueries *queries
  );
int XLALWeaveCacheQueriesInit(
  WeaveCacheQueries *queries,
  const UINT8 semi_index,
  const gsl_vector *semi_rssky,
  const INT4 semi_left,
  const INT4 semi_right,
  const UINT4 freq_partition_index
  );
int XLALWeaveCacheQuery(
  const WeaveCache *cache,
  WeaveCacheQueries *queries,
  const UINT4 query_index
  );
int XLALWeaveCacheQueriesFinal(
  WeaveCacheQueries *queries,
  PulsarDopplerParams *semi_phys,
  UINT4 *semi_nfreqs
  );
int XLALWeaveCacheQueriesGetCounts(
  const WeaveCacheQueries *queries,
  UINT8 *coh_nres,
  UINT8 *coh_ntmpl,
  UINT8 *semi_ntmpl
  );
WeaveCache *XLALWeaveCacheCreate(
  const LatticeTiling *coh_tiling,
  const BOOLEAN interpolation,
  const SuperskyTransformData *coh_rssky_transf,
  const SuperskyTransformData *semi_rssky_transf,
  WeaveCohInput *coh_input,
  const UINT4 max_size,
  const BOOLEAN all_gc
  );
void XLALWeaveCacheDestroy(
  WeaveCache *cache
  );
int XLALWeaveGetCacheMeanMaxSize(
  REAL4 *cache_mean_max_size,
  const size_t ncache,
  WeaveCache *const *cache
  );
int XLALWeaveCacheWriteInfo(
  FITSFile *file,
  const size_t ncache,
  WeaveCache *const *cache
  );
int XLALWeaveCacheExpire(
  WeaveCache *cache
  );
int XLALWeaveCacheClear(
  WeaveCache *cache
  );
int XLALWeaveCacheRetrieve(
  WeaveCache *cache,
  const WeaveCacheQueries *queries,
  const UINT4 query_index,
  const WeaveCohResults **coh_res,
  UINT8 *coh_index,
  UINT4 *coh_offset,
  WeaveSearchTiming *tim
  );

#ifdef __cplusplus
}
#endif

#endif // _CACHE_RESULTS_H

// Local Variables:
// c-file-style: "linux"
// c-basic-offset: 2
// End:
