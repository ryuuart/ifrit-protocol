/** @file
 * The seeded, bit-exact noise: trilinear value noise over the integer
 * lattice on top of the per-index hash.
 */

#include "sigilgeometry/path/Noise.h"

#include <cmath>

namespace sigil::geometry::path::noise {

float value3(glm::vec3 p, uint32_t seed) {
  auto hash = [seed](int x, int y, int z) {
    return (float)core::noise::lattice(seed, x, y, z) / (float)0xFFFFFFFFu;
  };
  const int xi = (int)std::floor(p.x), yi = (int)std::floor(p.y),
            zi = (int)std::floor(p.z);
  const float xf = p.x - (float)xi, yf = p.y - (float)yi, zf = p.z - (float)zi;
  auto smooth = [](float t) { return t * t * (3 - 2 * t); };
  const float u = smooth(xf), v = smooth(yf), w = smooth(zf);
  float accum = 0;
  for (int dz = 0; dz <= 1; ++dz)
    for (int dy = 0; dy <= 1; ++dy)
      for (int dx = 0; dx <= 1; ++dx) {
        const float weight =
            (dx ? u : 1 - u) * (dy ? v : 1 - v) * (dz ? w : 1 - w);
        accum += hash(xi + dx, yi + dy, zi + dz) * weight;
      }
  return accum * 2.0f - 1.0f;
}

}  // namespace sigil::geometry::path::noise
