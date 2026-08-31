#pragma once

/** @file
 * The value-noise field behind `Bound::wiggle()`: a 32-bit avalanche
 * hash, the seeded lattice, one quintic-smoothed octave, and the
 * normalised fractal sum. Integers and cmath only.
 */

#include <cstdint>

namespace sigil::motion {

namespace detail {

/** The noise field behind `Bound::wiggle()`. Deliberately private to this
 *  library and not shared with any GPU hash: those are bit-matched to
 *  compute kernels and pinned by parity tests, so borrowing one would tie
 *  an animation curve to a GPU ABI and let a future parity fix silently
 *  move every wiggle written against this one.
 *
 *  A 32-bit avalanche: every input bit reaches every output bit, so
 *  adjacent lattice cells and adjacent SEEDS come out uncorrelated. */
uint32_t wiggleHash(uint32_t x);

/** The lattice value at integer cell @p cell for @p seed, in [-1, 1).
 *  The seed is hashed BEFORE it is mixed with the cell, so seeds 0 and 1
 *  are as independent as seeds 0 and 5731. */
float wiggleLattice(int32_t cell, uint32_t seed);

/** ONE octave of 1-D VALUE noise, quintic-smoothed.
 *
 *  Value noise, not white noise, is the whole point: `wiggle()` exists
 *  because a property that teleports to a new random number every sample
 *  is not what any motion designer means by a shake — it reads as
 *  strobing, not as movement. The quintic fade (`6t⁵−15t⁴+10t³`,
 *  Perlin's improved-noise curve) is C² at the lattice, so neither the
 *  value nor its velocity nor its acceleration kinks as the phase
 *  crosses a cell boundary; the cheaper cubic smoothstep leaves a visible
 *  tick in a slow drift.
 *
 *  Out-of-range phases answer 0 rather than reinterpreting a float too
 *  large for `int32_t` (UB). A phase that big is a bug upstream, and a
 *  frozen wiggle is a debuggable symptom where UB is not. */
float wiggleOctave(float x, uint32_t seed);

/** Fractal sum of @p octaves octaves, each half the wavelength and
 *  @p falloff the amplitude of the one before — and NORMALISED by the
 *  weight sum, so the result stays in [-1, 1] whatever the octave count.
 *
 *  The normalisation is the point. `Bound::wiggle`'s `amount` promises a
 *  peak displacement in the property's own units, and a promise that
 *  changed as you added detail would not be one. Adding an octave here
 *  changes the TEXTURE, never the size. (After Effects does not
 *  normalise, which is why adding octaves there gets louder and has to be
 *  corrected by hand.) */
float wiggleNoise(float x, uint32_t seed, int octaves, float falloff);

}  // namespace detail

}  // namespace sigil::motion
