/** @file
 * `Binding::apply()`: one sample of the followed value run through the
 * stages in their declared order — and, under the pin that keeps it
 * honest, the equality an identity prune reads a binding through.
 */

#include "sigilmotion/bind/Binding.h"

#include <sigilcore/comparable/Fields.h>

#include <cmath>

#include "sigilmotion/bind/WiggleNoise.h"

namespace sigil::motion {

namespace {

float unitClamp(float value) {
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

}  // namespace

bool Envelope::operator==(const Envelope& other) const {
  return shape == other.shape && riseStart == other.riseStart &&
         holdStart == other.holdStart && holdEnd == other.holdEnd &&
         fallEnd == other.fallEnd && duty == other.duty &&
         easeEqual(curve, other.curve);
}

namespace envelope {

Envelope cosine() { return {.shape = Envelope::Shape::Cosine}; }

Envelope square(float duty) {
  return {.shape = Envelope::Shape::Square, .duty = unitClamp(duty)};
}

Envelope trapezoid(float riseStart, float holdStart, float holdEnd,
                   float fallEnd) {
  Envelope shape{.shape = Envelope::Shape::Trapezoid};
  shape.riseStart = riseStart;
  shape.holdStart = holdStart < riseStart ? riseStart : holdStart;
  shape.holdEnd = holdEnd < shape.holdStart ? shape.holdStart : holdEnd;
  shape.fallEnd = fallEnd < shape.holdEnd ? shape.holdEnd : fallEnd;
  return shape;
}

Envelope shaped(Easing curve) {
  return {.shape = Envelope::Shape::Shaped, .curve = std::move(curve)};
}

}  // namespace envelope

float Binding::apply(float value) const {
  // Scale-then-offset rather than subtract-then-divide: the two round
  // differently, and a bound property's pixels are pinned to this rounding.
  const float span = from.high - from.low;
  const float scale = span != 0.0f ? 1.0f / span : 0.0f;
  const float offset = span != 0.0f ? -from.low / span : 0.0f;
  value = value * scale + offset;
  if (clampFrom) value = unitClamp(value);
  // The noise PHASE is read here — off the normalised input, before
  // anything shapes the signal and before `to` puts it in output units:
  // the shake reads the SCHEDULE, so a folded phase does not retrace the
  // same shake on its way back and a curve does not ease it out.
  const float phase = value;
  if (alternate) {
    // A triangle of period 1: there and back across the span, and then
    // again, so a phase that keeps climbing keeps bouncing.
    const float folded = value - std::floor(value);
    value = folded < 0.5f ? folded + folded : 2.0f - (folded + folded);
  }
  switch (envelope.shape) {
    case Envelope::Shape::None:
      break;
    case Envelope::Shape::Cosine:
      // Periodic by construction — the repeat comes from the cosine itself.
      value = 0.5f - 0.5f * std::cos(6.28318530717958648f * value);
      break;
    case Envelope::Shape::Trapezoid:
      // Dark outside [riseStart, fallEnd] and NOT folded into one span: a
      // clamped source reads exactly 1 at the end of its beat, and a fold
      // would read that as the start of the next pass.
      if (value <= envelope.riseStart || value >= envelope.fallEnd)
        value = 0.0f;
      else if (value < envelope.holdStart)
        // Reachable only when the corners differ, which is what makes a
        // zero-length shoulder an instant cut rather than a division by
        // zero.
        value = (value - envelope.riseStart) /
                (envelope.holdStart - envelope.riseStart);
      else if (value <= envelope.holdEnd)
        value = 1.0f;
      else
        value = (envelope.fallEnd - value) /
                (envelope.fallEnd - envelope.holdEnd);
      break;
    case Envelope::Shape::Square: {
      // PHASE 0 IS ON: `folded < duty` answers 1 at exactly 0, so a pulse
      // never blanks its owner at the seam of a wrapping phase.
      const float folded = value - std::floor(value);
      value = folded < envelope.duty ? 1.0f : 0.0f;
      break;
    }
    case Envelope::Shape::Shaped: {
      const float folded = value - std::floor(value);
      value = envelope.curve ? envelope.curve(folded) : folded;
      break;
    }
  }
  if (ease) value = ease(value);
  if (quantize > 1)
    value = std::round(value * (float)(quantize - 1)) / (float)(quantize - 1);
  if (reverse) value = 1.0f - value;
  value = to.low + value * (to.high - to.low);
  // AFTER the output range, BEFORE the wiggle: the noise phase above reads
  // the unwrapped schedule, so a wrapped phase wiggles continuously across
  // the seam instead of repeating its shake every lap.
  if (wrap > 0.0f) {
    value = std::fmod(value, wrap);
    if (value < 0.0f) value += wrap;
  }
  if (wiggle.amount != 0.0f) {
    const int octaves =
        wiggle.octaves < 1 ? 1 : (wiggle.octaves > 8 ? 8 : wiggle.octaves);
    value += wiggle.amount * detail::wiggleNoise(phase * wiggle.frequency,
                                                 wiggle.seed, octaves,
                                                 unitClamp(wiggle.falloff));
  }
  value = value < clamp.low ? clamp.low : (value > clamp.high ? clamp.high : value);
  return value;
}

static_assert(core::kFieldCount<Binding> == 11,
              "Binding gained or lost a field. operator== below compares it "
              "BY HAND: rule on the new field (participate, or a stated "
              "reason not to), then bump this count. A miss is silent — the "
              "node prunes and keeps shaping through the old stages forever.");
bool Binding::operator==(const Binding& other) const {
  return from == other.from && clampFrom == other.clampFrom &&
         alternate == other.alternate && envelope == other.envelope &&
         // A plain function compares by identity; a capturing lambda is
         // unequal to everything, so the binding re-patches every describe.
         easeEqual(ease, other.ease) && quantize == other.quantize &&
         reverse == other.reverse && to == other.to && wrap == other.wrap &&
         wiggle == other.wiggle && clamp == other.clamp;
}

}  // namespace sigil::motion
