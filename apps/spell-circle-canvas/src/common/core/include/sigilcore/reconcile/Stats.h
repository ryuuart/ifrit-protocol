#pragma once

/** @file
 * What one reconcile pass did, counted: nodes visited, memo hits, nodes
 * whose description changed, nodes mounted and nodes retired.
 */

#include <sigilmeasure/stats/Counters.h>

#include <cstdint>

namespace sigil::core {

/** The reconciler's tallies. The first three are zeroed by every top-level
 *  render and accumulate across the slot patches that follow it; the last
 *  two count the pass the same way. Plain integers on the hot path;
 *  `report()` publishes them into named counters for a printed set. */
struct ReconcileStats {
  int64_t describedNodes = 0;  ///< descriptions visited
  int64_t memoHits = 0;        ///< memo env and props equal → describe skipped
  int64_t patchedNodes = 0;    ///< nodes whose description changed
  int64_t mounted = 0;         ///< nodes created
  int64_t retired = 0;         ///< nodes handed to the host's destroy

  /** Zero the per-pass counts. */
  void reset() {
    describedNodes = memoHits = patchedNodes = mounted = retired = 0;
  }
  /** Adds every count under its name (`reconcile.described`,
   *  `reconcile.memoHits`, `reconcile.patched`, `reconcile.mounted`,
   *  `reconcile.retired`). */
  void report(measure::Counters& counters) const {
    counters.add("reconcile.described", describedNodes);
    counters.add("reconcile.memoHits", memoHits);
    counters.add("reconcile.patched", patchedNodes);
    counters.add("reconcile.mounted", mounted);
    counters.add("reconcile.retired", retired);
  }
};

}  // namespace sigil::core
