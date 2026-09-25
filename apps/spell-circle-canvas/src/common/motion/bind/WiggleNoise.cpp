/** @file
 * The wiggle noise field: the avalanche hash, the seeded lattice, the
 * quintic-smoothed octave and the weight-normalised fractal sum.
 */

#include "sigilmotion/bind/WiggleNoise.h"

#include <cmath>

namespace sigil::motion {

namespace detail {

uint32_t wiggleHash(uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return value;
}

float wiggleLattice(int32_t cell, uint32_t seed) {
  const uint32_t hashed =
      wiggleHash((uint32_t)cell * 0x9e3779b9u ^ wiggleHash(seed + 0x85ebca6bu));
  return (float)(hashed >> 8u) * (1.0f / 8388608.0f) - 1.0f;
}

float wiggleOctave(float phase, uint32_t seed) {
  const float base = std::floor(phase);
  if (base <= -2.0e9f || base >= 2.0e9f) return 0.0f;
  const float fraction = phase - base;
  const int32_t cell = (int32_t)base;
  const float fade = fraction * fraction * fraction * (fraction * (fraction * 6.0f - 15.0f) + 10.0f);
  const float leftValue = wiggleLattice(cell, seed);
  const float rightValue = wiggleLattice(cell + 1, seed);
  return leftValue + (rightValue - leftValue) * fade;
}

float wiggleNoise(float phase, uint32_t seed, int octaves, float falloff) {
  const int count = octaves < 1 ? 1 : (octaves > 8 ? 8 : octaves);
  float sum = 0.0f, weight = 0.0f, amplitude = 1.0f, frequency = 1.0f;
  for (int i = 0; i < count; ++i) {
    sum += amplitude * wiggleOctave(phase * frequency, seed + (uint32_t)i * 0x9e3779b9u);
    weight += amplitude;
    amplitude *= falloff;
    frequency *= 2.0f;
  }
  return weight > 0.0f ? sum / weight : 0.0f;
}

}  // namespace detail

}  // namespace sigil::motion
