#pragma once

/** @file
 * Host scheduling for independent item and work-group ranges in the CPU
 * point-operator executors.
 */

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <cstddef>
#include <cstdint>

namespace sigil::geometry::mesh::parallel {

/** Run an independent item range. Small ranges stay on their caller;
 *  larger ones are split into coarse, contiguous pieces. */
template <class Run>
void items(size_t count, Run&& run) {
  constexpr size_t parallelThreshold = 16'384;
  constexpr size_t grain = 4'096;
  if (count < parallelThreshold) {
    run(0, count);
    return;
  }
  oneapi::tbb::parallel_for(
      oneapi::tbb::blocked_range<size_t>(0, count, grain),
      [&run](const oneapi::tbb::blocked_range<size_t>& range) {
        run(range.begin(), range.end());
      });
}

/** Run a kernel's one-dimensional group range. Small dispatches stay on
 *  their caller; larger ones are split into coarse, contiguous ranges so
 *  each worker enters the generated kernel once per range rather than once
 *  per group. The generated kernel owns the group size and clips its final
 *  group against the lane count. */
template <class Run>
void groups(uint32_t count, Run&& run) {
  constexpr uint32_t parallelThreshold = 256;
  constexpr uint32_t grain = 32;
  if (count < parallelThreshold) {
    run(0, count);
    return;
  }
  oneapi::tbb::parallel_for(
      oneapi::tbb::blocked_range<uint32_t>(0, count, grain),
      [&run](const oneapi::tbb::blocked_range<uint32_t>& range) {
        run(range.begin(), range.end());
      });
}

}  // namespace sigil::geometry::mesh::parallel
