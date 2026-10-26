#pragma once

/** @file
 * @ingroup core-compute
 *
 * A SHAPED CURVE AS A COMPARABLE VALUE: the shape and the numbers that
 * shape it, side by side, so a curve carrying parameters can still be
 * proved the same curve.
 *
 * The shape is a captureless function — a pointer, therefore comparable
 * — and the numbers are four floats beside it, so two curves are equal
 * when they are the same shape at the same settings and a memo keyed on
 * one can be skipped.
 *
 * THE ARITHMETIC IS THE CONTRACT. The house shapes below are the Penner
 * equations, spelled here rather than reached for in an animation
 * dependency, because a colour ramp and a point cook read the same curve
 * as an animation does and neither of them links one.
 */

#include <cmath>

/** THE SHAPED CURVE A UNIT POSITION IS RESHAPED BY, as a value that can
 *  still be compared: the shape and the numbers that parameterise it
 *  kept side by side, plus the house shapes over it. It sits this low
 *  because an animation's easing, a colour ramp's walk and a keyed
 *  track's segment are all the same value, and every one of them has to
 *  answer whether two curves are the same curve. */
namespace sigil::core::curve {

/** A CURVE THAT CARRIES ITS OWN PARAMETERS, and can still be compared. */
struct Curve {
  /** THE SHAPE, as a captureless function over the parameter block: a
   *  pointer, so two of them can be compared. Null answers `progress`
   *  unchanged, the identity ramp. It is also the escape hatch — a
   *  caller's own captureless lambda is comparable for free.
   *  @trap A body that must CAPTURE is a lambda again, and unequal to
   *  everything, so nothing holding it ever prunes. */
  float (*shape)(float progress, const float* parameters) = nullptr;
  /** What the shape reads. Four because the widest house curve — a cubic
   *  Bezier's two control points — takes four; the rest leave the tail at
   *  zero, and a zero is as comparable as any other number. */
  float parameters[4]{};

  /** THE VALUE AT @p progress. The input is NOT clamped: several of the shapes
   *  are defined outside [0,1] and a caller that must stay inside it says
   *  so at its own site. */
  [[nodiscard]] float at(float progress) const {
    return shape ? shape(progress, parameters) : progress;
  }

  /** The same reading, so a curve IS a float→float function and drops
   *  into anything that takes one. */
  float operator()(float progress) const { return at(progress); }

