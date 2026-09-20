#pragma once

/** @file
 * @ingroup compose-core
 *
 * Placement, as verbs: taking a node out of the flow and saying where
 * it stands instead — the insets, the pins, the two shorthands over
 * them, the centre point, and the cell a grid-shaped scheme puts it
 * in.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Layout.h>

#include <string_view>

namespace sigil::compose {

/** OUT OF THE FLOW, AND WHERE INSTEAD. Every verb here but the cell
 *  claim implies `absolute()`; lengths are `Dimension`s, so a bare
 *  number is pixels and a percent is of the parent's box. */
template <class Derived>
class PlacementVerbs {
 public:
  /** TAKE THIS NODE OUT OF THE FLOW — CSS `position: absolute`. It no
   *  longer sizes or displaces its siblings, and the verbs below place
   *  it; with none of them it stands at its parent's origin at its own
   *  size. */
  Derived& absolute();
  /** THIS NODE FILLS THE BOX IT STANDS IN — out of the flow and
   *  stretched to its parent's box. A size stated after it puts it back
   *  in the flow at that size; a pin or an inset stated after it is a
   *  placement, and stands. */
  Derived& cover();
  /** HOW FAR IN FROM EACH EDGE of the parent's box this node's own
   *  edges stand. One value is all four sides; four are left, top,
   *  right, bottom. A bare number is pixels, `pct()` is of the parent,
   *  and `autoDimension()` leaves a side unpinned, so a size or the
   *  opposite inset sizes the node instead of stretching it. */
  Derived& inset(Dimension all);
  Derived& inset(Dimension left, Dimension top, Dimension right,
                 Dimension bottom);
  /** Pin the node's LEFT edge @p d inside the parent's. The unpinned
   *  sides stay auto, which is what makes `.top(12).right(12)` a corner
   *  badge rather than a stretch. */
  Derived& left(Dimension d);
  /** Pin the node's TOP edge @p d below the parent's. */
  Derived& top(Dimension d);
  /** Pin the node's RIGHT edge @p d inside the parent's. Pinning left
   *  and right both stretches the node between them. */
  Derived& right(Dimension d);
  /** Pin the node's BOTTOM edge @p d above the parent's. Pinning top
   *  and bottom both stretches the node between them. */
  Derived& bottom(Dimension d);
  /** CENTRE this node ON a parent-space point, resolved after
   *  measurement so an intrinsic-size node centres correctly. */
  Derived& centerAt(SkPoint p);
  /** WHICH CELLS this child claims of the scheme above it, and how many
   *  it covers — read by grid-shaped schemes and by nothing else. A
   *  span of zero is raised to one. */
  Derived& cells(int column, int row, int columns = 1, int rows = 1);
  /** The same claim as one value — the shape a scheme reads it back as,
   *  so a caller computing a span passes what it computed. */
  Derived& cells(CellSpan span);
  /** WHICH NAMED REGION of the scheme above it this child claims. A
   *  name the scheme's picture does not carry is silent, and the child
   *  flows into the next free cell. */
  Derived& area(std::string_view name);
  /** Where this child sits INSIDE the cell box its span makes.
   *  `Align::Stretch` sizes it to the box instead of placing it in
   *  one. */
  Derived& cellAlign(Align across, Align down);
  /** Place this node on a parent-space RECT — exactly
   *  `left().top().width().height()`, so it writes the same four fields
   *  as the longhand and prunes identically. Pixels only; right and
   *  bottom stay unpinned. */
  Derived& rect(const SkRect& r);
  /** Pin this node's top-left to a parent-space POINT and leave it to
   *  size itself — exactly `left().top()`. Pixels only. */
  Derived& at(SkPoint topLeft);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
