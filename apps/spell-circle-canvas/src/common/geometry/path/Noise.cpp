/** @file
 * The seeded, bit-exact noise read at a position.
 */

#include "sigilgeometry/path/Noise.h"

#include <sigilcore/compute/Field.h>

namespace sigil::geometry::path {

float valueNoise(glm::vec3 p, uint32_t seed) {
  return core::noise::valueNoise(seed, p.x, p.y, p.z);
}

}  // namespace sigil::geometry::path
