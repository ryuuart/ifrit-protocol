#pragma once

/** @file
 * NodeTransform — every animated number in a node's matrix stack, for one
 * frame, and the three producers that turn those lanes into a matrix: the
 * 3x3, the 4x4 with the depth lanes in it, and the canvas's own elementary
 * ops.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkM44.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <sigilgeometry/path/Numeric.h>

#include <cmath>
#include <tuple>

#include "ComposeInternal.h"
#include "Transforms.h"

namespace sigil::compose {

/** Every animated number in paint()'s matrix stack, for ONE frame.
 *
 *  One resolver, three consumers — paint()'s matrix, recordBounds()'s
 *  child union and hitInstance()'s inverse — because the three must
 *  describe the same matrix or a node draws where it cannot be hit. And
 *  one matrix PRODUCER, `matrix()` below, for the same reason one step
 *  later: turning these lanes into a matrix is itself a place three
 *  hand-written copies could disagree. */
struct NodeTransform {
  float tx = 0, ty = 0, rot = 0, scl = 1, sx = 1, sy = 1, skx = 0, sky = 0;
  // The depth lanes: the plane's turn about x and y, its depth, and its
  // depth scale. At rest they are exactly the identity, and a node at
  // rest in all four is a 2D node in every consumer.
  float rx = 0, ry = 0, tz = 0, sz = 1;
  /** Does anything past the translate need the origin pivot at all?
   *
   *  THE ONE DEFINITION, and every consumer asks it rather than writing
   *  the condition out. A consumer that spells its own and omits a lane
   *  silently drops that lane's effect: a per-axis-scaled child would
   *  contribute UNSCALED bounds to its parent's layers and bakes, which
   *  truncates the overflow with no diagnostic. A lane added to this
   *  struct belongs in here. */
  bool pivoted() const {
    return rot != 0 || scl != 1 || sx != 1 || sy != 1 || skx != 0 || sky != 0;
  }
  /** Has a depth lane left rest? Then the node is a PLANE turned or moved
   *  in depth, its matrix is the 4x4 of matrix44() flattened, and the
   *  2D producers below are not asked. THE ONE DEFINITION, for the same
   *  reason pivoted() is: a consumer that spells its own and omits a lane
   *  draws a plane where it cannot be hit. */
  bool spatial() const { return rx != 0 || ry != 0 || tz != 0 || sz != 1; }
  /** The matrix these lanes describe, prepended with `anchor` (the
   *  layout offset — pass {0, 0} for node-local): the translate lanes,
   *  then — gated on pivoted(), NOT a copy of it — the origin-pivoted
   *  rotate → scale → skew stack. THE ONE PRODUCER for recordBounds()'s
   *  child union and hitInstance()'s inverse. The anchor folds into the
   *  FIRST translate rather than being post-concatenated, because the two
   *  associate their float multiplies differently and recordBounds()'s
   *  results must stay bitwise stable.
   *
   *  The 2D producer: a node whose depth lanes have left rest is placed
   *  by matrix44() instead, and every consumer asks spatial() first. */
  SkMatrix matrix(SkPoint anchor, const detail::PaintProps& p, float w,
                  float h) const {
    SkMatrix m = SkMatrix::Translate(anchor.x() + tx, anchor.y() + ty);
    if (pivoted()) {
      const SkPoint origin = detail::resolveOrigin(p, w, h);
      m.preTranslate(origin.x(), origin.y());
      if (rot != 0) m.preRotate(rot);
      if (scl != 1 || sx != 1 || sy != 1) m.preScale(scl * sx, scl * sy);
      if (skx != 0 || sky != 0)
        m.preSkew(std::tan(geometry::path::radians(skx)),
                  std::tan(geometry::path::radians(sky)));
      m.preTranslate(-origin.x(), -origin.y());
    }
    return m;
  }
  /** The SAME stack as a 4x4, with the depth lanes in it: the translate
   *  lanes (z included), then about the 3D origin the CSS rotation list
   *  `rotateX · rotateY · rotateZ`, the scale with its depth factor, and
   *  the skew. The 2D lanes sit in this product exactly where matrix()
   *  puts them, so a node that turns about y keeps the rotate, scale and
   *  skew it had while flat. THE ONE 4x4 PRODUCER: paint's flattening,
   *  the bounds union, the hit test's inverse, the depth sort and the
   *  node→root accumulation all read this, in this order of operations,
   *  and the settle compare between two of them needs the products to
   *  agree bit for bit. `depth` is the node's block, null on a node
   *  without one (then the origin has no z). */
  SkM44 matrix44(SkPoint anchor, const detail::PaintProps& p,
                 const detail::DepthData* depth, float w, float h) const {
    SkM44 m = SkM44::Translate(anchor.x() + tx, anchor.y() + ty, tz);
    if (pivoted() || spatial()) {
      const SkPoint origin = detail::resolveOrigin(p, w, h);
      const float oz = depth ? depth->originZ : 0.0f;
      m.preTranslate(origin.x(), origin.y(), oz);
      if (rx != 0) m.preConcat(detail::rotateXMatrix(rx));
      if (ry != 0) m.preConcat(detail::rotateYMatrix(ry));
      if (rot != 0) m.preConcat(detail::rotateZMatrix(rot));
      if (scl != 1 || sx != 1 || sy != 1 || sz != 1)
        m.preScale(scl * sx, scl * sy, sz);
      if (skx != 0 || sky != 0)
        m.preConcat(detail::skewMatrix(std::tan(geometry::path::radians(skx)),
                                       std::tan(geometry::path::radians(sky))));
      m.preTranslate(-origin.x(), -origin.y(), -oz);
    }
    return m;
  }
  /** paint()'s consumer: the SAME stack, applied as the canvas's own
   *  elementary ops rather than one concat of matrix()'s product.
   *
   *  NOT a convenience — a BYTE-EXACTNESS requirement. Composing the
   *  stack into one SkMatrix and concat()ing that associates the float
   *  multiplies differently than sequential canvas ops do, so the CTM
   *  lands a few ulps away, and antialiased coverage along every edge
   *  changes with it. Replacing this with a single concat of matrix()
   *  therefore moves pixels across the whole scene. The op list below and
   *  matrix()'s are the same list in the same order for every flat
   *  lane; the four depth lanes are matrix44()'s alone, since a canvas
   *  has no elementary op for them. A flat lane added to the struct goes
   *  in both (the fieldPin below counts it). */
  void concatTo(SkCanvas& canvas, const detail::PaintProps& p, float w,
                float h) const {
    if (tx != 0 || ty != 0) canvas.translate(tx, ty);
    if (pivoted()) {
      const SkPoint origin = detail::resolveOrigin(p, w, h);
      canvas.translate(origin.x(), origin.y());
      if (rot != 0) canvas.rotate(rot);
      if (scl != 1 || sx != 1 || sy != 1) canvas.scale(scl * sx, scl * sy);
      if (skx != 0 || sky != 0)
        canvas.skew(std::tan(geometry::path::radians(skx)),
                    std::tan(geometry::path::radians(sky)));
      canvas.translate(-origin.x(), -origin.y());
    }
  }
  /** FIELD PIN (see ComposeInternal.h's FIELD PINS block). `pivoted()`
   *  is a hand-written exhaustive list over these members, exactly like
   *  a comparator, and fails the same way: silently, by not noticing. */
  static void fieldPin(NodeTransform& v) {
    auto& [tx, ty, rot, scl, sx, sy, skx, sky, rx, ry, tz, sz] = v;
    static_assert(
        std::tuple_size_v<decltype(std::tie(tx, ty, rot, scl, sx, sy, skx, sky,
                                            rx, ry, tz, sz))> == 12,
        "NodeTransform gained or lost a lane — put it in pivoted() or "
        "spatial() above (unless it is a pure translate), in matrix()'s "
        "and matrix44()'s builds, and in transformOf()'s resolve, then "
        "bump this count.");
  }
};

}  // namespace sigil::compose
