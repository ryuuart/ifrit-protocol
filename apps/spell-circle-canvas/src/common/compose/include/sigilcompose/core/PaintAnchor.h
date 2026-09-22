#pragma once

/** @file
 * @ingroup compose-core
 *
 * Where a surface paint's unit square lands: which box it maps onto,
 * and which of that box's three rectangles it begins at.
 */

#include <cstdint>

namespace sigil::compose {

/** WHICH BOX A PAINT'S UNIT SQUARE MAPS ONTO. Both `Element::fill` and
 *  `Element::ink` take it, so a ramp can dress one box, a whole subtree
 *  from the box that declared it, or the canvas several boxes stand on.
 *  @trap `ink` inherits and `fill` does not, so `DeclaringBox` on a fill
 *  is `OwnBox`: the element that stated the fill is the one painting it. */
enum class PaintAnchor : uint8_t {
  /** Each thing painted maps the paint onto ITS OWN box, so every one of
   *  them shows the whole paint. A text leaf's own box is its
   *  text-metric box — x across the widest line, y from the first line's
   *  cap top to the last line's baseline — which is where a ramp has to
   *  land for chrome type to read. The default. */
  OwnBox,
  /** The box of the element that STATED the paint; everything under it
   *  shows its own slice of the one paint. */
  DeclaringBox,
  /** The whole canvas; elements anywhere in the tree show slices of one
   *  paint, and moving one of them moves which slice it shows. */
  CanvasBox,
};

/** WHERE A SURFACE PAINT BEGINS inside the box it is anchored to — CSS's
 *  background-origin. The border box is the whole of what the node
 *  occupies; the two others shrink it by what the node's border, and
 *  then its padding, reserve. Only `Element::fill` takes it. */
enum class BackgroundOrigin : uint8_t {
  BorderBox,   ///< the whole box — the default
  PaddingBox,  ///< inside the border
  ContentBox,  ///< inside the border and the padding
};

}  // namespace sigil::compose
