#pragma once

/** @file
 * What a paint costs, per node: the row a node contributes to the
 * composer's profile, and the scope that times it.
 */

#include <include/core/SkRect.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <cstddef>
#include <optional>
#include <string>

#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

/** A readable, ACTIONABLE identity for a profile row: the author's own
 *  key() when there is one (that is what they will search for), else the
 *  node kind and its painted size, which is usually enough to find it. */
std::string profileLabel(const detail::Instance& inst, const SkRect& rect);

/** Scoped per-node timer. RAII because paint() has several early returns
 *  and a half-written row would be worse than no row at all. */
struct ProfileScope {
  Composer::Impl* impl = nullptr;
  size_t row = SIZE_MAX;
  double savedChildren = 0;
  // Absent until profiling is on: a clock read per node per frame is not
  // a cost an unprofiled paint should pay.
  std::optional<measure::Stopwatch> watch;

  ProfileScope(Composer::Impl* i, const detail::Instance& inst,
               const SkRect& rect)
      : impl(i) {
    // A COVERAGE TRACE IS NOT A FRAME: it paints a node again into an
    // offscreen raster to read what it drew, so its nodes are not nodes
    // the viewer saw and their cost is not the frame's.
    if (!impl->profileEnabled || impl->coverageTrace) return;
    row = impl->profileRows.size();
    impl->profileRows.push_back(Composer::NodeCost{profileLabel(inst, rect), 0,
                                                   0, impl->profDepth,
                                                   Composer::CacheState::Live});
    savedChildren = impl->profChildMs;
    impl->profChildMs = 0;
    ++impl->profDepth;
    watch.emplace();
  }
  ~ProfileScope() {
    if (row == SIZE_MAX) return;
    const double total = measure::Milliseconds(watch->elapsed()).count();
    impl->profileRows[row].totalMs = total;
    impl->profileRows[row].selfMs = total - impl->profChildMs;
    // Hand our whole cost up to the parent's child accumulator.
    impl->profChildMs = savedChildren + total;
    --impl->profDepth;
  }
};

}  // namespace sigil::compose
