#pragma once

/** @file
 * How a property change moves: the Transition spec (duration, curve,
 * delay), the house easing curves as EaseFn values, `ramp()`, the
 * transition spelled in float milliseconds, and the comparator an
 * identity prune reads two specs through.
 */

#include <choreograph/Choreograph.h>

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
/** Overshoot and settle. `s` is the overshoot amount (Penner's 1.70158
 *  overshoots by ~10%); larger exaggerates the anticipation. */
inline choreograph::EaseFn outBack(float s = 1.70158f) {
  return [s](float t) { return choreograph::easeOutBack(t, s); };
}
inline choreograph::EaseFn inBack(float s = 1.70158f) {
  return [s](float t) { return choreograph::easeInBack(t, s); };
}
inline choreograph::EaseFn inOutBack(float s = 1.70158f) {
  return [s](float t) { return choreograph::easeInOutBack(t, s); };
}
/** Ring down to rest. `a` is amplitude, `p` the period. */
inline choreograph::EaseFn outElastic(float a = 1.0f, float p = 0.3f) {
  return [a, p](float t) { return choreograph::easeOutElastic(t, a, p); };
}
inline choreograph::EaseFn inElastic(float a = 1.0f, float p = 0.3f) {
  return [a, p](float t) { return choreograph::easeInElastic(t, a, p); };
}
/** Land and bounce. */
inline choreograph::EaseFn outBounce(float a = 1.70158f) {
  return [a](float t) { return choreograph::easeOutBounce(t, a); };
}
}  // namespace ease

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
