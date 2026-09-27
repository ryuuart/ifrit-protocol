#pragma once

/** @file
 * WHAT A FILL LOWERED TO, as the painter reads it: the Skia executor's
 * paint behind a fill's material, a paint wrapped back into a fill, and
 * the static collapse and per-frame resolves in paint terms. Internal: a
 * fill's public face is its material, and only the code that draws with
 * the executor reaches the paint beneath it.
 */

#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/paint/Paint.h>

#include <utility>

namespace sigil::compose {

namespace detail {

/** The door `Fill` opens to the painter. */
struct FillAccess {
  /** The paint @p fill lowered to; a paint of nothing for a fill that
   *  holds none. */
  static const material::Paint& paint(const Fill& fill);
  /** @p paint as a fill, the material whose base it is. A paint of
   *  nothing is no fill; a flat paint stays a paint, which the ink tells
   *  apart from a colour: a colour is the inherited ink lane, and a paint
   *  is a paint. */
  static Fill of(material::Paint paint);
};

/** The paint the executor lowered @p fill's material to — what the
 *  painter draws with. */
inline const material::Paint& paintOf(const Fill& fill) {
  return FillAccess::paint(fill);
}

/** @p paint as a fill; see `FillAccess::of`. */
inline Fill fillOf(material::Paint paint) {
  return FillAccess::of(std::move(paint));
}

/** A text effect's pass material as the executor lowered it. */
struct LoweredPass {
  material::Paint paint;
};

}  // namespace detail

/** The static collapse of a paint: a flat paint is its colour, a static
 *  one is itself, and one that needs a frame is no fill. */
Fill toFill(const material::Paint& paint);

/** The current-frame fill of a paint: flat as its colour, any other built
 *  against @p ctx. */
Fill resolveFill(const material::Paint& paint, const PaintContext& ctx);

/** THE INK'S PAINT AS A FILL at the node @p ctx describes. An own-box ink
 *  maps the paint's unit square onto that node's box, which is what every
 *  other paint on a node does; an anchored one maps it onto the box the
 *  context names and hands back this node's slice of it. */
[[nodiscard]] Fill resolveInk(const material::Paint& paint,
                              const PaintContext& ctx);

}  // namespace sigil::compose
