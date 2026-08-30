/** @file
 * The wiggle noise field: the avalanche hash, the seeded lattice, the
 * quintic-smoothed octave and the weight-normalised fractal sum.
 */

#include "sigilmotion/bind/WiggleNoise.h"

#include <cmath>

namespace sigil::motion {

namespace detail {

uint32_t wiggleHash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

float wiggleLattice(int32_t cell, uint32_t seed) {
  const uint32_t h =
      wiggleHash((uint32_t)cell * 0x9e3779b9u ^ wiggleHash(seed + 0x85ebca6bu));
  return (float)(h >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

float wiggleOctave(float x, uint32_t seed) {
  const float base = std::floor(x);
  if (!(base > -2.0e9f && base < 2.0e9f)) return 0.0f;
  const float t = x - base;
  const int32_t cell = (int32_t)base;
  const float u = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
  const float a = wiggleLattice(cell, seed);
  const float b = wiggleLattice(cell + 1, seed);
  return a + (b - a) * u;
}

float wiggleNoise(float x, uint32_t seed, int octaves, float falloff) {
  const int n = octaves < 1 ? 1 : (octaves > 8 ? 8 : octaves);
  float sum = 0.0f, weight = 0.0f, amp = 1.0f, freq = 1.0f;
  for (int i = 0; i < n; ++i) {
    sum += amp * wiggleOctave(x * freq, seed + (uint32_t)i * 0x9e3779b9u);
    weight += amp;
    amp *= falloff;
    freq *= 2.0f;
  }
  return weight > 0.0f ? sum / weight : 0.0f;
}

}  // namespace detail

}  // namespace sigil::motion
