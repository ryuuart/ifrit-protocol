#pragma once

/** @file
 * Internal to the kernel — what a composer counts about its own work: the
 * statistics a draw publishes, the per-node paint profiler, and the phase
 * time and layout calculations accumulated between draws.
 */

#include <cstddef>
#include <vector>

#include "sigilcompose/core/Composer.h"

namespace sigil::compose::detail {

/** THE COMPOSER'S COUNTERS: one part of its retained state, read off it by
 *  each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct DrawStatistics {
  mutable Composer::Stats stats;
  // ---- per-node paint profiler (opt-in; Composer::setProfiling) ----------
  // profileChildMilliseconds is the running total the CURRENT node's children
  // have cost; each node saves its parent's value, zeroes it, paints, then
  // reports its own total upward. That gives selfMilliseconds =
  // totalMilliseconds - children without a second traversal.
  bool profileEnabled = false;
  std::vector<Composer::NodeCost> profileRows;
  double profileChildMilliseconds = 0;
  int profDepth = 0;
  // render()/renderSlot() phase time accumulated since the previous draw();
  // draw() publishes it as stats.reconcileMilliseconds and zeroes the
  // accumulator.
  double reconcileAccumulatedMilliseconds = 0;
  // The volatility walk's time since the previous draw(), taken inside the
  // settle that precedes the paint; draw() publishes it as
  // stats.volatileMilliseconds, apart from the layout it settles beside.
  double volatileAccumulatedMilliseconds = 0;
  /** Whole-tree Yoga calculations since the previous draw(). */
  size_t layoutCalculations = 0;
};

}  // namespace sigil::compose::detail
