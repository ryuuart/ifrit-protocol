#pragma once
/** @file
 * What a flex item reads off the line it stands in: the container whose
 * line it is an item of, which is not always its parent.
 */

#include "runtime/Instance.h"

namespace sigil::compose::detail {

/** The node whose flex line @p inst is an item of: its parent, or the
 *  nearest ancestor above parents that have no box of their own. */
inline const Instance* flexContainerOf(const Instance& inst) {
  const Instance* container = inst.parent;
  while (container && container->parent &&
         container->computed.layout.display == Display::Contents)
    container = container->parent;
  return container;
}

}  // namespace sigil::compose::detail
