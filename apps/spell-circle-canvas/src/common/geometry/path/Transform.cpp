/** @file
 * The affine transform's named constructors and its arithmetic.
 */

#include "sigilgeometry/path/Transform.h"

#include <algorithm>
#include <cmath>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>

#include "sigilgeometry/path/Numeric.h"

namespace sigil::geometry::path {

namespace {
/** The column-major matrix of the affine map
 *  x' = a·x + c·y + e,  y' = b·x + d·y + f. */
glm::mat3 affine(float a, float b, float c, float d, float e, float f) {
  return glm::mat3{a, b, 0, c, d, 0, e, f, 1};
}
}  // namespace

Transform Transform::translate(glm::vec2 offset) {
  return {affine(1, 0, 0, 1, offset.x, offset.y)};
}

Transform Transform::rotate(float degrees, glm::vec2 about) {
  const float turn = radians(degrees);
  const float c = std::cos(turn), s = std::sin(turn);
  return translate(about) * Transform{affine(c, s, -s, c, 0, 0)} *
         translate(-about);
}

Transform Transform::scale(glm::vec2 factors, glm::vec2 about) {
  return translate(about) *
         Transform{affine(factors.x, 0, 0, factors.y, 0, 0)} *
         translate(-about);
}

Transform Transform::skew(glm::vec2 degrees) {
  return {affine(1, std::tan(radians(degrees.y)), std::tan(radians(degrees.x)),
                 1, 0, 0)};
}

Transform Transform::fit(const Rect& from, const Rect& to,
                         bool preserveAspect) {
  if (from.width() <= 0 || from.height() <= 0)
    return translate(to.centre() - from.centre());
  glm::vec2 factors{to.width() / from.width(), to.height() / from.height()};
  if (preserveAspect) factors = glm::vec2(std::min(factors.x, factors.y));
  return translate(to.centre()) * scale(factors) * translate(-from.centre());
}

glm::vec2 Transform::operator()(glm::vec2 point) const {
  const glm::vec3 moved = matrix * glm::vec3(point, 1.0f);
  return {moved.x, moved.y};
}

Transform Transform::operator*(const Transform& first) const {
  return {matrix * first.matrix};
}

Transform Transform::inverse() const {
  if (std::abs(glm::determinant(matrix)) < 1e-12f) return {};
  return {glm::inverse(matrix)};
}

}  // namespace sigil::geometry::path
