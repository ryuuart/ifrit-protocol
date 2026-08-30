#pragma once

/** @file
 * What one frame of a Scene did, counted — the reconcile pass's own
 * tallies beside the retained side's.
 */

#include <sigilcore/reconcile/Stats.h>
#include <sigilmeasure/stats/Counters.h>

#include <cstdint>

namespace sigil::world {

/** THE FRAME'S COST, as numbers a test can assert and a table can print.
 *  Plain integers on the hot path; `report()` publishes them under their
 *  names beside the reconciler's. */
struct SceneStats {
  /** What the reconcile pass did — described, memo hits, patched,
   *  mounted, retired. */
  core::ReconcileStats reconcile;
  int64_t nodes = 0;      ///< nodes standing in the retained tree
  int64_t extracted = 0;  ///< nodes extract wrote components for
  int64_t cooked = 0;     ///< geometry slots cooked this frame
  int64_t resources = 0;  ///< distinct cooked artefacts held
  int64_t baked = 0;      ///< subtrees whose draw order was taken
  int64_t replayed = 0;   ///< subtrees whose draw order was replayed
  int64_t drawn = 0;      ///< entities in the extracted draw order
  int64_t rounds = 0;     ///< converging rounds the phase runner ran

  void reset() {
    reconcile.reset();
    nodes = extracted = cooked = baked = replayed = drawn = rounds = 0;
  }

  /** Adds every count under its name (`world.nodes`, `world.extracted`,
   *  `world.cooked`, `world.resources`, `world.baked`,
   *  `world.replayed`, `world.drawn`, `world.rounds`), and the
   *  reconciler's under theirs. */
  void report(measure::Counters& counters) const {
    reconcile.report(counters);
    counters.add("world.nodes", nodes);
    counters.add("world.extracted", extracted);
    counters.add("world.cooked", cooked);
    counters.add("world.resources", resources);
    counters.add("world.baked", baked);
    counters.add("world.replayed", replayed);
    counters.add("world.drawn", drawn);
    counters.add("world.rounds", rounds);
  }
};

}  // namespace sigil::world
