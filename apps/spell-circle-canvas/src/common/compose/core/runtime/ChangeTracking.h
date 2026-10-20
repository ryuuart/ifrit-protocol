#pragma once

/** @file
 * Internal to the kernel — what a draw changes: whether the composer keeps
 * each node's extent for the region the next draw repaints, what the host
 * was promised before a draw and what the draw found against it, and the
 * one walk that answers both.
 */

#include <include/core/SkRect.h>

#include <optional>
#include <vector>

namespace sigil::compose::detail {

/** THE CHANGE TRACKING OF A COMPOSER: one part of its retained state, read
 *  off it by each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct ChangeTracking {
  // Kept from the first `Composer::changedRegion` on: every draw after it
  // walks the tree once more to keep each node's extent for the next.
  bool trackChanges = false;
  // Nothing the walk can say holds for the next draw: no draw has kept the
  // extents yet, or the size, the view, the map, its scale or the caches
  // changed. Cleared by the draw that keeps them.
  bool changesUnknown = true;
  // The region the host was told before this draw, and whether it asked.
  std::optional<std::vector<SkRect>> promisedRegion;
  bool regionPromised = false;
  // What the last draw found against that promise.
  bool drewWithinPromise = true;
  // The layout, cascade and volatility a draw settles were settled by a
  // question asked before it.
  bool settledForDraw = false;
  struct ChangeWalk {
    bool commit = false;
    bool bounded = true;
  };
};

}  // namespace sigil::compose::detail
