#pragma once

/** @file
 * A SHAPED CURVE AS A COMPARABLE VALUE: the shape and the numbers that
 * shape it, side by side, so a curve carrying parameters can still be
 * proved the same curve.
 *
 * Everything that reshapes a unit position reaches for one of these — an
 * animation's easing, a colour ramp's walk, a keyed track's segment — and
 * every one of them also has to answer whether two of its values are the
 * same, because the thing holding the curve is memoised on that answer. A
 * curve written as a lambda cannot answer: two lambdas built a frame
 * apart from identical text are different objects, so a record holding
 * one is unequal to itself and never prunes. A curve written as a bare
 * function pointer can answer, but then it has nowhere to keep the
 * overshoot, the period or the four control numbers that give it its
 * character.
 *
 * This is the shape and the numbers kept apart. The shape is a
 * captureless function — a pointer, therefore comparable — and the
 * numbers are four floats beside it, so two curves are equal when they
 * are the same shape at the same settings, and a memo keyed on one can be
 * skipped.
 *
 * THE ARITHMETIC IS THE CONTRACT. The house shapes below are the Penner
 * equations, spelled here rather than reached for in an animation
 * dependency, because a colour ramp and a point cook read the same curve
 * as an animation does and neither of them links one. Two callers asking
 * for the same shape at the same numbers get the same float.
 */

#include <cmath>

namespace sigil::core::curve {

/** A CURVE THAT CARRIES ITS OWN PARAMETERS, and can still be compared. */
struct Curve {
  /** The shape, as a captureless function over the parameter block: a
   *  pointer, so two of them can be compared. Null answers `t` unchanged,
   *  which is the identity ramp.
   *
   *  THIS IS THE ESCAPE HATCH. A caller's own curve fits by being written
   *  here: a captureless lambda over the parameter block is comparable for
   *  free, and reads its own numbers out of `parameters` exactly as the
   *  house shapes do. A body that must capture is a lambda again, and
   *  unequal to everything — which is correct, since nothing can prove two
   *  of them alike. */
  float (*shape)(float t, const float* parameters) = nullptr;
  /** What the shape reads. Four because the widest house curve — a cubic
   *  Bezier's two control points — takes four; the rest leave the tail at
   *  zero, and a zero is as comparable as any other number. */
  float parameters[4]{};

  /** THE VALUE AT @p t. The input is NOT clamped: several of the shapes
   *  are defined outside [0,1] and a caller that must stay inside it says
   *  so at its own site. */
  [[nodiscard]] float at(float t) const {
    return shape ? shape(t, parameters) : t;
  }

  /** The same reading, so a curve IS a float→float function and drops
   *  into anything that takes one. */
  float operator()(float t) const { return at(t); }

