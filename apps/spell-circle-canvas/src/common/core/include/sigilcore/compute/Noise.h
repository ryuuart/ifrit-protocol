#pragma once

/** @file
 * Seeded, deterministic noise — the one place a mixer lives.
 *
 * Two integer mixers — a 64-bit avalanche and the PCG word — and the unit
 * floats squeezed out of them. Every function here is a bit-exact
 * function of its inputs on every platform, so anything seeded by them
 * re-rolls identically: a scattered brush stamp, a roughened outline, a
 * drifted point cloud, a jittered layout.
 * A shader that has to agree with a CPU preview to the bit reproduces
 * `pcgHash` and `pcgUnit` rather than inventing its own; the
 * point-operator compute kernel does exactly that.
 *
 * THE CONSTANTS AND THE SHIFT SCHEDULES ARE NOT TUNING KNOBS. Renders
 * stored as bytes are seeded through here, and a GPU kernel reproduces
 * `pcgAdvance`, `pcgMix` and `pcgHash` word for word. Changing a
 * constant does not fail a build — it re-rolls every stored render and
 * desynchronizes the two ends of every operator chain that runs on both.
 *
 * `hash` and `pcgHash` are different mixers with different outputs, kept
 * side by side because each seeds work that is compared byte-for-byte
 * against stored renders. Pick by what the caller already uses; new
 * code takes `pcgHash`.
 */

#include <cstdint>

namespace sigil::core::noise {

/** The odd increment a splitmix64 counter walks by. A counter stepped by
 *  it visits every 64-bit word before repeating, which is what makes
 *  successive draws from `mix64` uncorrelated rather than merely
 *  different. */
inline constexpr uint64_t kMix64Gamma = 0x9e3779b97f4a7c15ull;

/** The 64-bit avalanche: two xor-shift-multiply rounds and a final
 *  xor-shift, so a one-bit change anywhere in @p z changes about half
 *  the result. It is a bijection — every input maps to its own output —
 *  so a counter walked through it never repeats a value before the
 *  counter does.
 *
 *  The stateless form is `mix64(x + kMix64Gamma)`; a stream is a counter
 *  advanced by the gamma and read through here. */
inline uint64_t mix64(uint64_t z) {
  z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27u)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31u);
}

/** Hash of (seed, i) to [-1, 1]: `mix64` over the pair, packed into one
 *  word. Successive `i` for one seed read as an uncorrelated sequence,
 *  which is what a per-stamp or per-vertex jitter wants. */
inline float hash(uint32_t seed, uint32_t i) {
  const uint64_t z =
      mix64((uint64_t(seed) << 32u | uint64_t(i * 0x9e3779b9u)) + kMix64Gamma);
  return (float)(z & 0xffffffu) / (float)0x7fffff - 1.0f;
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

}  // namespace sigil::core::noise
