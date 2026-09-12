#pragma once

/** @file
 * What a paint costs, per node: the row a node contributes to the
 * composer's profile, the scope that times it, and the COMPOSE_PROF
 * threshold above which a single draw prints itself.
 */

#include <include/core/SkRect.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <cstddef>
#include <cstdlib>  // std::getenv, std::strtod
#include <optional>
#include <string>

#include "ComposeRuntime.h"

namespace sigil::compose {

/** A readable, ACTIONABLE identity for a profile row: the author's own
 *  key() when there is one (that is what they will search for), else the
 *  node kind and its painted size, which is usually enough to find it. */
std::string profileLabel(const detail::Instance& inst, const SkRect& rect);

/** The threshold COMPOSE_PROF=<ms> sets: any draw above it prints itself —
 *  cached-texture blits, picture replays (which re-EXECUTE recorded ops on
 *  raster), live paints, and the bakes themselves, which are the cost a
 *  blit is bought with. Nested lines overlap (inclusive of children); any
 *  unparsable value means 4ms, and an unset variable means never. */
inline double profileThresholdMs() {
  static const double ms = [] {
    const char* environment = getenv("COMPOSE_PROF");
    if (!environment) return -1.0;
    const double v = std::strtod(environment, nullptr);
    return v > 0.0 ? v : 4.0;
  }();
  return ms;
}

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
    const double total = watch->elapsedMs();
    impl->profileRows[row].totalMs = total;
    impl->profileRows[row].selfMs = total - impl->profChildMs;
    // Hand our whole cost up to the parent's child accumulator.
    impl->profChildMs = savedChildren + total;
    --impl->profDepth;
  }
};

}  // namespace sigil::compose
