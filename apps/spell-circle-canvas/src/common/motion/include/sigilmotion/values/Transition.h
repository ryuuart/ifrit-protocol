#pragma once

/** @file
 * How a property change moves: the Transition spec (duration, curve,
 * delay), the house easing curves as EaseFn values, `ramp()`, the
 * transition spelled in float milliseconds, and the comparator an
 * identity prune reads two specs through.
 */

#include <choreograph/Choreograph.h>
#include <sigilmotion/bind/Curve.h>

#include <chrono>
#include <tuple>
#include <utility>

#include "sigilmotion/bind/BoundFloat.h"

namespace sigil::motion {

/** How a property change animates instead of snapping.
 *  `delay` holds the CURRENT value (the `from`, for animate() entrances)
 *  before the ramp starts, which is the stagger primitive: give a set of
 *  siblings delays that step by a fixed amount and the cascade is data
 *  rather than bookkeeping. */
struct Transition {
  std::chrono::milliseconds duration{250};
  choreograph::EaseFn ease = &choreograph::easeOutQuad;
  std::chrono::milliseconds delay{0};

  /** ALWAYS read the curve through here, never through `ease` directly.
   *
   *  `{360ms, {}, 220ms}` is the obvious way to write "the default curve,
   *  but I need to name the delay" — and because Transition is an
   *  aggregate, that `{}` initialises `ease` to an EMPTY std::function.
   *  It compiles, and calling it throws `bad_function_call` on the first
   *  frame. This accessor substitutes the default curve for an empty
   *  function, so `{}` means what the author meant. */
  const choreograph::EaseFn& easing() const {
    static const choreograph::EaseFn kDefault = &choreograph::easeOutQuad;
    return ease ? ease : kDefault;
  }
};

/** The house curves, as EaseFn VALUES.
 *
 *  `Transition::ease` holds a `choreograph::EaseFn`, a plain float→float
 *  function. Choreograph's most expressive curves — back, elastic,
 *  bounce — take an extra shape parameter, so `&choreograph::easeOutBack`
 *  does not convert to an EaseFn at all and the compiler answers with a
 *  wall of overload-resolution noise. These wrappers bind the shape
 *  parameter and hand back something a Transition can hold:
 *
 *      .scale(animate(from(0.86f).to(1.0f), {520ms, ease::outBack()}))
 */
namespace ease {
/** THE HERMITE S-CURVE, `t²(3 − 2t)`: eased at both ends, symmetric, and
 *  the one curve the house set otherwise lacked — Back, Elastic and
 *  Bounce all overshoot, and none of them is the plain smooth ramp a
 *  wipe, a fade edge or a gloss ring wants.
 *
 *  A plain function rather than a factory, because it has no shape
 *  parameter: call it directly on a normalised value, or hand
 *  `&ease::smoothstep` anywhere an `EaseFn` is wanted. The input is NOT
 *  clamped — pass it through `clamp01` first where the caller's value can
 *  leave [0,1], since the polynomial turns back on itself outside. */
inline float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

/** THE CSS CURVE, by its own definition: the cubic Bezier through (0,0),
 *  (x1,y1), (x2,y2), (1,1), evaluated as y at the x the caller asks for.
 *
 *  `cubic-bezier(0.25, 0.1, 0.25, 1)` is `ease`, the CSS default, and the
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
 *  when they were asked for at the same numbers, so a transition or a
 *  binding built on a CSS curve prunes like one built on a house curve. */
Curve cubicBezier(float x1, float y1, float x2, float y2);

/** Overshoot and settle. `s` is the overshoot amount (Penner's 1.70158
 *  overshoots by ~10%); larger exaggerates the anticipation.
 *
 *  Every shaped curve below hands back an `ease::Curve` — the shape and
 *  its numbers side by side — so two calls with the same argument compare
 *  EQUAL and the value holding one prunes. A curve written as a
 *  capturing lambda cannot, which is why these exist as factories rather
 *  than as an example to copy. */
inline Curve outBack(float s = 1.70158f) {
  return {[](float t, const float* p) { return choreograph::easeOutBack(t, p[0]); },
          {s}};
}
inline Curve inBack(float s = 1.70158f) {
  return {[](float t, const float* p) { return choreograph::easeInBack(t, p[0]); },
          {s}};
}
inline Curve inOutBack(float s = 1.70158f) {
  return {
      [](float t, const float* p) { return choreograph::easeInOutBack(t, p[0]); },
      {s}};
}
/** Ring down to rest. `a` is amplitude, `p` the period. */
inline Curve outElastic(float a = 1.0f, float p = 0.3f) {
  return {[](float t, const float* q) {
            return choreograph::easeOutElastic(t, q[0], q[1]);
          },
          {a, p}};
}
inline Curve inElastic(float a = 1.0f, float p = 0.3f) {
  return {[](float t, const float* q) {
            return choreograph::easeInElastic(t, q[0], q[1]);
          },
          {a, p}};
}
/** Land and bounce. */
inline Curve outBounce(float a = 1.70158f) {
  return {
      [](float t, const float* p) { return choreograph::easeOutBounce(t, p[0]); },
      {a}};
}
}  // namespace ease

/** A value held inside [0, 1] — the range every curve above is defined
 *  on, and the one a caller computing its own progress out of two times
 *  or two distances keeps stepping outside of. One body, because the
 *  three-way `std::clamp` spelled by hand is where a NaN quietly becomes
 *  the low end. */
inline float clamp01(float v) {
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

/** A delayed ramp, in MILLISECONDS as floats.
 *
 *  Float ms rather than `std::chrono::milliseconds` on purpose: a staggered
 *  reveal computes its delay arithmetically
 *  (`ramp(tTicks * 1000 + 300 + i * 25, 400)`), and a chrono parameter
 *  would put a cast at every such site. `Transition{.duration = 400ms}`
 *  remains the spelling wherever the numbers are literals. */
inline Transition ramp(float delayMs, float durationMs,
                       choreograph::EaseFn ease = &choreograph::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durationMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(ease);
  return t;
}

/** Same duration, same delay, same curve under `easeEqual`'s rule. */
bool transitionEqual(const Transition& a, const Transition& b);

namespace detail {
/** The spec decomposed member by member, for a comparator that wants to
 *  WALK it rather than name each field one at a time. */
inline auto fields(Transition& v) {
  auto& [duration, ease, delay] = v;
  return std::tie(duration, ease, delay);
}
}  // namespace detail

}  // namespace sigil::motion
