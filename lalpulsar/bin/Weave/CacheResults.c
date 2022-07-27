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

///
/// \file
/// \ingroup lalpulsar_bin_Weave
///

#include "CacheResults.h"

#include <lal/LALHeap.h>
#include <lal/LALHashTbl.h>
#include <lal/LALBitset.h>

// Compare two quantities, and return a sort order value if they are unequal
#define COMPARE_BY( x, y ) do { if ( (x) < (y) ) return -1; if ( (x) > (y) ) return +1; } while(0)

///
/// Item stored in the cache
///
typedef struct {
  /// Generation, used both to find items in cache and to decide how long to keep items
  UINT4 generation;
  /// Relevance, used to decide how long to keep items
  REAL4 relevance;
  /// Coherent locator index, used to find items in cache
  UINT8 coh_index;
  /// Results of a coherent computation on a single segment
  WeaveCohResults *coh_res;
} cache_item;

///
/// Container for a series of cache queries
///
struct tagWeaveCacheQueries {
  /// Number of parameter-space dimensions
  size_t ndim;
  /// Lowest tiled parameter-space dimension
  size_t dim0;
  // Frequency spacing used by lattices
  double dfreq;
  /// Number of queries for which space is allocated
  UINT4 nqueries;
  /// Number of partitions to divide semicoherent frequency block into
  UINT4 nfreq_partitions;
  /// Index to current partition of semicoherent frequency block
  UINT4 freq_partition_index;
  /// Offset to apply to coherent left-most index to enclose a frequency partition
  INT4 *coh_part_left_offset;
  /// Offset to apply to coherent right-most index to enclose a frequency partition
  INT4 *coh_part_right_offset;
  /// Sequential indexes for each queried coherent frequency block
  UINT8 *coh_index;
  /// Physical points of each queried coherent frequency block
  PulsarDopplerParams *coh_phys;
  /// Indexes of left-most point in queried coherent frequency block
  INT4 *coh_left;
  /// Indexes of right-most point in queried coherent frequency block
  INT4 *coh_right;
  /// Relevance of each queried coherent frequency block
  REAL4 *coh_relevance;
  /// Number of computed coherent results (per query)
  UINT8 *coh_nres;
  /// Number of coherent templates (per query)
  UINT8 *coh_ntmpl;
  /// Reduced supersky transform data for semicoherent lattice
  const SuperskyTransformData *semi_rssky_transf;
  /// Sequential index for the current semicoherent frequency block
  UINT8 semi_index;
  // Current semicoherent frequency block in dimension 'dim0'
  double semi_rssky_dim0;
  /// Physical coordinates of the current semicoherent frequency block
  PulsarDopplerParams semi_phys;
  /// Index of left-most point in current semicoherent frequency block
  INT4 semi_left;
  /// Index of right-most point in current semicoherent frequency block
  INT4 semi_right;
  /// Relevance of the current semicoherent frequency block
  REAL4 semi_relevance;
  /// Offset used in computation of semicoherent point relevance
  double semi_relevance_offset;
  /// Number of semicoherent templates (over all queries)
  UINT8 semi_ntmpl;
};

///
/// Cache used to store coherent results
///
struct tagWeaveCache {
  /// Number of parameter-space dimensions
  size_t ndim;
  /// Lowest tiled parameter-space dimension
  size_t dim0;
  /// Reduced supersky transform data for coherent lattice
  const SuperskyTransformData *coh_rssky_transf;
  /// Reduced supersky transform data for semicoherent lattice
  const SuperskyTransformData *semi_rssky_transf;
  /// Input data required for computing coherent results
  WeaveCohInput *coh_input;
  /// Coherent parameter-space tiling locator
  LatticeTilingLocator *coh_locator;
  /// Maximum value of index from coherent locator
  UINT8 coh_max_index;
  /// Current generation of cache items
  UINT4 generation;
  /// Heap which ranks cache items by relevance
  LALHeap *relevance_heap;
  /// Maximum size obtained by relevance heap
  UINT4 heap_max_size;
  /// Hash table which looks up cache items by index
  LALHashTbl *coh_index_hash;
  /// Bitset which records whether an item has ever been computed
  LALBitset *coh_computed_bitset;
  /// Offset used in computation of coherent point relevance
  double coh_relevance_offset;
  /// Whether any garbage collection of results should be used
  BOOLEAN any_gc;
  /// Whether garbage collection should remove as many results as possible
  BOOLEAN all_gc;
  /// Save an no-longer-used cache item for re-use
  cache_item *saved_item;
};

///
/// \name Internal functions
///
/// @{

static int cache_left_right_offsets( const UINT4 semi_nfreqs, const UINT4 nfreq_partitions, const UINT4 freq_partition_index, INT4 *left_offset, INT4 *right_offset );
static int cache_max_semi_bbox_sample_dim0( const WeaveCache *cache, const LatticeTiling *coh_tiling, const size_t i, gsl_vector *coh_bbox_sample, gsl_vector *semi_bbox_sample, double *max_semi_bbox_sample_dim0 );
static UINT8 cache_item_hash( const void *x );
static int cache_item_compare_by_coh_index( const void *x, const void *y );
static int cache_item_compare_by_relevance( const void *x, const void *y );
static void cache_item_destroy( void *x );

/// @}

///
/// Destroy a cache item
///
void cache_item_destroy(
  void *x
  )
{
  if ( x != NULL ) {
    cache_item *ix = ( cache_item * ) x;
    XLALWeaveCohResultsDestroy( ix->coh_res );
    XLALFree( ix );
  }
}

