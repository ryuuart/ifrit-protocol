/** @file
 * `BoundFloat::apply()`: one sample of the bound Output run through the
 * chain in its fixed stage order — and, under the pin that keeps them
 * honest, the two comparators an identity prune reads a shaped binding
 * through.
 */

#include "sigilmotion/bind/BoundFloat.h"

#include <sigilcore/comparable/Fields.h>

#include <cmath>

#include "sigilmotion/bind/WiggleNoise.h"

namespace sigil::motion {

float BoundFloat::apply(float value) const {
  value = value * inputScale + inputOffset;
  if (clampInput) value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
  // The noise PHASE is read here — off the normalised input, before the
  // curve shapes it and before the affine chain puts it in output
  // units. Phase from the schedule, amplitude in output units; see
  // Bound::wiggle for why that pairing is the useful one.
  const float phase = value;
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
      const float folded = value - std::floor(value);
      value = folded < 0.5f ? folded + folded : 2.0f - (folded + folded);
      break;
    }
    case Envelope::kCosine:
      // Periodic by construction — the same repeat, from the cosine
      // itself rather than from a fold.
      value = 0.5f - 0.5f * std::cos(6.28318530717958648f * value);
      break;
    case Envelope::kTrapezoid:
      // Dark outside [riseStart, fallEnd] — NOT folded into one span,
      // because `window()` clamps its input to exactly 1 and a fold would
      // read that as the START of the next pass, turning a settled beat
      // dark. A repeating trapezoid rides a phase that already wraps.
      if (value <= riseStart || value >= fallEnd)
        value = 0.0f;
      else if (value < holdStart)
        // Reachable only when the corners differ, because the test above
        // already answered every value at or below riseStart — which is what
        // makes a zero-length shoulder an instant cut rather than a
        // division by zero.
        value = (value - riseStart) / (holdStart - riseStart);
      else if (value <= holdEnd)
        value = 1.0f;
      else
        value = (fallEnd - value) / (fallEnd - holdEnd);
      break;
    case Envelope::kSquare: {
      // The pulse, folded on the same period pingPong folds on: 1 across
      // the first `duty` of each period, 0 across the rest. PHASE 0 IS
      // ON — `folded < duty` answers 1 at exactly 0 — because a pulse's owner
      // is born at the start of its cycle and must be born visible; a
      // shape answering 0 at exactly 0 would blank that one instant at
      // every seam of a wrapping phase.
      const float folded = value - std::floor(value);
      value = folded < duty ? 1.0f : 0.0f;
      break;
    }
    case Envelope::kWave: {
      // The caller's own shape, read on the folded phase, in [0,1) — so
      // whatever the function draws across one period, the signal
      // repeats it. An empty function passes the folded phase through.
      const float folded = value - std::floor(value);
      value = waveFunction ? waveFunction(folded) : folded;
      break;
    }
  }
  if (curve) value = curve(value);
  if (steps > 1) value = std::round(value * (float)(steps - 1)) / (float)(steps - 1);
  value = value * scale + offset;
  // AFTER the affine chain, BEFORE wiggle: the noise phase above reads
  // the unwrapped schedule, so a wrapped phase wiggles continuously
  // across the seam instead of repeating its shake every lap.
  if (wrapPeriod > 0.0f) {
    value = std::fmod(value, wrapPeriod);
    if (value < 0.0f) value += wrapPeriod;
  }
  if (wiggleAmount != 0.0f)
    value += wiggleAmount * detail::wiggleNoise(phase * wiggleFrequency, wiggleSeed,
                                            wiggleOctaves, wiggleFalloff);
  if (clamped) value = value < low ? low : (value > high ? high : value);
  return value;
}

static_assert(core::kFieldCount<BoundFloat> == 24,
              "BoundFloat gained or lost a field. boundMapEqual() below "
              "compares it BY HAND: rule on the new field (participate, or "
              "a stated reason not to), then bump this count. A miss is "
              "silent — the node prunes and keeps shaping through the old "
              "map forever.");
bool boundMapEqual(const BoundFloat& left, const BoundFloat& right) {
  return left.source == right.source && left.inputScale == right.inputScale &&
         left.inputOffset == right.inputOffset && left.clampInput == right.clampInput &&
         left.envelope == right.envelope && left.riseStart == right.riseStart &&
         left.holdStart == right.holdStart && left.holdEnd == right.holdEnd &&
         left.fallEnd == right.fallEnd && left.duty == right.duty && left.steps == right.steps &&
         left.scale == right.scale && left.offset == right.offset && left.clamped == right.clamped &&
         left.low == right.low && left.high == right.high && left.wiggleAmount == right.wiggleAmount &&
         left.wiggleFrequency == right.wiggleFrequency &&
         left.wiggleSeed == right.wiggleSeed && left.wiggleOctaves == right.wiggleOctaves &&
         left.wiggleFalloff == right.wiggleFalloff && left.wrapPeriod == right.wrapPeriod &&
         // The two curve slots compare under the same conservative rule: a
         // plain function is compared by identity, a capturing lambda is
         // unequal to everything and the binding re-patches every describe.
         easeEqual(left.curve, right.curve) &&
         easeEqual(left.waveFunction, right.waveFunction);
}

}  // namespace sigil::motion
