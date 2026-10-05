/** @file
 * Instanced geometry's two readings: one instance's placement, and one
 * named attribute.
 */

#include "sigilgeometry/mesh/Instances.h"

#include <glm/gtc/matrix_transform.hpp>

namespace sigil::geometry::mesh {

glm::mat4 Instances::placement(size_t index) const {
  const glm::vec3 position =
      index < positions.size() ? positions[index] : glm::vec3(0.0f);
  const glm::quat orientation = index < orientations.size()
                                    ? orientations[index]
                                    : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  const glm::vec3 scale =
      index < scales.size() ? scales[index] : glm::vec3(1.0f);
  glm::mat4 out = glm::mat4_cast(orientation);
  out[0] *= scale.x;
  out[1] *= scale.y;
  out[2] *= scale.z;
  out[3] = glm::vec4(position, 1.0f);
  return out;
}

const std::vector<float>* Instances::attributeIf(std::string_view name) const {
  const auto found = attributes.find(name);
  return found == attributes.end() ? nullptr : &found->second;
}

}  // namespace sigil::geometry::mesh
