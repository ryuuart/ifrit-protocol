#pragma once

/** @file
 * @ingroup motion-bind
 *
 * `Binding`, the stages a followed number is shaped through on its way to
 * a property — the source's range, a there-and-back, an envelope, a
 * curve, levels, a reversal, the output range, a wrap, a wiggle and a
 * clamp — as FIELDS, applied in the fixed order they are declared in.
 * `Binding::apply` is a pure float→float map that reads no clock.
 */

#include <sigilmotion/ease/Ease.h>

#include <cstdint>
#include <limits>

namespace sigil::motion {

/** A RANGE of numbers, low end first. A range whose ends are equal is a
 *  point; a range whose low end is above its high end runs backwards. */
struct Range {
  float low = 0.0f;
  float high = 1.0f;

  bool operator==(const Range&) const = default;
};

/** THE SHAPE a normalised phase takes across its span — one per binding,
 *  because a phase has one shape. Built by the `envelope::` factories;
 *  the default shapes nothing. */
struct Envelope {
  enum class Shape : uint8_t {
    /** The phase passes through unchanged. */
    None,
    /** The swell: 0 at both ends of the span, 1 at its middle, eased into
     *  and out of both — periodic, so a phase that keeps climbing keeps
     *  breathing. */
    Cosine,
    /** Ramp up, hold at 1, ramp down, dark — positions inside ONE pass,
     *  so a repeating gate rides a phase that already wraps. */
    Trapezoid,
    /** The pulse: 1 across the first `duty` of each period, 0 across the
     *  rest. Phase 0 is ON, so a caret born at the start of its cycle is
     *  born visible. */
    Square,
    /** The caller's own shape, read on the folded phase in [0, 1), so
     *  whatever it draws across one period the signal repeats. */
    Shaped,
  };
  Shape shape = Shape::None;
  /** Trapezoid: the four corners in the normalised phase, held
   *  non-decreasing so a zero-length shoulder is an instant cut. */
  float riseStart = 0.0f, holdStart = 0.0f, holdEnd = 1.0f, fallEnd = 1.0f;
  /** Square: the ON fraction of each period, held inside [0, 1]. */
  float duty = 0.5f;
  /** Shaped: the caller's curve. Part of the envelope's identity, compared
   *  under `easeEqual`'s rule. */
  Easing curve;

  /** Same shape, same numbers, and the same curve under `easeEqual`. */
  bool operator==(const Envelope& other) const;
};

/** The envelope factories: `envelope::cosine()`, `envelope::square(0.6f)`,
 *  `envelope::trapezoid(0, 0.03f, 0.84f, 1)`, `envelope::shaped(curve)`. */
namespace envelope {
/** The swell. */
Envelope cosine();
/** The pulse; @p duty is held inside [0, 1]. */
Envelope square(float duty = 0.5f);
/** Ramp from @p riseStart to @p holdStart, hold at 1 to @p holdEnd, ramp
 *  down to @p fallEnd; each corner is held to at least the one before. */
Envelope trapezoid(float riseStart, float holdStart, float holdEnd,
                   float fallEnd);
/** The caller's own periodic shape. */
Envelope shaped(Easing curve);
}  // namespace envelope

/** SMOOTH PROCEDURAL NOISE added in the property's own units — camera
 *  shake, handheld drift, organic jitter. An `amount` of zero is no
 *  wiggle, and that is the default.
 *
 *  It reads NO CLOCK: the noise is a function of the bound value,
 *  sampled at the normalised phase (after `from`, before everything that
 *  shapes the signal), so "wiggle over time" is spelled by binding a
 *  phase that ramps with time and a headless frame stays a pure function
 *  of what its values hold. Different seeds per axis is the whole of a
 *  shake rig: a shared seed moves a layer along a diagonal. */
struct Wiggle {
  /** Peak displacement, in the property's own units. */
  float amount = 0.0f;
  /** Cycles per unit of normalised phase — hertz, for a phase in
   *  seconds with the default `from`. */
  float frequency = 2.0f;
  /** Which noise: the same seed is the same wiggle, always. */
  uint32_t seed = 0;
  /** 1 is a smooth drift; 2–3 add the fine tremble. Held inside 1..8. */
  int octaves = 1;
  /** How much quieter each octave is than the last, inside [0, 1]. */
  float falloff = 0.5f;

  bool operator==(const Wiggle&) const = default;
};

/** THE STAGES a followed number is shaped through, as fields.
 *
 *      bind(phase, {.to = {-70, 170}})
 *      bind(seconds, {.from = {0, 7.2f}, .envelope = envelope::cosine()})
 *      bind(hitPoints, {.from = {0, maximum}, .ease = ease::outBack()})
 *
 *  The stages run in the order the fields are declared, whatever order a
 *  caller names them in — designated initialisers enforce that order at
 *  the call site, so a binding reads the way it runs:
 *
 *    1. `from` normalises the source's range onto [0, 1]; `clampFrom`
 *       holds the result inside [0, 1] (a beat inside a longer timeline,
 *       and nothing else);
 *    2. `alternate` folds the phase there and back — a triangle of period
 *       1, so a phase that keeps climbing keeps bouncing;
 *    3. `envelope` shapes the phase across the span;
 *    4. `ease` curves it;
 *    5. `quantize` snaps it to that many evenly spaced levels (0 and 1
 *       are continuous);
 *    6. `reverse` turns it round (1 − v);
 *    7. `to` puts [0, 1] onto the output range;
 *    8. `wrap` folds the output into [0, wrap) — the looping phase; 0
 *       folds nothing;
 *    9. `wiggle` adds noise in output units;
 *   10. `clamp` bounds the output; the default range is unbounded.
 *
 *  A binding costs no storage on a property that never shapes anything:
 *  the stages ride the out-of-line block an `Animatable` allocates for
 *  its fat forms. */
struct Binding {
  Range from{0.0f, 1.0f};
  bool clampFrom = false;
  bool alternate = false;
  Envelope envelope{};
  Easing ease;
  int quantize = 0;
  bool reverse = false;
  Range to{0.0f, 1.0f};
  float wrap = 0.0f;
  Wiggle wiggle{};
  Range clamp{-std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity()};

  /** Runs the stages on one sample of the followed value. */
  [[nodiscard]] float apply(float value) const;

  /** Every field, the two curves under `easeEqual`'s rule: a re-describe
   *  that changes only a curve must not prune, because the stages are
   *  read live and a pruned node would keep shaping through the old one. */
  bool operator==(const Binding& other) const;
};

}  // namespace sigil::motion
