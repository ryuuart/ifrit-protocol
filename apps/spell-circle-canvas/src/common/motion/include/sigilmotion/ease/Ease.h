#pragma once

/** @file
 * @ingroup motion-ease
 *
 * THE CURVES A MOTION IS SHAPED BY: `Easing`, the one type every
 * transition, binding, keyed step and spread holds a curve as; the
 * named catalogue under `ease::`; the parameterised families; and
 * `easeEqual`, the rule two held curves compare by.
 */

#include <sigilcore/compute/Curve.h>

#include <cmath>
#include <functional>

namespace sigil::motion {

/** A CURVE, as every slot in this library holds one: a float→float
 *  function over a unit position. A plain function, a `ease::Curve` and
 *  a lambda all convert to it, so every name below drops in wherever a
 *  curve is asked for:
 *
 *      {520ms, ease::outBack()}
 *      {360ms, ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f)}
 *      {200ms, ease::outQuint}
 *
 *  Only two of those forms can be read back out again, which is what
 *  `easeEqual` compares. */
using Easing = std::function<float(float)>;

/** Equal only when PROVABLY identical: two curves compare equal when
 *  both are the same plain function pointer, or both are the same
 *  `ease::Curve` shape at the same settings — the two forms an `Easing`
 *  can be read back as. Two empty slots are equal. A lambda-valued curve
 *  compares unequal to everything, itself included, because an `Easing`
 *  holding one cannot be inspected.
 *
 *  ONE BODY for every curve slot in the library — a binding's map and
 *  wave, a Transition's, a Spread's distribution — because a second
 *  spelling of this rule would let two comparators disagree about
 *  whether the value that holds a curve may prune. */
bool easeEqual(const Easing& a, const Easing& b);

}  // namespace sigil::motion

/** THE NAMED CURVES. `linear`, then `in`, `out` and `inOut` over ten
 *  families — Quad, Cubic, Quart, Quint, Sine, Expo, Circ (plain
 *  functions, so each is its own address and compares by it), and Back,
 *  Elastic, Bounce (factories over their shape parameter, handing back a
 *  `Curve` that compares by its numbers) — plus the power families, the
 *  steps, the CSS cubic Bézier and the Hermite `smoothstep`.
 *
 *  `in` starts slow and arrives fast, `out` leaves fast and settles, and
 *  `inOut` does both, meeting at the midpoint. A plain function is
 *  written bare (`ease::outQuint`); a factory is called
 *  (`ease::outBack()`, `ease::outElastic(1.0f, 0.4f)`). */
