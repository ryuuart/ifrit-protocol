/** @file
 * The matrix a 3D placement describes.
 */

#include "sigilgeometry/mesh/Transform.h"

#include <glm/gtc/matrix_transform.hpp>

namespace sigil::geometry::mesh {

glm::mat4 Transform::matrix() const {
  constexpr float kRadiansPerDegree = 3.14159265358979323846f / 180.0f;
  glm::mat4 placed = glm::translate(glm::mat4(1.0f), translate);
  placed = glm::translate(placed, origin);
  if (glm::dot(axis, axis) > 0.0f && axisDegrees != 0.0f)
    placed = glm::rotate(placed, axisDegrees * kRadiansPerDegree,
                         glm::normalize(axis));
  // z, then y, then x — reading the three factors right to left is
  // reading the order they apply in.
  placed = glm::rotate(placed, rotateDegrees.x * kRadiansPerDegree, {1.0f, 0.0f, 0.0f});
  placed = glm::rotate(placed, rotateDegrees.y * kRadiansPerDegree, {0.0f, 1.0f, 0.0f});
  placed = glm::rotate(placed, rotateDegrees.z * kRadiansPerDegree, {0.0f, 0.0f, 1.0f});
  placed = glm::scale(placed, scale);
  return glm::translate(placed, -origin);
}

}  // namespace sigil::geometry::mesh
