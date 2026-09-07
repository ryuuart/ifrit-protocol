/** @file
 * `BoundFloat::apply()`: one sample of the bound Output run through the
 * chain in its fixed stage order — and, under the pin that keeps them
 * honest, the two comparators an identity prune reads a shaped binding
 * through.
 */

#include "sigilmotion/bind/BoundFloat.h"

#include <sigilcore/comparable/Fields.h>

#include <cmath>

#include "sigilmotion/bind/Curve.h"
#include "sigilmotion/bind/WiggleNoise.h"

namespace sigil::motion {

float BoundFloat::apply(float v) const {
  v = v * inScale + inOffset;
  if (clampInput) v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
  // The noise PHASE is read here — off the normalised input, before the
  // curve shapes it and before the affine chain puts it in output
  // units. Phase from the schedule, amplitude in output units; see
  // Bound::wiggle for why that pairing is the useful one.
  const float phase = v;
  // THE ENVELOPE, on the phase and before the curve. It sits after the
  // noise phase is read for the same reason `wrap` does: the shake reads
  // the SCHEDULE, so a ping-ponged phase does not retrace the identical
  // shake on the way back and a trapezoid's dark stretch does not freeze
  // it. It sits before `map` so the curve shapes what the envelope
  // PRODUCED — any curve through (0,0) and (1,1) rounds a trapezoid's
  // shoulders while leaving its hold at exactly 1 and its dark at exactly
  // 0 — and before the affine chain, which is what puts the shape into
  // the property's own units.
  switch (envelope) {
    case Envelope::kNone:
      break;
    case Envelope::kPingPong: {
      // A triangle of period 1: there and back across the span, and then
      // again, so a phase that keeps climbing keeps bouncing.
      const float u = v - std::floor(v);
      v = u < 0.5f ? u + u : 2.0f - (u + u);
      break;
    }
    case Envelope::kCosine:
      // Periodic by construction — the same repeat, from the cosine
      // itself rather than from a fold.
      v = 0.5f - 0.5f * std::cos(6.28318530717958648f * v);
      break;
    case Envelope::kTrapezoid:
      // Dark outside [riseStart, fallEnd] — NOT folded into one span,
      // because `window()` clamps its input to exactly 1 and a fold would
      // read that as the START of the next pass, turning a settled beat
      // dark. A repeating trapezoid rides a phase that already wraps.
      if (v <= riseStart || v >= fallEnd)
        v = 0.0f;
      else if (v < holdStart)
        // Reachable only when the corners differ, because the test above
        // already answered every v at or below riseStart — which is what
        // makes a zero-length shoulder an instant cut rather than a
        // division by zero.
        v = (v - riseStart) / (holdStart - riseStart);
      else if (v <= holdEnd)
        v = 1.0f;
      else
        v = (fallEnd - v) / (fallEnd - holdEnd);
      break;
    case Envelope::kSquare: {
      // The pulse, folded on the same period pingPong folds on: 1 across
      // the first `duty` of each period, 0 across the rest. PHASE 0 IS
      // ON — `u < duty` answers 1 at exactly 0 — because a pulse's owner
      // is born at the start of its cycle and must be born visible; a
      // shape answering 0 at exactly 0 would blank that one instant at
      // every seam of a wrapping phase.
      const float u = v - std::floor(v);
      v = u < duty ? 1.0f : 0.0f;
      break;
    }
    case Envelope::kWave: {
      // The caller's own shape, read on the folded phase u ∈ [0,1) — so
      // whatever the function draws across one period, the signal
      // repeats it. An empty function passes the folded phase through.
      const float u = v - std::floor(v);
      v = waveFn ? waveFn(u) : u;
      break;
    }
  }
  if (curve) v = curve(v);
  if (steps > 1) v = std::round(v * (float)(steps - 1)) / (float)(steps - 1);
  v = v * scale + offset;
  // AFTER the affine chain, BEFORE wiggle: the noise phase above reads
  // the unwrapped schedule, so a wrapped phase wiggles continuously
  // across the seam instead of repeating its shake every lap.
  if (wrapPeriod > 0.0f) {
    v = std::fmod(v, wrapPeriod);
    if (v < 0.0f) v += wrapPeriod;
  }
  if (wiggleAmount != 0.0f)
    v += wiggleAmount * detail::wiggleNoise(phase * wiggleFrequency, wiggleSeed,
                                            wiggleOctaves, wiggleFalloff);
  if (clamped) v = v < lo ? lo : (v > hi ? hi : v);
  return v;
}

bool easeEqual(const choreograph::EaseFn& a, const choreograph::EaseFn& b) {
  const bool aSet = (bool)a, bSet = (bool)b;
  if (aSet != bSet) return false;
  if (!aSet) return true;
  using Ptr = float (*)(float);
  if (const Ptr* pa = a.target<Ptr>(); pa) {
    const Ptr* pb = b.target<Ptr>();
    return pb && *pa == *pb;
  }
  // A shaped curve keeps its shape and its numbers where they can be read
  // back (see ease::Curve); anything else is a lambda and stays unequal.
  if (const ease::Curve* ca = a.target<ease::Curve>(); ca) {
    const ease::Curve* cb = b.target<ease::Curve>();
    return cb && *ca == *cb;
  }
  return false;
}

static_assert(core::kFieldCount<BoundFloat> == 24,
              "BoundFloat gained or lost a field. boundMapEqual() below "
              "compares it BY HAND: rule on the new field (participate, or "
              "a stated reason not to), then bump this count. A miss is "
              "silent — the node prunes and keeps shaping through the old "
              "map forever.");
bool boundMapEqual(const BoundFloat& a, const BoundFloat& b) {
  return a.source == b.source && a.inScale == b.inScale &&
         a.inOffset == b.inOffset && a.clampInput == b.clampInput &&
         a.envelope == b.envelope && a.riseStart == b.riseStart &&
         a.holdStart == b.holdStart && a.holdEnd == b.holdEnd &&
         a.fallEnd == b.fallEnd && a.duty == b.duty && a.steps == b.steps &&
         a.scale == b.scale && a.offset == b.offset && a.clamped == b.clamped &&
         a.lo == b.lo && a.hi == b.hi && a.wiggleAmount == b.wiggleAmount &&
         a.wiggleFrequency == b.wiggleFrequency &&
         a.wiggleSeed == b.wiggleSeed && a.wiggleOctaves == b.wiggleOctaves &&
         a.wiggleFalloff == b.wiggleFalloff && a.wrapPeriod == b.wrapPeriod &&
         // The two curve slots compare under the same conservative rule: a
         // plain function is compared by identity, a capturing lambda is
         // unequal to everything and the binding re-patches every describe.
         easeEqual(a.curve, b.curve) && easeEqual(a.waveFn, b.waveFn);
}

}  // namespace sigil::motion
