/** @file
 * The layered value-noise field behind `noise()`.
 */

#include <sigilcore/compute/Noise.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Noise.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw {

namespace {

/** The whole part as an int that cannot overflow, and the fraction. */
void split(float v, int& whole, float& fraction) {
  const float f = std::floor(v);
  whole = (int)std::clamp(f, -1.0e9f, 1.0e9f);
  fraction = v - f;
}

/** p5's blend between two lattice corners: a raised cosine. */
float blend(float t) { return 0.5f * (1.0f - std::cos(t * PI)); }

float sample(uint32_t seed, float x, float y, float z) {
  int ix, iy, iz;
  float fx, fy, fz;
  split(x, ix, fx);
  split(y, iy, fy);
  split(z, iz, fz);
  const float sx = blend(fx);
  const float sy = blend(fy);
  const float sz = blend(fz);
  auto corner = [&](int dx, int dy, int dz) {
    return NoiseField::corner(seed, ix + dx, iy + dy, iz + dz);
  };
  auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
  const float x00 = mix(corner(0, 0, 0), corner(1, 0, 0), sx);
  const float x10 = mix(corner(0, 1, 0), corner(1, 1, 0), sx);
  const float x01 = mix(corner(0, 0, 1), corner(1, 0, 1), sx);
  const float x11 = mix(corner(0, 1, 1), corner(1, 1, 1), sx);
  const float y0 = mix(x00, x10, sy);
  const float y1 = mix(x01, x11, sy);
  return mix(y0, y1, sz);
}

}  // namespace

void NoiseField::detail(int octaves, float falloff) {
  if (octaves > 0) m_octaves = octaves;
  if (falloff > 0.0f && falloff < 1.0f) m_falloff = falloff;
}

float NoiseField::corner(uint32_t seed, int x, int y, int z) {
  return (float)(core::noise::lattice(seed, x, y, z) & 0x00FFFFFFu) /
         16777216.0f;
}

float NoiseField::at(float x, float y, float z) const {
  // p5's sum: the first octave weighs a half and each next one `falloff`
  // of the one before, so the total stays inside [0, 1).
  float result = 0.0f;
  float amplitude = 0.5f;
  for (int octave = 0; octave < m_octaves; ++octave) {
    result += amplitude * sample(m_seed, x, y, z);
    amplitude *= m_falloff;
    x *= 2.0f;
    y *= 2.0f;
    z *= 2.0f;
  }
  return std::min(result, 0.99999994f);
}

}  // namespace sigil::draw
