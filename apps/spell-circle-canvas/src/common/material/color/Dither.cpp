/** @file
 * The two threshold patterns, and the rounding read through them.
 */

#include "sigilmaterial/color/Dither.h"

#include <algorithm>
#include <cmath>

namespace sigil::material {
namespace {

/** The matrix side as a power of two between 2 and 16, and the count of
 *  bits that side takes. */
int matrixBits(int matrix) {
  int bits = 1;
  while ((1 << (bits + 1)) <= std::clamp(matrix, 2, 16)) ++bits;
  return bits;
}

/** THE BAYER VALUE at a cell of a 2^bits matrix, in [0, 4^bits).
 *
 *  The recursion that defines the matrix — each cell of the small
 *  matrix becoming a quadrant of the next, at four times the value plus
 *  a fixed offset — is exactly this walk down the coordinate bits, most
 *  significant first, taking two bits of the answer from each. Writing
 *  it as the walk rather than as a table is what lets one body answer
 *  for every size. */
uint32_t bayer(uint32_t x, uint32_t y, int bits) {
  uint32_t value = 0;
  for (int i = bits - 1; i >= 0; --i) {
    const uint32_t xi = (x >> i) & 1u;
    const uint32_t yi = (y >> i) & 1u;
    value = (value << 2) | ((xi ^ yi) << 1) | yi;
  }
  return value;
}

/** A hash of two coordinates whose values are spread rather than
 *  clumped: the interleaved-gradient construction, which is two
 *  irrational-looking slopes read through one fractional part. Its
 *  neighbours differ, which is the property a threshold needs and a
 *  plain avalanche hash does not have to have. */
float screenNoise(int x, int y) {
  const float slope = 0.06711056f * (float)x + 0.00583715f * (float)y;
  const float folded = slope - std::floor(slope);
  const float value = 52.9829189f * folded;
  return value - std::floor(value);
}

}  // namespace

float Dither::threshold(int x, int y) const {
  if (kind == DitherKind::Noise) return screenNoise(x, y);
  const int bits = matrixBits(matrix);
  const int side = 1 << bits;
  // A negative coordinate folds forward rather than mirroring, so the
  // tiling is continuous across the origin.
  const uint32_t cx = (uint32_t)(((x % side) + side) % side);
  const uint32_t cy = (uint32_t)(((y % side) + side) % side);
  const uint32_t cells = (uint32_t)(side * side);
  // Plus a half: the cell's value stands for the band it opens, so the
  // thresholds sit at the centres of `cells` bands and their average is
  // exactly one half.
  return ((float)bayer(cx, cy, bits) + 0.5f) / (float)cells;
}

Color Dither::at(const Color& color, int x, int y) const {
  const int steps = std::max(levels, 2) - 1;
  const float step = 1.0f / (float)steps;
  const float offset = (threshold(x, y) - 0.5f) * amount * step;
  auto channel = [&](float v) {
    const float shifted = std::clamp(v + offset, 0.0f, 1.0f);
    return std::round(shifted * (float)steps) / (float)steps;
  };
  return {channel(color.r), channel(color.g), channel(color.b), color.a};
}

bool Dither::on(float value, int x, int y) const {
  return value > 0.5f + (threshold(x, y) - 0.5f) * amount;
}

}  // namespace sigil::material
