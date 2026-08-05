#pragma once

#include <cstdint>

/** The one integer hash the library scatters with.
 *
 *  PARITY LOCK: this is the hash the world pop kernels
 *  (world/shaders/pop.slang and friends) implement in Slang, bit for
 *  bit. Pop.cpp's CPU executor must agree with the GPU executor
 *  numerically (World.PopCpuAndGpuExecutorsAgree), so the constants
 *  and the shift schedule here are ABI, not taste — do not retune
 *  them, and do not "improve" the mix.
 *
 *  It is split in two because the two callers enter at different
 *  points: Pop.cpp hashes a value (advance + mix), Points.cpp carries a
 *  PRNG state (advance the state, mix a copy of it). Composing the same
 *  two steps keeps both bit-identical to the single copies they
 *  replaced. VecMath.h's sibling note stands: the scatter BASIS stays
 *  literal at the Pop.cpp site; only this hash is shared. */
namespace sigil::shape::detail {

/** One LCG step (Numerical Recipes / PCG-32 constants). Unsigned
 *  overflow wraps, which is the intent. */
inline uint32_t pcgAdvance(uint32_t state) {
  return state * 747796405u + 2891336453u;
}

/** The PCG output permutation: a variable xorshift chosen by the top
 *  nibble, a multiply, then a fixed xorshift. */
inline uint32_t pcgMix(uint32_t x) {
  x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
  return (x >> 22u) ^ x;
}

/** Stateless hash of @p x — advance then mix. */
inline uint32_t pcgHash(uint32_t x) { return pcgMix(pcgAdvance(x)); }

}  // namespace sigil::shape::detail
