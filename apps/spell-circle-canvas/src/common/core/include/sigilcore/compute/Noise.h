#pragma once

/** @file
 * Seeded, deterministic noise — the one place a mixer lives.
 *
 * Three integer mixers — a 64-bit avalanche, the PCG word and the
 * xorshift step — and the unit floats squeezed out of them. Every
 * function here is a bit-exact
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
 * `hash`, `lattice`, `pcgHash` and `xorshiftNext` are different mixers
 * with different outputs, kept side by side because each seeds work that
 * is compared byte-for-byte against stored renders. Pick by what the
 * caller already uses; new code takes `pcgHash`.
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

/** A SPLITMIX64 STREAM: a 64-bit counter stepped by the gamma and read
 *  through `mix64`, with the unit floats squeezed out of it.
 *
 *  This is the stream form of `mix64`, and it is a different function
 *  from `pcgNext` and `xorshiftNext` in the way the top of this file
 *  states: seeded work compared byte-for-byte against a stored render
 *  cannot swap one for another. What this one buys over those two is the
 *  64-bit counter — a caller with two integers to fold into a seed (a
 *  glyph's index within its run, and the run's within its text) packs
 *  them into one word with no mixing of its own.
 *
 *  Every draw takes the HIGH half of the avalanche, which is the half a
 *  splitmix64 mixes best. Not a cryptographic generator and not a
 *  substitute for one. */
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

/** ONE XORSHIFT32 STEP, in the shift schedule 13 left, 17 right, 5 left:
 *  advances @p state in place and returns it.
 *
 *  A second stream beside the PCG one, for the same reason the constants
 *  above are fixed: a scatter already keyed to these three shifts draws
 *  a different sequence from `pcgNext`, so the two are not
 *  interchangeable in anything stored as bytes. New code takes
 *  `pcgUnitNext`.
 *
 *  Zero is the one state to keep out: all three shifts fix it, so a
 *  stream that reaches zero stays there. Seed with any other word. */
inline uint32_t xorshiftNext(uint32_t& state) {
  state ^= state << 13u;
  state ^= state >> 17u;
  state ^= state << 5u;
  return state;
}

/** `xorshiftNext` squeezed to [0, 1) through the top 24 bits — the 24 a
 *  float's mantissa holds exactly, taken from the high end, which is the
 *  end an xorshift word mixes best. */
inline float xorshiftUnitNext(uint32_t& state) {
  return (float)(xorshiftNext(state) >> 8u) * (1.0f / 16777216.0f);
}

/** THE LATTICE MIXER: three integer coordinates and a seed to one
 *  well-mixed word — what value noise asks at each corner of a cell, and
 *  what anything indexed by a grid position asks for a stable draw.
 *
 *  The three coordinate weights are large odd words, so a step of one
 *  along any axis moves the sum far; the xor-shift-multiply and the
 *  final fold are what turn that sum into an avalanche. THE CONSTANTS
 *  AND THE SHIFTS ARE NOT TUNING KNOBS, for the reason stated at the top
 *  of this file.
 *
 *  The multiplies are meant to WRAP. Unsigned operands make that wrap
 *  the defined kind, where signed ones would overflow; the bits are the
 *  same either way. */
[[nodiscard]] constexpr uint32_t lattice(uint32_t seed, int x, int y,
                                         int z) noexcept {
  uint32_t h = seed + (uint32_t)x * 374761393u + (uint32_t)y * 668265263u +
               (uint32_t)z * 2147483647u;
  h = (h ^ (h >> 13u)) * 1274126177u;
  return h ^ (h >> 16u);
}

}  // namespace sigil::core::noise