  bool operator==(const Curve&) const = default;
};

/** THE HERMITE S-CURVE, `t²(3 − 2t)`: eased at both ends, symmetric,
 *  and the plain smooth ramp a wipe, a fade edge or a gloss ring wants.
 *  A plain function rather than a `Curve` because it has no parameters,
 *  so `&smoothstep` goes anywhere a float→float function is wanted and
 *  compares by its own address.
 *  @trap The input is NOT clamped: the polynomial turns back on itself
 *  outside [0, 1]. */
[[nodiscard]] inline float smoothstep(float progress) {
  return progress * progress * (3.0f - 2.0f * progress);
}

namespace detail {
/** Half a turn, as the float the trigonometric shapes below are defined
 *  at. */
inline constexpr float kPi = 3.14159265358979323846f;

/** The parabolic rebounds, with @p overshoot controlling how far each
 *  one carries. Four arcs of decreasing width and height over [0,1]. */
[[nodiscard]] inline float bounceOut(float progress, float overshoot) {
  if (progress == 1) return 1.0f;
  if (progress < (4 / 11.0f)) return 7.5625f * progress * progress;
  if (progress < (8 / 11.0f)) {
    progress -= (6 / 11.0f);
    return -overshoot * (1 - (7.5625f * progress * progress + 0.75f)) + 1.0f;
  }
  if (progress < (10 / 11.0f)) {
    progress -= (9 / 11.0f);
    return -overshoot * (1 - (7.5625f * progress * progress + 0.9375f)) + 1.0f;
  }
  progress -= (21 / 22.0f);
  return -overshoot * (1 - (7.5625f * progress * progress + 0.984375f)) + 1.0f;
}

/** Where the decaying sine starts, given @p amplitude and @p period.
 *  An amplitude under the travelled distance cannot reach the endpoint,
 *  so it is raised to it and the phase is a quarter period. */
/** The default ring period, and the one a period of zero reads as: a
 *  ring of no period is a division by zero, and the curves here answer
 *  a determinate float for every argument. */
inline constexpr float kElasticPeriod = 0.3f;

/** @p period, or the default period when it has no ring in it. */
[[nodiscard]] inline constexpr float elasticPeriod(float period) {
  return period > 0.0f ? period : kElasticPeriod;
}

[[nodiscard]] inline float elasticPhase(float& amplitude, float period) {
  if (amplitude < 1.0f) {
    amplitude = 1.0f;
    return period / 4.0f;
  }
  return period / (2 * kPi) * std::asin(1.0f / amplitude);
}

/** One coordinate of the cubic through (0,0), (@p firstControl,·), (@p
 * secondControl,·), (1,1). */
[[nodiscard]] inline float bezierAxis(float parameter, float firstControl,
                                      float secondControl) {
  const float complement = 1.0f - parameter;
  return 3.0f * complement * complement * parameter * firstControl +
         3.0f * complement * parameter * parameter * secondControl +
         parameter * parameter * parameter;
}
}  // namespace detail

/** THE CSS CURVE, by its own definition: the cubic Bezier through
 *  (0,0), (x1,y1), (x2,y2), (1,1), evaluated as y at the x asked for,
 *  so a design handed over as a CSS timing function is spelled as it
 *  was written. `cubicBezier(0.25, 0.1, 0.25, 1)` is `ease`, the CSS
 *  default. The four control numbers ARE the identity: two curves
 *  compare equal when they were asked for at the same numbers. */
[[nodiscard]] inline Curve cubicBezier(float x1, float y1, float x2, float y2) {
  return {[](float progress, const float* parameters) {
            if (progress <= 0.0f) return 0.0f;
            if (progress >= 1.0f) return 1.0f;
            // Twenty-four halvings put the parameter within 2^-24 of the
            // one whose x is the position asked for, which is exact to
            // far below a pixel at any size a screen has.
            float low = 0.0f, high = 1.0f, parameter = progress;
            for (int i = 0; i < 24; ++i) {
              parameter = 0.5f * (low + high);
              (detail::bezierAxis(parameter, parameters[0], parameters[2]) <
                       progress
                   ? low
                   : high) = parameter;
            }
            return detail::bezierAxis(parameter, parameters[1], parameters[3]);
          },
          {x1, y1, x2, y2}};
}

/** Overshoot and settle: the cubic `(s+1)t³ − st²`, read out. @p overshoot is
 * the overshoot amount — 1.70158 overshoots by about a tenth, and larger
 *  exaggerates the anticipation. */
[[nodiscard]] inline Curve outBack(float overshoot = 1.70158f) {
  return {[](float progress, const float* parameters) {
            progress -= 1;
            return progress * progress *
                       ((parameters[0] + 1) * progress + parameters[0]) +
                   1;
          },
          {overshoot}};
}

/** The same cubic read in: it pulls back before it goes. */
[[nodiscard]] inline Curve inBack(float overshoot = 1.70158f) {
  return {[](float progress, const float* parameters) {
            return progress * progress *
                   ((parameters[0] + 1) * progress - parameters[0]);
          },
          {overshoot}};
}

/** Pull back, go, overshoot, settle — the two halves joined, each at a
 *  half-scale overshoot so the middle passes through cleanly. */
[[nodiscard]] inline Curve inOutBack(float overshoot = 1.70158f) {
  return {[](float progress, const float* parameters) {
            progress *= 2;
            const float scaledOvershoot = parameters[0] * 1.525f;
            if (progress < 1)
              return 0.5f *
                     (progress * progress *
                      ((scaledOvershoot + 1) * progress - scaledOvershoot));
            progress -= 2;
            return 0.5f *
                   (progress * progress *
                        ((scaledOvershoot + 1) * progress + scaledOvershoot) +
                    2);
          },
          {overshoot}};
}

/** Ring down to rest: a sine decaying under a halving exponential. @p amplitude
 *  is how far it swings, @p period how long one swing takes. A period of zero
 * has no ring in it at all and is read as the default period, so the curve
 * stays the determinate float every shape here promises. */
[[nodiscard]] inline Curve outElastic(float amplitude = 1.0f,
                                      float period = 0.3f) {
  return {[](float progress, const float* parameters) {
            if (progress == 0) return 0.0f;
            if (progress == 1) return 1.0f;
            float ringAmplitude = parameters[0];
            const float ringPeriod = detail::elasticPeriod(parameters[1]);
            const float phase = detail::elasticPhase(ringAmplitude, ringPeriod);
            return ringAmplitude * std::pow(2.0f, -10 * progress) *
                       std::sin((progress - phase) * (2 * detail::kPi) /
                                ringPeriod) +
                   1.0f;
          },
          {amplitude, period}};
}

/** The same ring read in: it shakes loose before it goes. A period of
 *  zero is read as the default period, as it is on the way out. */
[[nodiscard]] inline Curve inElastic(float amplitude = 1.0f,
                                     float period = 0.3f) {
  return {
      [](float progress, const float* parameters) {
        if (progress == 0) return 0.0f;
        if (progress == 1) return 1.0f;
        float ringAmplitude = parameters[0];
        const float ringPeriod = detail::elasticPeriod(parameters[1]);
        const float phase = detail::elasticPhase(ringAmplitude, ringPeriod);
        progress -= 1;
        return -(ringAmplitude * std::pow(2.0f, 10 * progress) *
                 std::sin((progress - phase) * (2 * detail::kPi) / ringPeriod));
      },
      {amplitude, period}};
}

/** Land and bounce: parabolic rebounds of decreasing height. @p overshoot
 *  controls how far each rebound carries. */
[[nodiscard]] inline Curve outBounce(float overshoot = 1.70158f) {
  return {[](float progress, const float* parameters) {
            return detail::bounceOut(progress, parameters[0]);
          },
          {overshoot}};
}

}  // namespace sigil::core::curve
