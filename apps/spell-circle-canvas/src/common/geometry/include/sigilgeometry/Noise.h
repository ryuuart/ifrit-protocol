#pragma once
/** @file
 * Seeded, deterministic noise — the one place a hash lives.
 *
 * Two integer mixers and the value noise built on them. Both are
 * bit-exact functions of their inputs on every platform, so anything
 * seeded by them re-rolls identically: a scattered brush stamp, a
 * roughened outline, a drifted point cloud, a jittered layout. The
 * SkSL twin of `pcgHash` is in the material library so a shader and its
 * CPU preview agree to the bit.
 *
 * `hash` and `pcgHash` are different mixers with different outputs, kept
 * side by side because each seeds work that is compared byte-for-byte
 * against stored renders. Pick by what the caller already uses; new
 * code takes `pcgHash`.
 */
#include <cstdint>
#include <glm/vec3.hpp>

namespace sigil::geometry::noise {

/** Hash of (seed, i) to [-1, 1]: a splitmix64 finalizer over the pair.
 *  Successive `i` for one seed read as an uncorrelated sequence, which is
 *  what a per-stamp or per-vertex jitter wants. */
inline float hash(uint32_t seed, uint32_t i) {
  uint64_t z =
      (uint64_t(seed) << 32 | (i * 0x9e3779b9u)) + 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  z ^= z >> 31;
  return (float)(z & 0xffffff) / (float)0x7fffff - 1.0f;
}

/** One PCG step: the LCG advance. */
inline uint32_t pcgAdvance(uint32_t state) {
  return state * 747796405u + 2891336453u;
}

/** The PCG output permutation (RXS-M-XS). */
inline uint32_t pcgMix(uint32_t x) {
  x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
  return (x >> 22u) ^ x;
}

/** Stateless hash of one integer: advance then mix. */
inline uint32_t pcgHash(uint32_t x) { return pcgMix(pcgAdvance(x)); }

/** A stream: advances `state` and returns the next word. */
inline uint32_t pcgNext(uint32_t& state) {
  state = pcgAdvance(state);
  return pcgMix(state);
}

/** `pcgNext` squeezed to [0, 1): the next unit float of a stream. */
inline float pcgUnitNext(uint32_t& state) {
  return (float)pcgNext(state) / (float)0xFFFFFFFFu;
}

/** `pcgHash` squeezed to [0, 1) through the 24 mantissa bits a float
 *  can hold exactly — the same squeeze a shader performs, so CPU and GPU
 *  lattices agree. */
inline float pcgUnit(uint32_t x) {
  return (float)(pcgHash(x) & 0x00FFFFFFu) / 16777216.0f;
}

/** Trilinear value noise over the integer lattice, in [-1, 1], seeded. */
float value3(glm::vec3 p, uint32_t seed);

}  // namespace sigil::geometry::noise
