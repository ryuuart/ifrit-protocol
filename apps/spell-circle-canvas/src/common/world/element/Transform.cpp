/** @file
 * The local matrix a node's resolved lanes describe.
 */

#include <sigilworld/element/Transform.h>

#include <glm/gtc/matrix_transform.hpp>

namespace sigil::world {

glm::mat4 localMatrix(const TransformValues& values) {
  constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
  glm::mat4 m = glm::translate(glm::mat4(1.0f), values.translate);
  m = glm::translate(m, values.origin);
  if (glm::dot(values.axis, values.axis) > 0.0f && values.axisDegrees != 0.0f)
    m = glm::rotate(m, values.axisDegrees * kDegToRad,
                    glm::normalize(values.axis));
  // z, then y, then x — reading the three factors right to left is
  // reading the order they apply in.
  m = glm::rotate(m, values.rotateDegrees.x * kDegToRad, {1.0f, 0.0f, 0.0f});
  m = glm::rotate(m, values.rotateDegrees.y * kDegToRad, {0.0f, 1.0f, 0.0f});
  m = glm::rotate(m, values.rotateDegrees.z * kDegToRad, {0.0f, 0.0f, 1.0f});
  m = glm::scale(m, values.scale);
  return glm::translate(m, -values.origin);
}

}  // namespace sigil::world
