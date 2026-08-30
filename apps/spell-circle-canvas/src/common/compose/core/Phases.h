#pragma once

/** @file
 * Internal to the kernel — the layout phase list: the passes ensureLayout
 * runs, in order, once a reconcile has left the tree needing layout. Each is
 * a member of Composer::Impl that answers whether it moved anything, and a
 * pass marked converging runs inside the bounded convergence loop rather
 * than once.
 */

#include "Instance.h"

namespace sigil::compose::detail {

/** One layout pass. `run` answers true when it changed geometry some other
 *  pass may have already read; the runner only asks that of a converging
 *  pass. */
struct LayoutPhase {
  const char* name;
  bool (Composer::Impl::*run)();
  /** Runs inside the convergence loop: the group of converging passes is
   *  repeated, with a relayout and a settle between rounds, until a round
   *  changes nothing or the round cap is reached. */
  bool converging;
};

}  // namespace sigil::compose::detail