///
/// Compare cache items by generation, then relevance
///
int cache_item_compare_by_relevance(
  const void *x,
  const void *y
  )
{
  const cache_item *ix = ( const cache_item * ) x;
  const cache_item *iy = ( const cache_item * ) y;
  COMPARE_BY( ix->generation, iy->generation );   // Compare in ascending order
  COMPARE_BY( ix->relevance, iy->relevance );   // Compare in ascending order
  return 0;
}

///
/// Compare cache items by generation, then locator index
///
int cache_item_compare_by_coh_index(
  const void *x,
  const void *y
  )
{
  const cache_item *ix = ( const cache_item * ) x;
  const cache_item *iy = ( const cache_item * ) y;
  COMPARE_BY( ix->generation, iy->generation );   // Compare in ascending order
  COMPARE_BY( ix->coh_index, iy->coh_index );   // Compare in ascending order
  return 0;
}

///
/// Hash cache items by generation and locator index
///
UINT8 cache_item_hash(
  const void *x
  )
{
  const cache_item *ix = ( const cache_item * ) x;
  UINT4 hval = 0;
  XLALPearsonHash( &hval, sizeof( hval ), &ix->generation, sizeof( ix->generation ) );
  XLALPearsonHash( &hval, sizeof( hval ), &ix->coh_index, sizeof( ix->coh_index ) );
  return hval;
}

