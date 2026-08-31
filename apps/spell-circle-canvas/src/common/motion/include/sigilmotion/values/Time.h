#pragma once

/** @file
 * Two arithmetic helpers over the clock: `quantizeTime()`, seconds
 * posterised at a declared rate, and `phase()`, seconds folded into a
 * wrapping [0, 1) loop.
 */

#include <cmath>

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

}  // namespace sigil::motion
