#pragma once

/** @file
 * @ingroup core-compute
 *
 * Seeded, deterministic noise — the one place a mixer lives.
 *
 * Three integer mixers — a 64-bit avalanche, the PCG word and the
 * xorshift step — and the unit floats squeezed out of them. Every
 * function here is a bit-exact function of its inputs on every
 * platform, so anything seeded by them re-rolls identically.
 *
 * THE CONSTANTS AND THE SHIFT SCHEDULES ARE NOT TUNING KNOBS. Renders
 * stored as bytes are seeded through here, and a GPU kernel reproduces
 * `pcgAdvance`, `pcgMix` and `pcgHash` word for word. Changing a
 * constant does not fail a build — it re-rolls every stored render.
 */

#include <cstdint>

/** SEEDED, DETERMINISTIC NOISE — the one place a mixer lives. Integer
 *  mixers, the unit floats squeezed out of them, and the noise field
 *  read at a point. Every function here is a bit-exact function of its
 *  inputs on every platform, so anything seeded by them re-rolls
 *  identically: a scattered brush stamp, a roughened outline, a drifted
 *  point cloud, a jittered layout, and a shader's CPU twin. Reach for
 *  `chance` when a run of draws belongs to one held stream instead. */
namespace sigil::core::noise {

/** The odd increment a splitmix64 counter walks by. A counter stepped by
 *  it visits every 64-bit word before repeating, which is what makes
 *  successive draws from `mix64` uncorrelated rather than merely
 *  different. */
inline constexpr uint64_t kMix64Gamma = 0x9e3779b97f4a7c15ull;

/** THE 64-BIT AVALANCHE: two xor-shift-multiply rounds and a final
 *  xor-shift, so a one-bit change anywhere in @p z changes about half
 *  the result. A bijection, so a counter walked through it never
 *  repeats a value before the counter does. The stateless form is
 *  `mix64(x + kMix64Gamma)`; a stream is a counter advanced by the
 *  gamma and read through here. */
inline constexpr uint64_t mix64(uint64_t z) {
  z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27u)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31u);
}

/** A SPLITMIX64 STREAM: a 64-bit counter stepped by the gamma and read
 *  through `mix64`, with the unit floats squeezed out of it. What it
 *  buys over the PCG and xorshift streams is the 64-bit counter — two
 *  integers fold into one seed word with no mixing of the caller's own.
 *  Every draw takes the HIGH half of the avalanche.
 *  @trap A different function from `pcgNext` and `xorshiftNext`: work
 *  compared byte-for-byte against a stored render cannot swap one for
 *  another. Not a cryptographic generator. */
class Mix64Stream {
 public:
  explicit Mix64Stream(uint64_t seed) : m_state(seed) {}

  /** The next 32 bits: the counter steps by the gamma and the stepped
   *  value goes through the avalanche, whose high half is handed back. */
  uint32_t bits() {
    m_state += kMix64Gamma;
    return (uint32_t)(mix64(m_state) >> 32u);
  }
  /** The next value in [0, 1), through the 24 mantissa bits a float
   *  holds exactly. */
  float unit() { return (float)(bits() >> 8u) * (1.0f / 16777216.0f); }
  /** The next value in [-1, 1). */
  float signedUnit() { return unit() * 2.0f - 1.0f; }
  /** The next value in [lo, hi). */
  float range(float lo, float hi) { return lo + unit() * (hi - lo); }

 private:
  uint64_t m_state;
};

/** Hash of (seed, i) to [-1, 1): `mix64` over the pair, packed into one
 *  word. Successive `i` for one seed read as an uncorrelated sequence,
 *  which is what a per-stamp or per-vertex jitter wants. */
inline constexpr float hash(uint32_t seed, uint32_t i) {
  const uint64_t z =
      mix64((uint64_t(seed) << 32u | uint64_t(i * 0x9e3779b9u)) + kMix64Gamma);
  // Halved against 2^23 rather than 2^23 - 1: the divisor is exact as a
  // float, so the largest word lands just under 1 instead of a hair
  // above it, and the range the caller is promised is the range drawn.
  return (float)(z & 0xffffffu) * (1.0f / 8388608.0f) - 1.0f;
}

