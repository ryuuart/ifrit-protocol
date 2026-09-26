#pragma once
/** @file
 * @ingroup geometry-path
 *
 * A 2D AFFINE TRANSFORM AS A VALUE: translate, rotate, scale and skew by
 * name, composed with `*`, applied to a point here and to an outline by
 * `Outline::transformed`.
 */
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>

#include "sigilgeometry/path/Outline.h"

namespace sigil::geometry::path {

/** A 2D affine map in y-down space, held as a glm matrix acting on
 *  column vectors `(x, y, 1)`. Angles are degrees, positive turning
 *  CLOCKWISE on screen.
 *
 *  Composition reads right to left, as matrices do: `(a * b)(point)` is
 *  `a(b(point))` — `b` happens first. */
struct Transform {
  glm::mat3 matrix{1.0f};

  /** Moves every point by @p offset. */
  static Transform translate(glm::vec2 offset);
  /** Turns by @p degrees clockwise on screen about @p about. */
  static Transform rotate(float degrees, glm::vec2 about = {0, 0});
  /** Scales by @p factors along x and y, holding @p about still. */
  static Transform scale(glm::vec2 factors, glm::vec2 about = {0, 0});
  /** Leans x by @p degrees.x per unit of y and y by @p degrees.y per
   *  unit of x — CSS `skew()`. */
  static Transform skew(glm::vec2 degrees);
  /** The map carrying @p from onto @p to: stretched to fill it, or, with
   *  @p preserveAspect, scaled evenly to fit inside it and centred. An
   *  empty @p from maps by translation alone. */
  static Transform fit(const Rect& from, const Rect& to,
                       bool preserveAspect = false);

  /** Where @p point lands. */
  glm::vec2 operator()(glm::vec2 point) const;
  /** This map after @p first: `(*this * first)(p) == (*this)(first(p))`. */
  Transform operator*(const Transform& first) const;
  /** The map that undoes this one; a map that flattens the plane has no
   *  inverse and answers the identity. */
  Transform inverse() const;

  bool operator==(const Transform&) const = default;
};

}  // namespace sigil::geometry::path
