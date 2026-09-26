#pragma once
/** @file
 * @ingroup geometry-path
 *
 * THE BRIDGE TO SKIA: the conversions between the geometry library's
 * own values — glm vectors, `Rect`, `Outline` — and Skia's point, rect
 * and path. Skia is the executor behind an outline (its path, its path
 * operations and its contour measure), and this header is the one place
 * a caller that draws a geometry result with Skia, or hands a Skia path
 * to the geometry library, spells the crossing.
 */
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <glm/vec2.hpp>

#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Transform.h"

namespace sigil::geometry::path {

/** The vector as a Skia point: the same two numbers under the other
 *  type, no reinterpretation. */
inline SkPoint toSk(glm::vec2 v) { return {v.x, v.y}; }
/** The point back as a vector, on the same terms. */
inline glm::vec2 fromSk(SkPoint p) { return {p.fX, p.fY}; }
/** A size as a vector: width into x, height into y. */
inline glm::vec2 fromSk(SkSize s) { return {s.width(), s.height()}; }
/** A size vector as a Skia size. */
inline SkSize toSkSize(glm::vec2 size) { return {size.x, size.y}; }

/** The rect's centre as a vector. */
inline glm::vec2 centre(const SkRect& r) { return {r.centerX(), r.centerY()}; }

/** The rectangle as a Skia rect. */
inline SkRect toSk(const Rect& r) {
  return SkRect::MakeLTRB(r.min.x, r.min.y, r.max.x, r.max.y);
}
/** A Skia rect as a rectangle. */
inline Rect fromSk(const SkRect& r) {
  return {{r.fLeft, r.fTop}, {r.fRight, r.fBottom}};
}

/** The outline as the Skia path that stands behind it: the same verbs
 *  and points under the same fill rule. Sharing, not copying — a Skia
 *  path is itself a shared value. */
SkPath toSk(const Outline& outline);
/** A Skia path as an outline, with its fill rule: even-odd and
 *  inverse-even-odd read as even-odd, the others as non-zero. */
Outline fromSk(SkPath path);

/** The affine map as a Skia matrix. */
inline SkMatrix toSk(const Transform& transform) {
  const glm::mat3& m = transform.matrix;
  return SkMatrix::MakeAll(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1],
                           m[2][1], 0, 0, 1);
}

/** The winding as the direction Skia draws a closed figure in. */
inline SkPathDirection toSk(Winding winding) {
  return winding == Winding::OutersClockwise ? SkPathDirection::kCW
                                             : SkPathDirection::kCCW;
}

}  // namespace sigil::geometry::path
