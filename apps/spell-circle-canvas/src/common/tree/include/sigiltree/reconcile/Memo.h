#pragma once

/** @file
 * @ingroup tree-reconcile
 *
 * The memo — a description deferred behind its inputs: the properties it was
 * given, the comparison that says whether two prop sets are the same, and
 * the call that produces the description.
 */

#include <any>
#include <functional>

namespace sigil::tree {

/** A deferred describe and its key. The reconciler compares `properties`
 *  against the memo the retained node was last described from; on a hit
 *  the retained payload stands and `invoke` is never called, on a miss
 *  `invoke` runs and its result becomes the node's payload. The properties
 *  are the whole key, so everything the describe reads belongs in them.
 *  `Produced` is whatever the author's function returns; the host says how
 *  a description is read off it. */
template <class Produced>
struct Memo {
  std::any properties;
  std::function<bool(const std::any&, const std::any&)> equal;
  std::function<Produced(const std::any&)> invoke;
};

}  // namespace sigil::tree
