#pragma once

/** @file
 * The arithmetic over a clock reading: seconds posterised at a declared
 * rate, the step INDEX that rate is on, seconds folded into a wrapping
 * [0, 1) loop, the open-ended settle a time constant describes, and the
 * one-shot flash built on it.
 */

#include <cmath>
#include <cstdint>

namespace sigil::motion {

/** Posterize TIME at a declared rate: `floor(t·hz)/hz`, held still
 *  between steps.
 *
 *  Not the same operation as `Bound::quantize`, despite the similar name.
 *  This one snaps unbounded seconds to a RATE; that one snaps a
 *  normalised [0,1] value to a COUNT OF LEVELS. The different word marks
 *  the different contract.
 *
 *  A template on ONE type, so every call site keeps its own precision:
 *  `quantizeTime(t, 8.0)` is double math, `quantizeTime(ft, 8.0f)` is
 *  float. That makes replacing a hand-written `std::floor(t * hz) / hz`
 *  with this call bit-identical rather than merely close.
 *
 *  `hz <= 0` answers the input unchanged — the spelling of
 *  "continuous". */
template <typename T>
inline T quantizeTime(T t, T hz) {
  return hz > T(0) ? std::floor(t * hz) / hz : t;
}

/** WHICH STEP a posterised clock is on: `floor(t·hz)` as an integer, the
 *  counterpart of `quantizeTime`'s re-emitted seconds.
 *
 *  The two answer the same fact and are not interchangeable. A value
 *  driven by a held clock wants the seconds; anything that must know
 *  WHICH tick it is looking at — reseeding a scramble once per step,
 *  advancing a cursor, indexing a table of frames — wants the count, and
 *  recovering it from the seconds means dividing back out by the rate and
 *  rounding, which is a second place for the two to disagree.
 *
 *  A `long long` because a monotonic clock at 60 Hz outruns a 32-bit
 *  count in under a year of running. `hz <= 0` answers 0 — the spelling
 *  of "continuous", which is on no step at all. */
inline long long stepIndex(double t, double hz) {
  return hz > 0 ? (long long)std::floor(t * hz) : 0;
}

/** A wrapping phase in [0, 1): `t` seconds over a `period`-second loop —
 *  the marching-ants offset, the orbiting comet, the scrolling marquee,
 *  the scanline creep.
 *
 *  A non-positive period gives 0 rather than the NaN the bare `fmod` would
 *  produce, and a negative `t` wraps forward instead of returning a
 *  negative phase, so the result is always in range whatever the caller
 *  hands in.
 *
 *  Deliberately narrow. The two neighbouring signals, `0.5 + 0.5·sin(t·k)`
 *  and `min(1, t/k)`, are one short expression each and are not here. */
inline float phase(double t, double period) {
  if (!(period > 0)) return 0.0f;
  const double p = std::fmod(t / period, 1.0);
  return (float)(p < 0 ? p + 1.0 : p);
}

/** AN OPEN-ENDED SETTLE: `exp(-age/tau)`, 1 at the instant a thing
 *  happened and decaying towards 0 for as long as it is remembered — the
 *  ripple that fades, the hit that cools, the trail that thins.
 *
 *  Not an easing curve, and the reason is the shape of the question. An
 *  `ease::` curve maps a NORMALISED progress: it needs a duration, and it
 *  arrives at exactly 0 or 1 at a stated moment. This takes an AGE in
 *  whatever unit `tau` is in, has no end, and never quite reaches 0. A
 *  non-positive `tau` answers 0 — no memory at all — rather than dividing
 *  by zero. */
inline float decay(float age, float tau) {
  return tau > 0.0f ? std::exp(-age / tau) : 0.0f;
}

/** A ONE-SHOT FLASH: up over @p attack seconds, then down towards @p rest
 *  on a time constant — the strike, the muzzle flare, the hit that lands
 *  and cools, the lamp that comes on hot and settles to its burn.
 *
 *  Three numbers, and each one is a different half of the shape. @p
 *  attack is how long the rise takes, and it is LINEAR: the thing that
 *  makes a flash read as hot is that it arrives in a frame or two and
 *  leaves over half a second, so an eased rise only softens what should
 *  be the sharpest edge in the envelope. @p tau is the fall's time
 *  constant, in the same unit as @p age. @p rest is where the fall is
 *  headed — 0 for a flare that goes out, above 0 for a thing that stays
 *  lit at a lower level once it has flared.
 *
 *  Exactly 1 at the crest whatever `rest` is, so the crest is the same
 *  height for a flare and for a lamp and the two can be mixed without
 *  rescaling. Before the event (`age < 0`) it is 0, not `rest`: nothing
 *  has happened yet. A non-positive @p attack is the instantaneous rise
 *  — 1 at age 0 — and a non-positive @p tau holds the crest rather than
 *  dividing by zero.
 *
 *  `decay` is this envelope's fall alone, and the two agree: `flash(age,
 *  0, tau, 0)` and `decay(age, tau)` are the same number for a
 *  non-negative age. Reach for `decay` when there is no attack to
 *  describe. */
inline float flash(float age, float attack, float tau, float rest = 0.0f) {
  if (age < 0.0f) return 0.0f;
  if (age < attack) return age / attack;
  if (!(tau > 0.0f)) return 1.0f;
  return rest + (1.0f - rest) * std::exp(-(age - attack) / tau);
}

}  // namespace sigil::motion