/** One PCG step: the LCG advance. */
inline constexpr uint32_t pcgAdvance(uint32_t state) {
  return state * 747796405u + 2891336453u;
}

/** The PCG output permutation (RXS-M-XS). */
inline constexpr uint32_t pcgMix(uint32_t x) {
  x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
  return (x >> 22u) ^ x;
}

/** Stateless hash of one integer: advance then mix. */
inline constexpr uint32_t pcgHash(uint32_t x) { return pcgMix(pcgAdvance(x)); }

/** A stream: advances `state` and returns the next word. */
inline constexpr uint32_t pcgNext(uint32_t& state) {
  state = pcgAdvance(state);
  return pcgMix(state);
}

/** `pcgNext` squeezed to [0, 1) through the top 24 bits — the 24 a
 *  float holds exactly, which is the one squeeze every unit draw in
 *  this tree is made by. Dividing the whole word by 0xFFFFFFFF instead
 *  answers exactly 1 for the largest word, because both it and the
 *  divisor round to 2^32 as floats. */
inline constexpr float pcgUnitNext(uint32_t& state) {
  return (float)(pcgNext(state) >> 8u) * (1.0f / 16777216.0f);
}

/** `pcgHash` squeezed to [0, 1) through the 24 mantissa bits a float
 *  can hold exactly — the same squeeze a shader performs, so CPU and GPU
 *  lattices agree. */
inline constexpr float pcgUnit(uint32_t x) {
  return (float)(pcgHash(x) & 0x00FFFFFFu) / 16777216.0f;
}

/** ONE XORSHIFT32 STEP, in the shift schedule 13 left, 17 right, 5
 *  left: advances @p state in place and returns it. A second stream
 *  beside the PCG one, not interchangeable with it in anything stored
 *  as bytes; new code takes `pcgUnitNext`.
 *  @trap Zero is the one state to keep out — all three shifts fix it,
 *  so a stream that reaches zero stays there. */
inline constexpr uint32_t xorshiftNext(uint32_t& state) {
  state ^= state << 13u;
  state ^= state >> 17u;
  state ^= state << 5u;
  return state;
}

/** `xorshiftNext` squeezed to [0, 1) through the top 24 bits — the 24 a
 *  float's mantissa holds exactly, taken from the high end, which is the
 *  end an xorshift word mixes best. */
inline constexpr float xorshiftUnitNext(uint32_t& state) {
  return (float)(xorshiftNext(state) >> 8u) * (1.0f / 16777216.0f);
}

/** THE 64-BIT XORSHIFT STREAM — the 13/7/17 schedule, which is the one a
 *  field seeded from a 64-bit word is usually written over, and a
 *  DIFFERENT mixer from the 32-bit one above rather than a wider version
 *  of it: the two visit different sequences, so a render stored from one
 *  cannot be reproduced by the other. A caller picks by which sequence
 *  its stored pixels came from. */
inline constexpr uint64_t xorshift64Next(uint64_t& state) {
  state ^= state << 13u;
  state ^= state >> 7u;
  state ^= state << 17u;
  return state;
}

/** `xorshift64Next` squeezed to [0, 1) through 24 bits taken from above
 *  the low eleven — the 24 a float's mantissa holds exactly, off the end
 *  a 64-bit xorshift mixes best. */
inline constexpr float xorshift64UnitNext(uint64_t& state) {
  return (float)((xorshift64Next(state) >> 11u) & 0xffffffu) *
         (1.0f / 16777216.0f);
}

/** THE LATTICE MIXER: three integer coordinates and a seed to one
 *  well-mixed word — what value noise asks at each corner of a cell,
 *  and what anything indexed by a grid position asks for a stable draw.
 *  The three coordinate weights are large odd words, so a step of one
 *  along any axis moves the sum far. The multiplies are meant to WRAP,
 *  and unsigned operands make that wrap the defined kind. */
[[nodiscard]] constexpr uint32_t lattice(uint32_t seed, int x, int y,
                                         int z) noexcept {
  uint32_t h = seed + (uint32_t)x * 374761393u + (uint32_t)y * 668265263u +
               (uint32_t)z * 2147483647u;
  h = (h ^ (h >> 13u)) * 1274126177u;
  return h ^ (h >> 16u);
}

}  // namespace sigil::core::noise
