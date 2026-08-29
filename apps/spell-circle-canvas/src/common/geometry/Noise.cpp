#include "sigilgeometry/Noise.h"

#include <cmath>

namespace sigil::geometry::noise {

float value3(glm::vec3 p, uint32_t seed) {
  auto hash = [seed](int x, int y, int z) {
    uint32_t h = seed + (uint32_t)x * 374761393u + (uint32_t)y * 668265263u +
                 (uint32_t)z * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)(h ^ (h >> 16)) / (float)0xFFFFFFFFu;
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

}  // namespace sigil::geometry::noise