namespace sigil::motion::ease {

/** A CURVE THAT CARRIES ITS OWN PARAMETERS and still compares: a
 *  captureless shape and the numbers it reads, kept side by side, so two
 *  calls with the same arguments compare equal and the value holding one
 *  prunes. A caller's own curve fits the same way — write the body as a
 *  captureless lambda over the parameter block. */
using Curve = core::curve::Curve;

namespace detail {
/** Half a turn, as the float the trigonometric curves are defined at. */
inline constexpr float kPi = 3.14159265358979323846f;
}  // namespace detail

/** The identity: the position unchanged. */
[[nodiscard]] inline float linear(float progress) { return progress; }

[[nodiscard]] inline float inQuad(float progress) {
  return progress * progress;
}
[[nodiscard]] inline float outQuad(float progress) {
  return -progress * (progress - 2);
}
[[nodiscard]] inline float inOutQuad(float progress) {
  progress *= 2;
  if (progress < 1) return 0.5f * progress * progress;
  progress -= 1;
  return -0.5f * (progress * (progress - 2) - 1);
}

[[nodiscard]] inline float inCubic(float progress) {
  return progress * progress * progress;
}
[[nodiscard]] inline float outCubic(float progress) {
  progress -= 1;
  return progress * progress * progress + 1;
}
[[nodiscard]] inline float inOutCubic(float progress) {
  progress *= 2;
  if (progress < 1) return 0.5f * progress * progress * progress;
  progress -= 2;
  return 0.5f * (progress * progress * progress + 2);
}

[[nodiscard]] inline float inQuart(float progress) {
  return progress * progress * progress * progress;
}
[[nodiscard]] inline float outQuart(float progress) {
  progress -= 1;
  return -(progress * progress * progress * progress - 1);
}
[[nodiscard]] inline float inOutQuart(float progress) {
  progress *= 2;
  if (progress < 1) return 0.5f * progress * progress * progress * progress;
  progress -= 2;
  return -0.5f * (progress * progress * progress * progress - 2);
}

[[nodiscard]] inline float inQuint(float progress) {
  return progress * progress * progress * progress * progress;
}
[[nodiscard]] inline float outQuint(float progress) {
  progress -= 1;
  return progress * progress * progress * progress * progress + 1;
}
[[nodiscard]] inline float inOutQuint(float progress) {
  progress *= 2;
  if (progress < 1)
    return 0.5f * progress * progress * progress * progress * progress;
  progress -= 2;
  return 0.5f * (progress * progress * progress * progress * progress + 2);
}

[[nodiscard]] inline float inSine(float progress) {
  return -std::cos(progress * detail::kPi / 2) + 1;
}
[[nodiscard]] inline float outSine(float progress) {
  return std::sin(progress * detail::kPi / 2);
}
[[nodiscard]] inline float inOutSine(float progress) {
  return -0.5f * (std::cos(detail::kPi * progress) - 1);
}

/** The exponential curves are pinned to exactly 0 and 1 at their ends,
 *  where the power of two alone would stop a thousandth short. */
[[nodiscard]] inline float inExpo(float progress) {
  return progress == 0 ? 0.0f : std::pow(2.0f, 10 * (progress - 1));
}
[[nodiscard]] inline float outExpo(float progress) {
  return progress == 1 ? 1.0f : -std::pow(2.0f, -10 * progress) + 1;
}
[[nodiscard]] inline float inOutExpo(float progress) {
  if (progress == 0) return 0.0f;
  if (progress == 1) return 1.0f;
  progress *= 2;
  if (progress < 1) return 0.5f * std::pow(2.0f, 10 * (progress - 1));
  return 0.5f * (-std::pow(2.0f, -10 * (progress - 1)) + 2);
}

/** The circular curves are quarter circles, and are defined on [0, 1]
 *  only: outside it the square root has nothing to read. */
[[nodiscard]] inline float inCirc(float progress) {
  return -(std::sqrt(1 - progress * progress) - 1);
}
[[nodiscard]] inline float outCirc(float progress) {
  progress -= 1;
  return std::sqrt(1 - progress * progress);
}
[[nodiscard]] inline float inOutCirc(float progress) {
  progress *= 2;
  if (progress < 1) return -0.5f * (std::sqrt(1 - progress * progress) - 1);
  progress -= 2;
  return 0.5f * (std::sqrt(1 - progress * progress) + 1);
}

/** THE OVERSHOOTS: the cubic `(s+1)t³ − st²` pulling back before it
 *  goes (`in`), overshooting and settling (`out`), or both at a
 *  half-scale overshoot so the middle passes cleanly (`inOut`). An
 *  overshoot of 1.70158 carries about a tenth past the end. */
using core::curve::inBack;
using core::curve::inOutBack;
using core::curve::outBack;

/** THE RINGS: a sine decaying under a halving exponential, shaking loose
 *  before it goes (`in`), ringing down to rest (`out`), or both. An
 *  amplitude under one is raised to one; a period of zero is read as the
 *  default period, so every curve answers a determinate float. */
using core::curve::inElastic;
using core::curve::outElastic;
[[nodiscard]] inline Curve inOutElastic(float amplitude = 1.0f,
                                        float period = 0.3f) {
  return {[](float progress, const float* parameters) {
            if (progress == 0) return 0.0f;
            progress *= 2;
            if (progress == 2) return 1.0f;
            float ringAmplitude = parameters[0];
            const float ringPeriod =
                core::curve::detail::elasticPeriod(parameters[1]);
            const float phase =
                core::curve::detail::elasticPhase(ringAmplitude, ringPeriod);
            if (progress < 1)
              return -0.5f * (ringAmplitude * std::pow(2.0f, 10 * (progress - 1)) *
                              std::sin((progress - 1 - phase) *
                                       (2 * detail::kPi) / ringPeriod));
            return ringAmplitude * std::pow(2.0f, -10 * (progress - 1)) *
                       std::sin((progress - 1 - phase) * (2 * detail::kPi) /
                                ringPeriod) *
                       0.5f +
                   1;
          },
          {amplitude, period}};
}

/** THE REBOUNDS: four parabolic arcs of decreasing height, landing
 *  (`out`), leaving (`in`, the landing run backwards), or both.
 *  @p overshoot controls how far each rebound after the first carries. */
using core::curve::outBounce;
[[nodiscard]] inline Curve inBounce(float overshoot = 1.70158f) {
  return {[](float progress, const float* parameters) {
            return 1 - core::curve::detail::bounceOut(1 - progress,
                                                      parameters[0]);
          },
          {overshoot}};
}
[[nodiscard]] inline Curve inOutBounce(float overshoot = 1.70158f) {
  return {[](float progress, const float* parameters) {
            if (progress < 0.5f)
              return (1 - core::curve::detail::bounceOut(1 - 2 * progress,
                                                         parameters[0])) /
                     2;
            if (progress == 1) return 1.0f;
            return core::curve::detail::bounceOut(2 * progress - 1,
                                                  parameters[0]) /
                       2 +
                   0.5f;
          },
          {overshoot}};
}

/** THE POWER FAMILIES: `t^power` read in, out, or both, so a strength
 *  between the named ones is a number rather than a new name —
 *  `in(2)` is `inQuad`, `out(3)` is `outCubic`. The default 1.68 sits
 *  between linear and quadratic. */
[[nodiscard]] inline Curve in(float power = 1.68f) {
  return {[](float progress, const float* parameters) {
            return std::pow(progress, parameters[0]);
          },
          {power}};
}
[[nodiscard]] inline Curve out(float power = 1.68f) {
  return {[](float progress, const float* parameters) {
            return 1 - std::pow(1 - progress, parameters[0]);
          },
          {power}};
}
[[nodiscard]] inline Curve inOut(float power = 1.68f) {
  return {[](float progress, const float* parameters) {
            if (progress < 0.5f)
              return std::pow(2 * progress, parameters[0]) / 2;
            return 1 - std::pow(2 - 2 * progress, parameters[0]) / 2;
          },
          {power}};
}

/** A STAIRCASE of @p count equal jumps. The first jump lands at the end
 *  of the first step, so the curve holds 0 until then; with
 *  @p jumpAtStart it lands at the start instead and the curve reaches 1
 *  one step early. The position is held inside [0, 1] first, so the
 *  curve never steps past either end. A count under one is one step. */
[[nodiscard]] inline Curve steps(int count, bool jumpAtStart = false) {
  return {[](float progress, const float* parameters) {
            const float stepCount = parameters[0];
            const float held =
                progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
            const float step = parameters[1] != 0.0f
                                   ? std::ceil(held * stepCount)
                                   : std::floor(held * stepCount);
            return step / stepCount;
          },
          {count < 1 ? 1.0f : (float)count, jumpAtStart ? 1.0f : 0.0f}};
}

/** THE CSS CURVE, spelled as it was written: the cubic Bézier through
 *  (0,0), (x1,y1), (x2,y2), (1,1), read as y at the x asked for.
 *  `cubicBezier(0.25, 0.1, 0.25, 1)` is CSS's `ease`. */
using core::curve::cubicBezier;

/** The Hermite S-curve `t²(3 − 2t)`, with no parameters: eased at both
 *  ends and symmetric. */
using core::curve::smoothstep;

}  // namespace sigil::motion::ease
