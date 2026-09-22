/** @file
 * Derive phase: content whose input is RESOLVED geometry. Runs after Yoga
 * has laid the tree out — the pass over the flat instance lists. The
 * families it drives are beside it: FlowAround.cpp, Threads.cpp and
 * Routes.cpp.
 */

#include "ComposeRuntime.h"
#include "DeriveInternal.h"

namespace sigil::compose {

using namespace detail;

/** The derive pass over the flat instance lists the key index rebuilds each
 *  render: contentFlowAround text nodes first, then the chains, then the
 *  nodes that borrow another's geometry, all in tree order — no tree
 *  recursion here. Returns true when a text exclusion changed, which means
 *  the geometry the caller just laid out is stale and layout must run
 *  again. */
bool Composer::Impl::resolveDerived() {
  bool relayout = false;
  for (Instance* inst : flowInstances) relayout |= deriveFlow(*inst);
  relayout |= resolveThreads();
  for (Instance* inst : borrowInstances) deriveBorrows(*inst);
  return relayout;
}

}  // namespace sigil::compose