  bool operator==(const Curve&) const = default;
};

/** THE HERMITE S-CURVE, `t²(3 − 2t)`: eased at both ends, symmetric, and
 *  the one shape the house set otherwise lacks — back, elastic and bounce
 *  all overshoot, and none of them is the plain smooth ramp a wipe, a
 *  fade edge or a gloss ring wants.
 *
 *  A plain function rather than a `Curve`, because it has no parameters:
 *  call it directly on a normalised value, or hand `&smoothstep` anywhere
 *  a float→float function is wanted, where it compares by its own address.
 *  The input is NOT clamped — the polynomial turns back on itself outside
 *  [0,1]. */
[[nodiscard]] inline float smoothstep(float t) {
  return t * t * (3.0f - 2.0f * t);
}

namespace detail {
/** Half a turn, as the float the trigonometric shapes below are defined
 *  at. */
inline constexpr float kPi = 3.14159265358979323846f;

/** The parabolic rebounds, with @p overshoot controlling how far each
 *  one carries. Four arcs of decreasing width and height over [0,1]. */
[[nodiscard]] inline float bounceOut(float t, float overshoot) {
  if (t == 1) return 1.0f;
  if (t < (4 / 11.0f)) return 7.5625f * t * t;
  if (t < (8 / 11.0f)) {
    t -= (6 / 11.0f);
    return -overshoot * (1 - (7.5625f * t * t + 0.75f)) + 1.0f;
  }
  if (t < (10 / 11.0f)) {
    t -= (9 / 11.0f);
    return -overshoot * (1 - (7.5625f * t * t + 0.9375f)) + 1.0f;
  }
  t -= (21 / 22.0f);
  return -overshoot * (1 - (7.5625f * t * t + 0.984375f)) + 1.0f;
}

/** Where the decaying sine starts, given amplitude @p a and period @p p.
 *  An amplitude under the travelled distance cannot reach the endpoint,
 *  so it is raised to it and the phase is a quarter period. */
/** The default ring period, and the one a period of zero reads as: a
 *  ring of no period is a division by zero, and the curves here answer
 *  a determinate float for every argument. */
inline constexpr float kElasticPeriod = 0.3f;

/** @p p, or the default period when it has no ring in it. */
[[nodiscard]] inline constexpr float elasticPeriod(float p) {
  return p > 0.0f ? p : kElasticPeriod;
}

[[nodiscard]] inline float elasticPhase(float& a, float p) {
  if (a < 1.0f) {
    a = 1.0f;
    return p / 4.0f;
  }
  return p / (2 * kPi) * std::asin(1.0f / a);
}

/** One coordinate of the cubic through (0,0), (@p a,·), (@p b,·), (1,1). */
[[nodiscard]] inline float bezierAxis(float u, float a, float b) {
  const float v = 1.0f - u;
  return 3.0f * v * v * u * a + 3.0f * v * u * u * b + u * u * u;
}
}  // namespace detail

/** THE CSS CURVE, by its own definition: the cubic Bezier through (0,0),
 *  (x1,y1), (x2,y2), (1,1), evaluated as y at the x the caller asks for.
 *
 *  `cubicBezier(0.25, 0.1, 0.25, 1)` is `ease`, the CSS default, and the
 *  whole point of having this is that a design handed over as a CSS
 *  timing function can be spelled as it was written instead of matched by
 *  eye against the nearest house curve.
 *
 *  x is solved by bisection rather than Newton: the curve is monotonic in
 *  x for control points in [0,1], so a fixed number of halvings is exact
 *  to well under a pixel and cannot fail to converge on a degenerate
 *  curve the way a derivative-based solve can.
 *
 *  The four control numbers ARE the identity: two curves compare equal
 *  when they were asked for at the same numbers. */
[[nodiscard]] inline Curve cubicBezier(float x1, float y1, float x2, float y2) {
  return {[](float t, const float* p) {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            // Twenty-four halvings put the parameter within 2^-24 of the
            // one whose x is the position asked for, which is exact to
            // far below a pixel at any size a screen has.
            float lo = 0.0f, hi = 1.0f, u = t;
            for (int i = 0; i < 24; ++i) {
              u = 0.5f * (lo + hi);
              (detail::bezierAxis(u, p[0], p[2]) < t ? lo : hi) = u;
            }
            return detail::bezierAxis(u, p[1], p[3]);
          },
          {x1, y1, x2, y2}};
}

/** Overshoot and settle: the cubic `(s+1)t³ − st²`, read out. @p s is the
 *  overshoot amount — 1.70158 overshoots by about a tenth, and larger
 *  exaggerates the anticipation. */
[[nodiscard]] inline Curve outBack(float s = 1.70158f) {
  return {[](float t, const float* p) {
            t -= 1;
            return t * t * ((p[0] + 1) * t + p[0]) + 1;
          },
          {s}};
}

/** The same cubic read in: it pulls back before it goes. */
[[nodiscard]] inline Curve inBack(float s = 1.70158f) {
  return {
      [](float t, const float* p) { return t * t * ((p[0] + 1) * t - p[0]); },
      {s}};
}

/** Pull back, go, overshoot, settle — the two halves joined, each at a
 *  half-scale overshoot so the middle passes through cleanly. */
[[nodiscard]] inline Curve inOutBack(float s = 1.70158f) {
  return {[](float t, const float* p) {
            t *= 2;
            const float s2 = p[0] * 1.525f;
            if (t < 1) return 0.5f * (t * t * ((s2 + 1) * t - s2));
            t -= 2;
            return 0.5f * (t * t * ((s2 + 1) * t + s2) + 2);
          },
          {s}};
}

/** Ring down to rest: a sine decaying under a halving exponential. @p a
 *  is the amplitude, @p p the period. A period of zero has no ring in
 *  it at all and is read as the default period, so the curve stays the
 *  determinate float every shape here promises. */
[[nodiscard]] inline Curve outElastic(float a = 1.0f, float p = 0.3f) {
  return {[](float t, const float* q) {
            if (t == 0) return 0.0f;
            if (t == 1) return 1.0f;
            float amplitude = q[0];
            const float period = detail::elasticPeriod(q[1]);
            const float s = detail::elasticPhase(amplitude, period);
            return amplitude * std::pow(2.0f, -10 * t) *
                       std::sin((t - s) * (2 * detail::kPi) / period) +
                   1.0f;
          },
          {a, p}};
}

/** The same ring read in: it shakes loose before it goes. A period of
 *  zero is read as the default period, as it is on the way out. */
[[nodiscard]] inline Curve inElastic(float a = 1.0f, float p = 0.3f) {
  return {[](float t, const float* q) {
            if (t == 0) return 0.0f;
            if (t == 1) return 1.0f;
            float amplitude = q[0];
            const float period = detail::elasticPeriod(q[1]);
            const float s = detail::elasticPhase(amplitude, period);
            t -= 1;
            return -(amplitude * std::pow(2.0f, 10 * t) *
                     std::sin((t - s) * (2 * detail::kPi) / period));
          },
          {a, p}};
}

/** Land and bounce: parabolic rebounds of decreasing height. @p a
 *  controls how far each rebound carries. */
[[nodiscard]] inline Curve outBounce(float a = 1.70158f) {
  return {[](float t, const float* p) { return detail::bounceOut(t, p[0]); },
          {a}};
}

}  // namespace sigil::core::curve
