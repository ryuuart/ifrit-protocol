#pragma once
/** @file
 * What a flex item reads off the line it stands in: the container whose
 * line it is an item of, which is not always its parent, and the floor
 * CSS's automatic minimum gives a text leaf in a column.
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

/** Whether @p inst is a text leaf whose lines run across an in-flow item
 *  of a column that Yoga lays out, and which does not clip them: the one
 *  shape of item whose content height is its lines' and that draws them
 *  whole whatever box it is given. */
bool linesRunAcrossAColumn(const Instance& inst);

/** Whether @p inst keeps the height of its lines on a column short of
 *  room. CSS floors a flex item whose minimum is automatic at its content
 *  size, so a text leaf of that shape that states neither a minimum height
 *  nor a basis is held at its lines, or at a stated height or maximum
 *  shorter than they are. Yoga floors every item at zero and carries no
 *  automatic minimum, so the floor is the leaf declining to shrink: its
 *  basis is its lines, or its stated height. */
bool keepsItsLines(const Instance& inst);

}  // namespace sigil::compose::detail
