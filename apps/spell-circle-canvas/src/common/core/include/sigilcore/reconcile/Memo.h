#pragma once

/** @file
 * The memo — a description deferred behind its inputs: the props it was
 * given, the comparison that says whether two prop sets are the same, the
 * call that produces the description, and the environment the author had
 * when the memo was written.
 */

#include <any>
#include <functional>

#include "sigilcore/reconcile/Env.h"

namespace sigil::core {

/** A deferred describe and its key. The reconciler compares `env` first
 *  and `props` second against the memo the retained node was last
 *  described from; on a hit the retained payload stands and `invoke` is
 *  never called, on a miss `invoke` runs under `env` re-established and
 *  its result becomes the node's payload. `Produced` is whatever the
 *  author's function returns; the host says how a description is read
 *  off it. */
template <class Produced>
struct Memo {
  std::any props;
  std::function<bool(const std::any&, const std::any&)> equal;
  std::function<Produced(const std::any&)> invoke;
  /** The `env::` bindings in scope where this memo was WRITTEN. The memo
   *  is the one deferred describe, so it is also the one place an
   *  inherited value could go stale: the snapshot rides in the memo's
   *  key and is re-established around the invoke. Empty — hence free —
   *  when nothing is bound. */
  env::Snapshot env;
};

}  // namespace sigil::core