///
/// Sample points on surface of coherent bounding box, convert to semicoherent supersky
/// coordinates, and record maximum value of semicoherent coordinate in dimension 'dim0'
///
int cache_max_semi_bbox_sample_dim0(
  const WeaveCache *cache,
  const LatticeTiling *coh_tiling,
  const size_t i,
  gsl_vector *coh_bbox_sample,
  gsl_vector *semi_bbox_sample,
  double *max_semi_bbox_sample_dim0
  )
{

  if ( i < cache->ndim ) {

    // Get coherent lattice tiling bounding box in dimension 'i'
    const double coh_bbox_i = XLALLatticeTilingBoundingBox( coh_tiling, i );
    XLAL_CHECK( xlalErrno == 0, XLAL_EFUNC );

    // Get current value of 'coh_bbox_sample' in dimension 'i'
    const double coh_bbox_sample_i = gsl_vector_get( coh_bbox_sample, i );

    // Move 'coh_bbox_sample' in dimension 'i' to vertices, edge centres and face centres of bounding box, and proceed to higher dimensions
    for ( int step = -1; step <= 1; ++ step ) {
      gsl_vector_set( coh_bbox_sample, i, coh_bbox_sample_i - step * 0.5 * coh_bbox_i );
      XLAL_CHECK( cache_max_semi_bbox_sample_dim0( cache, coh_tiling, i + 1, coh_bbox_sample, semi_bbox_sample, max_semi_bbox_sample_dim0 ) == XLAL_SUCCESS, XLAL_EFUNC );
    }

    // Restore current value of 'coh_bbox_sample' in dimension 'i'
    gsl_vector_set( coh_bbox_sample, i, coh_bbox_sample_i );

  } else {

    // Convert 'coh_bbox_sample' to semicoherent reduced supersky coordinates
    XLAL_CHECK( XLALConvertSuperskyToSuperskyPoint( semi_bbox_sample, cache->semi_rssky_transf, coh_bbox_sample, coh_bbox_sample, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EINVAL );

    // Record maximum value of semicoherent coordinate in dimension 'dim0'
    *max_semi_bbox_sample_dim0 = GSL_MAX( *max_semi_bbox_sample_dim0, gsl_vector_get( semi_bbox_sample, cache->dim0 ) );

  }

  return XLAL_SUCCESS;

}

///
/// Compute left/right-most index offsets to select a given partition
///
int cache_left_right_offsets(
  const UINT4 semi_nfreqs,
  const UINT4 nfreq_partitions,
  const UINT4 freq_partition_index,
  INT4 *left_offset,
  INT4 *right_offset
  )
{

  // Minimum number of points in the current frequency partition
  const UINT4 min_part_nfreqs = semi_nfreqs / nfreq_partitions;

  // Excess number of points which must be added to get 'semi_nfreqs' in total
  INT4 excess_nfreqs = semi_nfreqs - nfreq_partitions * min_part_nfreqs;

  // Initialise number of points in this frequency partition
  // - If there are excess points, add an extra point
  INT4 part_nfreqs = min_part_nfreqs;
  if ( excess_nfreqs > 0 ) {
    ++part_nfreqs;
  }

  // Compute offsets to left/right-most indexes to the select each given partition
  // - Add 'part_nfreqs' to '*left_offset' for each increase in partition index
  // - Decrease number of excess points; if zero, subtract extra point from 'part_nfreqs'
  *left_offset = 0;
  for ( size_t i = 0; i < freq_partition_index; ++i ) {
    *left_offset += part_nfreqs;
    --excess_nfreqs;
    if ( excess_nfreqs == 0 ) {
      --part_nfreqs;
    }
  }
  *right_offset = *left_offset + part_nfreqs - semi_nfreqs;
  XLAL_CHECK( *left_offset >= 0, XLAL_EDOM );
  XLAL_CHECK( *right_offset <= 0, XLAL_EDOM );

  return XLAL_SUCCESS;

}

///
/// Create storage for a series of cache queries
///
WeaveCacheQueries *XLALWeaveCacheQueriesCreate(
  const LatticeTiling *semi_tiling,
  const SuperskyTransformData *semi_rssky_transf,
  const double dfreq,
  const UINT4 nqueries,
  const UINT4 nfreq_partitions
  )
{

  // Check input
  XLAL_CHECK_NULL( semi_tiling != NULL, XLAL_EFAULT );
  XLAL_CHECK_NULL( dfreq >= 0, XLAL_EINVAL );
  XLAL_CHECK_NULL( nqueries > 0, XLAL_EINVAL );
  XLAL_CHECK_NULL( nfreq_partitions > 0, XLAL_EINVAL );

  // Allocate memory
  WeaveCacheQueries *queries = XLALCalloc( 1, sizeof( *queries ) );
  XLAL_CHECK_NULL( queries != NULL, XLAL_ENOMEM );
  queries->coh_part_left_offset = XLALCalloc( nfreq_partitions, sizeof( *queries->coh_part_left_offset ) );
  XLAL_CHECK_NULL( queries->coh_part_left_offset != NULL, XLAL_ENOMEM );
  queries->coh_part_right_offset = XLALCalloc( nfreq_partitions, sizeof( *queries->coh_part_right_offset ) );
  XLAL_CHECK_NULL( queries->coh_part_right_offset != NULL, XLAL_ENOMEM );
  queries->coh_index = XLALCalloc( nqueries, sizeof( *queries->coh_index ) );
  XLAL_CHECK_NULL( queries->coh_index != NULL, XLAL_ENOMEM );
  queries->coh_phys = XLALCalloc( nqueries, sizeof( *queries->coh_phys ) );
  XLAL_CHECK_NULL( queries->coh_phys != NULL, XLAL_ENOMEM );
  queries->coh_left = XLALCalloc( nqueries, sizeof( *queries->coh_left ) );
  XLAL_CHECK_NULL( queries->coh_left != NULL, XLAL_ENOMEM );
  queries->coh_right = XLALCalloc( nqueries, sizeof( *queries->coh_right ) );
  XLAL_CHECK_NULL( queries->coh_right != NULL, XLAL_ENOMEM );
  queries->coh_relevance = XLALCalloc( nqueries, sizeof( *queries->coh_relevance ) );
  XLAL_CHECK_NULL( queries->coh_relevance != NULL, XLAL_ENOMEM );
  queries->coh_nres = XLALCalloc( nqueries, sizeof( *queries->coh_nres ) );
  XLAL_CHECK_NULL( queries->coh_nres != NULL, XLAL_ENOMEM );
  queries->coh_ntmpl = XLALCalloc( nqueries, sizeof( *queries->coh_ntmpl ) );
  XLAL_CHECK_NULL( queries->coh_ntmpl != NULL, XLAL_ENOMEM );

  // Set fields
  queries->semi_rssky_transf = semi_rssky_transf;
  queries->dfreq = dfreq;
  queries->nqueries = nqueries;
  queries->nfreq_partitions = nfreq_partitions;

  // Get number of parameter-space dimensions
  queries->ndim = XLALTotalLatticeTilingDimensions( semi_tiling );
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Get lowest tiled parameter-space dimension
  if ( XLALTiledLatticeTilingDimensions( semi_tiling ) > 0 ) {
    queries->dim0 = XLALLatticeTilingTiledDimension( semi_tiling, 0 );
  } else {
    queries->dim0 = 0;
  }
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Compute offset used in computation of semicoherent point relevance:
  // - Negative half of semicoherent lattice tiling bounding box in dimension 'dim0'
  queries->semi_relevance_offset = -0.5 * XLALLatticeTilingBoundingBox( semi_tiling, queries->dim0 );
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Minimum number of points in a semicoherent frequency block
  const LatticeTilingStats *stats = XLALLatticeTilingStatistics( semi_tiling, queries->ndim - 1 );
  XLAL_CHECK_NULL( stats != NULL, XLAL_EFUNC );
  XLAL_CHECK_NULL( stats->min_points > 0, XLAL_EFAILED );

  // Compute offsets to coherent left/right-most indexes to enclose each frequency partition
  // - Use 'semi_min_freqs' to make offsets less than or equal to any semicoherent offsets
  for ( size_t i = 0; i < queries->nfreq_partitions; ++i ) {
    XLAL_CHECK_NULL( cache_left_right_offsets( stats->min_points, queries->nfreq_partitions, i, &queries->coh_part_left_offset[i], &queries->coh_part_right_offset[i] ) == XLAL_SUCCESS, XLAL_EFUNC );
  }

  return queries;

}

///
/// Destroy storage for a series of cache queries
///
void XLALWeaveCacheQueriesDestroy(
  WeaveCacheQueries *queries
  )
{
  if ( queries != NULL ) {
    XLALFree( queries->coh_part_left_offset );
    XLALFree( queries->coh_part_right_offset );
    XLALFree( queries->coh_index );
    XLALFree( queries->coh_phys );
    XLALFree( queries->coh_left );
    XLALFree( queries->coh_right );
    XLALFree( queries->coh_relevance );
    XLALFree( queries->coh_nres );
    XLALFree( queries->coh_ntmpl );
    XLALFree( queries );
  }
}

///
/// Initialise a series of cache queries
///
int XLALWeaveCacheQueriesInit(
  WeaveCacheQueries *queries,
  const UINT8 semi_index,
  const gsl_vector *semi_rssky,
  const INT4 semi_left,
  const INT4 semi_right,
  const UINT4 freq_partition_index
  )
{

  // Check input
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( semi_rssky != NULL, XLAL_EFAULT );
  XLAL_CHECK( freq_partition_index < queries->nfreq_partitions, XLAL_EINVAL );

  // Reset coherent sequential indexes to zero, indicating no query
  memset( queries->coh_index, 0, queries->nqueries * sizeof( queries->coh_index[0] ) );

  // Save current semicoherent sequential index
  queries->semi_index = semi_index;

  // Save current semicoherent frequency block in dimension 'dim0'
  queries->semi_rssky_dim0 = gsl_vector_get( semi_rssky, queries->dim0 );

  // Convert semicoherent point to physical coordinates
  XLAL_CHECK( XLALConvertSuperskyToPhysicalPoint( &queries->semi_phys, semi_rssky, NULL, queries->semi_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Save indexes of left/right-most point in current semicoherent frequency block
  queries->semi_left = semi_left;
  queries->semi_right = semi_right;

  // Save index to current partition of semicoherent frequency block
  queries->freq_partition_index = freq_partition_index;

  // Compute the relevance of the current semicoherent frequency block
  // - Relevance is semicoherent reduced supersky point in dimension 'dim0', plus offset
  queries->semi_relevance = gsl_vector_get( semi_rssky, queries->dim0 ) + queries->semi_relevance_offset;

  return XLAL_SUCCESS;

}

///
/// Query a cache for the results nearest to a given coherent point
///
int XLALWeaveCacheQuery(
  const WeaveCache *cache,
  WeaveCacheQueries *queries,
  const UINT4 query_index
  )
{

  // Check input
  XLAL_CHECK( cache != NULL, XLAL_EFAULT );
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( cache->ndim == queries->ndim, XLAL_ESIZE );
  XLAL_CHECK( cache->dim0 == queries->dim0, XLAL_ESIZE );
  XLAL_CHECK( query_index < queries->nqueries, XLAL_EINVAL );

  // Convert semicoherent physical point to coherent reduced supersky coordinates
  double coh_point_array[cache->ndim];
  gsl_vector_view coh_point_view = gsl_vector_view_array( coh_point_array, cache->ndim );
  XLAL_CHECK( XLALConvertPhysicalToSuperskyPoint( &coh_point_view.vector, &queries->semi_phys, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Initialise nearest point to semicoherent point in coherent reduced supersky coordinates
  double coh_near_point_array[cache->ndim];
  gsl_vector_view coh_near_point_view = gsl_vector_view_array( coh_near_point_array, cache->ndim );
  gsl_vector_memcpy( &coh_near_point_view.vector, &coh_point_view.vector );

  // Initialise values for a non-interpolating search
  queries->coh_index[query_index] = queries->semi_index;
  queries->coh_left[query_index] = queries->semi_left;
  queries->coh_right[query_index] = queries->semi_right;

  // If performing an interpolating search, find the nearest coherent frequency pass in this segment
  // - 'coh_near_point' is set to the nearest point to the mid-point of the semicoherent frequency block
  // - 'coh_index' is set to the index of this coherent frequency block, used for cache lookup
  // - 'coh_left' and 'coh_right' are set of number of points to the left/right of 'coh_near_point';
  //   check that these are sufficient to contain the number of points to the left/right of 'semi_rssky'
  if ( cache->coh_locator != NULL ) {
    XLAL_CHECK( XLALNearestLatticeTilingBlock( cache->coh_locator, &coh_point_view.vector, cache->ndim - 1, &coh_near_point_view.vector, &queries->coh_index[query_index], &queries->coh_left[query_index], &queries->coh_right[query_index] ) == XLAL_SUCCESS, XLAL_EFUNC );
    XLAL_CHECK( queries->coh_index[query_index] < cache->coh_max_index, XLAL_EFAILED, "Coherent index %"LAL_UINT8_FORMAT" out of range [0,%"LAL_UINT8_FORMAT") at semicoherent index %"LAL_UINT8_FORMAT", query index %u", queries->coh_index[query_index], cache->coh_max_index, queries->semi_index, query_index );
    XLAL_CHECK( queries->coh_left[query_index] <= queries->coh_right[query_index], XLAL_EINVAL );
    XLAL_CHECK( queries->coh_left[query_index] <= queries->semi_left && queries->semi_right <= queries->coh_right[query_index], XLAL_EFAILED, "Range of coherent tiling [%i,%i] does not contain semicoherent tiling [%i,%i] at semicoherent index %"LAL_UINT8_FORMAT", query index %u", queries->coh_left[query_index], queries->coh_right[query_index], queries->semi_left, queries->semi_right, queries->semi_index, query_index );
  }

  // Make 'coh_index' a 1-based index, so zero can be used to indicate a missing query
  ++queries->coh_index[query_index];

  // Convert nearest coherent point to physical coordinates
  XLAL_INIT_MEM( queries->coh_phys[query_index] );
  XLAL_CHECK( XLALConvertSuperskyToPhysicalPoint( &queries->coh_phys[query_index], &coh_near_point_view.vector, &coh_point_view.vector, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Compute the relevance of the current coherent frequency block:
  // - Convert nearest coherent point to semicoherent reduced supersky coordinates
  // - Relevance is nearest coherent point in semicoherent reduced supersky coordinates in dimension 'dim0', plus offset
  {
    double semi_near_point_array[cache->ndim];
    gsl_vector_view semi_near_point_view = gsl_vector_view_array( semi_near_point_array, cache->ndim );
    XLAL_CHECK( XLALConvertSuperskyToSuperskyPoint( &semi_near_point_view.vector, cache->semi_rssky_transf, &coh_near_point_view.vector, &coh_point_view.vector, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EINVAL );
    const double semi_near_point_dim0 = gsl_vector_get( &semi_near_point_view.vector, cache->dim0 );
    queries->coh_relevance[query_index] = GSL_MAX( semi_near_point_dim0, queries->semi_rssky_dim0 ) + cache->coh_relevance_offset;
  }

  return XLAL_SUCCESS;

}

///
/// Finalise a series of cache queries
///
int XLALWeaveCacheQueriesFinal(
  WeaveCacheQueries *queries,
  PulsarDopplerParams *semi_phys,
  UINT4 *semi_nfreqs
  )
{

  // Check input
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( semi_phys != NULL, XLAL_EFAULT );
  XLAL_CHECK( semi_nfreqs != NULL, XLAL_EFAULT );

  // Check that every 'coh_index' is at least 1, i.e. a query was made
  for ( size_t i = 0; i < queries->nqueries; ++i ) {
    XLAL_CHECK( queries->coh_index[i] > 0, XLAL_EINVAL, "Missing query at index %zu", i );
  }

  // Compute the total number of points in the semicoherent frequency block
  *semi_nfreqs = queries->semi_right - queries->semi_left + 1;

  // Compute offsets to semicoherent left/right-most indexes to select each frequency partition
  INT4 semi_part_left_offset = 0, semi_part_right_offset = 0;
  XLAL_CHECK( cache_left_right_offsets( *semi_nfreqs, queries->nfreq_partitions, queries->freq_partition_index, &semi_part_left_offset, &semi_part_right_offset ) == XLAL_SUCCESS, XLAL_EFUNC );
  XLAL_CHECK( queries->coh_part_left_offset[queries->freq_partition_index] <= semi_part_left_offset && semi_part_right_offset <= queries->coh_part_right_offset[queries->freq_partition_index], XLAL_EFAILED );

  // Adjust semicoherent left/right-most indexes to select the given partition
  // - If there are fewer points in the semicoherent frequency block than partitions,
  //   then some partitions will have 'semi_right' < 'semi_left', i.e. no points. In
  //   which set '*semi_nfreqs' to zero indicates that this partition should be skipped
  //   for this semicoherent frequency block
  queries->semi_left += semi_part_left_offset;
  queries->semi_right += semi_part_right_offset;
  if ( queries->semi_right < queries->semi_left ) {
    *semi_nfreqs = 0;
    return XLAL_SUCCESS;
  }

  // Compute the total number of points in the semicoherent frequency block partition
  *semi_nfreqs = queries->semi_right - queries->semi_left + 1;

  // Adjust coherent left/right-most indexes to enclose the given partition
  for ( size_t i = 0; i < queries->nqueries; ++i ) {
    queries->coh_left[i] += queries->coh_part_left_offset[queries->freq_partition_index];
    queries->coh_right[i] += queries->coh_part_right_offset[queries->freq_partition_index];
  }

  // Shift physical frequencies to first point in coherent/semicoherent frequency block partition
  queries->semi_phys.fkdot[0] += queries->dfreq * queries->semi_left;
  for ( size_t i = 0; i < queries->nqueries; ++i ) {
    queries->coh_phys[i].fkdot[0] += queries->dfreq * queries->coh_left[i];
  }

  // Return first point in semicoherent frequency block partition
  *semi_phys = queries->semi_phys;

  // Increment number of semicoherent templates
  queries->semi_ntmpl += *semi_nfreqs;

  return XLAL_SUCCESS;

}

///
/// Get number of computed coherent results, and number of coherent and semicoherent templates
///
int XLALWeaveCacheQueriesGetCounts(
  const WeaveCacheQueries *queries,
  UINT8 *coh_nres,
  UINT8 *coh_ntmpl,
  UINT8 *semi_ntmpl
  )
{

  // Check input
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( coh_nres != NULL, XLAL_EFAULT );
  XLAL_CHECK( coh_ntmpl != NULL, XLAL_EFAULT );
  XLAL_CHECK( semi_ntmpl != NULL, XLAL_EFAULT );

  // Return number of computed coherent results
  *coh_nres = 0;
  for ( size_t i = 0; i < queries->nqueries; ++i ) {
    *coh_nres += queries->coh_nres[i];
  }

  // Return number of computed coherent templates
  *coh_ntmpl = 0;
  for ( size_t i = 0; i < queries->nqueries; ++i ) {
    *coh_ntmpl += queries->coh_ntmpl[i];
  }

  // Write number of semicoherent templates
  *semi_ntmpl = queries->semi_ntmpl;

  return XLAL_SUCCESS;

}

///
/// Create a cache
///
WeaveCache *XLALWeaveCacheCreate(
  const LatticeTiling *coh_tiling,
  const BOOLEAN interpolation,
  const SuperskyTransformData *coh_rssky_transf,
  const SuperskyTransformData *semi_rssky_transf,
  WeaveCohInput *coh_input,
  const UINT4 max_size,
  const BOOLEAN all_gc
  )
{

  // Check input
  XLAL_CHECK_NULL( coh_tiling != NULL, XLAL_EFAULT );
  XLAL_CHECK_NULL( coh_input != NULL, XLAL_EFAULT );

  // Allocate memory
  WeaveCache *cache = XLALCalloc( 1, sizeof( *cache ) );
  XLAL_CHECK_NULL( cache != NULL, XLAL_ENOMEM );

  // Set fields
  cache->coh_rssky_transf = coh_rssky_transf;
  cache->semi_rssky_transf = semi_rssky_transf;
  cache->coh_input = coh_input;
  cache->generation = 0;

  // Set garbage collection mode:
  // - Garbage collection is not performed for a fixed-size cache (i.e. 'max_size > 0'),
  //   i.e. the cache will only discard items once the fixed-size cache is full, but not
  //   try to remove cache items earlier based on their relevances
  // - Garbage collection is applied to as many items as possible if 'all_gc' is true
  cache->any_gc = ( max_size == 0 );
  cache->all_gc = all_gc;

  // Get number of parameter-space dimensions
  cache->ndim = XLALTotalLatticeTilingDimensions( coh_tiling );
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Get lowest tiled parameter-space dimension
  if ( XLALTiledLatticeTilingDimensions( coh_tiling ) > 0 ) {
    cache->dim0 = XLALLatticeTilingTiledDimension( coh_tiling, 0 );
  } else {
    cache->dim0 = 0;
  }
  XLAL_CHECK_NULL( xlalErrno == 0, XLAL_EFUNC );

  // Compute offset used in computation of semicoherent point relevance:
  {

    // Convert a physical point far from any parameter-space boundaries to coherent and semicoherent reduced supersky coordinates
    PulsarDopplerParams phys_origin = { .Alpha = 0, .Delta = LAL_PI_2, .fkdot = {0} };
    XLAL_CHECK_NULL( XLALSetPhysicalPointSuperskyRefTime( &phys_origin, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );
    double coh_origin_array[cache->ndim];
    gsl_vector_view coh_origin_view = gsl_vector_view_array( coh_origin_array, cache->ndim );
    XLAL_CHECK_NULL( XLALConvertPhysicalToSuperskyPoint( &coh_origin_view.vector, &phys_origin, cache->coh_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );
    double semi_origin_array[cache->ndim];
    gsl_vector_view semi_origin_view = gsl_vector_view_array( semi_origin_array, cache->ndim );
    XLAL_CHECK_NULL( XLALConvertPhysicalToSuperskyPoint( &semi_origin_view.vector, &phys_origin, cache->semi_rssky_transf ) == XLAL_SUCCESS, XLAL_EFUNC );
    const double semi_origin_dim0 = gsl_vector_get( &semi_origin_view.vector, cache->dim0 );

    // Sample vertices of bounding box around 'coh_origin', and record maximum vertex in semicoherent reduced supersky coordinates in dimension 'dim0'
    double coh_bbox_sample_array[cache->ndim];
    gsl_vector_view coh_bbox_sample_view = gsl_vector_view_array( coh_bbox_sample_array, cache->ndim );
    gsl_vector_memcpy( &coh_bbox_sample_view.vector, &coh_origin_view.vector );
    double semi_bbox_sample_array[cache->ndim];
    gsl_vector_view semi_bbox_sample_view = gsl_vector_view_array( semi_bbox_sample_array, cache->ndim );
    double max_semi_bbox_sample_dim0 = semi_origin_dim0;
    XLAL_CHECK_NULL( cache_max_semi_bbox_sample_dim0( cache, coh_tiling, 0, &coh_bbox_sample_view.vector, &semi_bbox_sample_view.vector, &max_semi_bbox_sample_dim0 ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Subtract off origin of 'semi_origin' to get relevance offset
    cache->coh_relevance_offset = max_semi_bbox_sample_dim0 - semi_origin_dim0;

  }

  // If this is an interpolating search, create a lattice tiling locator
  if ( interpolation ) {
    cache->coh_locator = XLALCreateLatticeTilingLocator( coh_tiling );
    XLAL_CHECK_NULL( cache->coh_locator != NULL, XLAL_EFUNC );
    const LatticeTilingStats *stats = XLALLatticeTilingStatistics( coh_tiling, cache->ndim - 2 );
    XLAL_CHECK_NULL( stats != NULL, XLAL_EFUNC );
    cache->coh_max_index = stats->total_points;
  }

  // Create a heap which sorts items by "relevance", a quantity which determines how long
  // cache items are kept. Consider the following scenario:
  //
  //   +-----> parameter-space dimension 'dim0'
  //   |
  //   V parameter-space dimensions > 'dim0'
  //
  //        :
  //        : R[S1] = relevance of semicoherent point 'S1'
  //        :
  //        +-----+
  //        | /`\ |
  //        || S1||
  //        | \,/ |
  //        +-----+
  //      +          :
  //     / \         : R[S2] = relevance of semicoherent point 'S2'
  //    /,,,\        :
  //   /(   \\       +-----+
  //  + (    \\      | /`\ |
  //   \\  C  \\     || S2||
  //    \\    ) +    | \,/ |
  //     \\   )/:    +-----+
  //      \```/ :
  //       \ /  :
  //        +   : R[C] = relevance of coherent point 'C'
  //            :
  //
  // The relevance R[C] of the coherent point 'C' is given by the coordinate in dimension 'dim0' of
  // the *rightmost* edge of the bounding box surrounding its metric ellipse. The relevances of
  // two semicoherent points S1 and S2, R[S1] and R[S2], are given by the *leftmost* edges of the
  // bounding box surrounding their metric ellipses.
  //
  // Note that iteration over the parameter space is ordered such that dimension 'dim0' is the slowest
  // (tiled) coordinate, i.e. dimension 'dim0' is passed over only once, therefore coordinates in this
  // dimension are monotonically increasing, therefore relevances are also monotonically increasing.
  //
  // Suppose S1 is the current point in the semicoherent parameter-space tiling; note that R[C] > R[S1].
  // It is clear that, as iteration progresses, some future semicoherent points will overlap with C.
  // Therefore C cannot be discarded from the cache, since it will be the closest point for future
  // semicoherent points. Now suppose that S2 is the current point; note that R[C] < R[S1]. It is clear
  // that neither S2, nor any future point in the semicoherent parameter-space tiling, can ever overlap
  // with C. Therefore C can never be the closest point for any future semicoherent points, and therefore
  // it can safely be discarded from the cache.
  //
  // In short, an item in the cache can be discarded once its relevance falls below the threshold set
  // by the current point semicoherent in the semicoherent parameter-space tiling.
  //
  // Items removed from the heap are destroyed by calling cache_item_destroy().
  cache->relevance_heap = XLALHeapCreate( cache_item_destroy, max_size, -1, cache_item_compare_by_relevance );
  XLAL_CHECK_NULL( cache->relevance_heap != NULL, XLAL_EFUNC );

  // Create a hash table which looks up cache items by partition and locator index. Items removed
  // from the hash table are NOT destroyed, since items are shared with 'relevance_heap'.
  cache->coh_index_hash = XLALHashTblCreate( NULL, cache_item_hash, cache_item_compare_by_coh_index );
  XLAL_CHECK_NULL( cache->coh_index_hash != NULL, XLAL_EFUNC );

  // Create a bitset which records which cache items have ever been computed.
  cache->coh_computed_bitset = XLALBitsetCreate();
  XLAL_CHECK_NULL( cache->coh_computed_bitset != NULL, XLAL_EFUNC );

  return cache;

}

///
/// Destroy a cache
///
void XLALWeaveCacheDestroy(
  WeaveCache *cache
  )
{
  if ( cache != NULL ) {
    XLALDestroyLatticeTilingLocator( cache->coh_locator );
    XLALHeapDestroy( cache->relevance_heap );
    XLALHashTblDestroy( cache->coh_index_hash );
    cache_item_destroy( cache->saved_item );
    XLALBitsetDestroy( cache->coh_computed_bitset );
    XLALFree( cache );
  }
}

///
/// Determine the mean maximum size obtained by caches
///
int XLALWeaveGetCacheMeanMaxSize(
  REAL4 *cache_mean_max_size,
  const size_t ncache,
  WeaveCache *const *cache
  )
{

  // Check input
  XLAL_CHECK( cache_mean_max_size != NULL, XLAL_EFAULT );
  XLAL_CHECK( ncache > 0, XLAL_ESIZE );
  XLAL_CHECK( cache != NULL, XLAL_EFAULT );

  // Determine the mean maximum size obtained by caches
  *cache_mean_max_size = 0;
  for ( size_t i = 0; i < ncache; ++i ) {
    *cache_mean_max_size += cache[i]->heap_max_size;
  }
  *cache_mean_max_size /= ncache;

  return XLAL_SUCCESS;

}

///
/// Write various information from caches to a FITS file
///
int XLALWeaveCacheWriteInfo(
  FITSFile *file,
  const size_t ncache,
  WeaveCache *const *cache
  )
{

  // Check input
  XLAL_CHECK( file != NULL, XLAL_EFAULT );
  XLAL_CHECK( ncache > 0, XLAL_ESIZE );
  XLAL_CHECK( cache != NULL, XLAL_EFAULT );

  // Write the mean maximum size obtained by caches
  REAL4 cache_mean_max_size = 0;
  XLAL_CHECK( XLALWeaveGetCacheMeanMaxSize( &cache_mean_max_size, ncache, cache ) == XLAL_SUCCESS, XLAL_EFUNC );
  XLAL_CHECK( XLALFITSHeaderWriteREAL4( file, "cachemmx", cache_mean_max_size, "Mean maximum size obtained by cache" ) == XLAL_SUCCESS, XLAL_EFUNC );

  return XLAL_SUCCESS;

}

///
/// Expire all items in the cache
///
int XLALWeaveCacheExpire(
  WeaveCache *cache
  )
{

  // Check input
  XLAL_CHECK( cache != NULL, XLAL_EFAULT );

  // Advance current generation of cache items
  // - Existing items will no longer be accessible, but are still kept for reuse
  ++cache->generation;

  return XLAL_SUCCESS;

}

///
/// Clear all items in the cache from memory
///
int XLALWeaveCacheClear(
  WeaveCache *cache
  )
{

  // Check input
  XLAL_CHECK( cache != NULL, XLAL_EFAULT );

  // Clear items in the relevance heap and hash table from memory
  XLAL_CHECK( XLALHeapClear( cache->relevance_heap ) == XLAL_SUCCESS, XLAL_EFUNC );
  XLAL_CHECK( XLALHashTblClear( cache->coh_index_hash ) == XLAL_SUCCESS, XLAL_EFUNC );

  // Reset current generation of cache items
  cache->generation = 0;

  return XLAL_SUCCESS;

}

///
/// Retrieve coherent results for a given query, or compute new coherent results if not found
///
int XLALWeaveCacheRetrieve(
  WeaveCache *cache,
  const WeaveCacheQueries *queries,
  const UINT4 query_index,
  const WeaveCohResults **coh_res,
  UINT8 *coh_index,
  UINT4 *coh_offset,
  WeaveSearchTiming *tim
  )
{

  // Check input
  XLAL_CHECK( cache != NULL, XLAL_EFAULT );
  XLAL_CHECK( queries != NULL, XLAL_EFAULT );
  XLAL_CHECK( query_index < queries->nqueries, XLAL_EINVAL );
  XLAL_CHECK( coh_res != NULL, XLAL_EFAULT );
  XLAL_CHECK( coh_offset != NULL, XLAL_EFAULT );
  XLAL_CHECK( tim != NULL, XLAL_EFAULT );

  // See if coherent results are already cached
  const cache_item find_key = { .generation = cache->generation, .coh_index = queries->coh_index[query_index] };
  const cache_item *find_item = NULL;
  XLAL_CHECK( XLALHashTblFind( cache->coh_index_hash, &find_key, ( const void ** ) &find_item ) == XLAL_SUCCESS, XLAL_EFUNC );
  if ( find_item == NULL ) {

    // Reuse 'saved_item' if possible, otherwise allocate memory for a new cache item
    if ( cache->saved_item == NULL ) {
      cache->saved_item = XLALCalloc( 1, sizeof( *cache->saved_item ) );
      XLAL_CHECK( cache->saved_item != NULL, XLAL_EINVAL );
    }
    cache_item *new_item = cache->saved_item;
    find_item = new_item;

    // Set the key of the new cache item for future lookups
    new_item->generation = find_key.generation;
    new_item->coh_index = find_key.coh_index;

    // Set the relevance of the coherent frequency block associated with the new cache item
    new_item->relevance = queries->coh_relevance[query_index];

    // Determine the number of points in the coherent frequency block
    const UINT4 coh_nfreqs = queries->coh_right[query_index] - queries->coh_left[query_index] + 1;

    // Compute coherent results for the new cache item
    XLAL_CHECK( XLALWeaveCohResultsCompute( &new_item->coh_res, cache->coh_input, &queries->coh_phys[query_index], coh_nfreqs, tim ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Add new cache item to the index hash table
    XLAL_CHECK( XLALHashTblAdd( cache->coh_index_hash, new_item ) == XLAL_SUCCESS, XLAL_EFUNC );

    // Get the item in the cache with the smallest relevance
    const cache_item *least_relevant_item = ( const cache_item * ) XLALHeapRoot( cache->relevance_heap );
    XLAL_CHECK( xlalErrno == 0, XLAL_EFUNC );

    // Create a 'fake' item specifying thresholds for cache item relevance, for comparison with 'least_relevant_item'
    const cache_item relevance_threshold = { .generation = cache->generation, .relevance = queries->semi_relevance };

    // If garbage collection is enabled, and item's relevance has fallen below the threshold relevance, it can be removed from the cache
    if ( cache->any_gc && least_relevant_item != NULL && least_relevant_item != new_item && cache_item_compare_by_relevance( least_relevant_item, &relevance_threshold ) < 0 ) {

      // Remove least relevant item from index hash table
      XLAL_CHECK( XLALHashTblRemove( cache->coh_index_hash, least_relevant_item ) == XLAL_SUCCESS, XLAL_EFUNC );

      // Exchange 'saved_item' with the least relevant item in the relevance heap
      XLAL_CHECK( XLALHeapExchangeRoot( cache->relevance_heap, ( void ** ) &cache->saved_item ) == XLAL_SUCCESS, XLAL_EFUNC );

      // If maximal garbage collection is enabled, remove as many results as possible
      while ( cache->all_gc ) {

        // Get the item in the cache with the smallest relevance
        least_relevant_item = ( const cache_item * ) XLALHeapRoot( cache->relevance_heap );
        XLAL_CHECK( xlalErrno == 0, XLAL_EFUNC );

        // If item's relevance has fallen below the threshold relevance, it can be removed from the cache
        if ( least_relevant_item != NULL && least_relevant_item != new_item && cache_item_compare_by_relevance( least_relevant_item, &relevance_threshold ) < 0 ) {

          // Remove least relevant item from index hash table
          XLAL_CHECK( XLALHashTblRemove( cache->coh_index_hash, least_relevant_item ) == XLAL_SUCCESS, XLAL_EFUNC );

          // Remove and destroy least relevant item from the relevance heap
          XLAL_CHECK( XLALHeapRemoveRoot( cache->relevance_heap ) == XLAL_SUCCESS, XLAL_EINVAL );

        } else {

          // All cache items are still relevant
          break;

        }

      }

    } else {

      // Add new cache item to the relevance heap; 'saved_item' many now contains an item removed from the heap
      XLAL_CHECK( XLALHeapAdd( cache->relevance_heap, ( void ** ) &cache->saved_item ) == XLAL_SUCCESS, XLAL_EFUNC );

      // If 'saved_item' contains an item removed from the heap, also remove it from the index hash table
      if ( cache->saved_item != NULL ) {
        XLAL_CHECK( XLALHashTblRemove( cache->coh_index_hash, cache->saved_item ) == XLAL_SUCCESS, XLAL_EFUNC );
      }

    }

    // Update maximum size obtained by relevance heap
    const UINT4 heap_size = XLALHeapSize( cache->relevance_heap );
    if ( cache->heap_max_size < heap_size ) {
      cache->heap_max_size = heap_size;
    }

    // Increment number of computed coherent results
    queries->coh_nres[query_index] += coh_nfreqs;

    // Check if coherent results have been computed previously
    const UINT8 coh_bitset_index = queries->freq_partition_index * cache->coh_max_index + find_key.coh_index;
    BOOLEAN computed = 0;
    XLAL_CHECK( XLALBitsetGet( cache->coh_computed_bitset, coh_bitset_index, &computed ) == XLAL_SUCCESS, XLAL_EFUNC );
    if ( !computed ) {

      // Coherent results have not been computed before: increment the number of coherent templates
      queries->coh_ntmpl[query_index] += coh_nfreqs;

      // This coherent result has now been computed
      XLAL_CHECK( XLALBitsetSet( cache->coh_computed_bitset, coh_bitset_index, 1 ) == XLAL_SUCCESS, XLAL_EFUNC );

    }

  }

  // Return coherent results from cache
  *coh_res = find_item->coh_res;

  // Return index of coherent result
  *coh_index = find_item->coh_index;

  // Return offset at which coherent results should be combined with semicoherent results
  *coh_offset = queries->semi_left - queries->coh_left[query_index];

  return XLAL_SUCCESS;

}

// Local Variables:
// c-file-style: "linux"
// c-basic-offset: 2
// End:
