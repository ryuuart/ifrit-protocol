#pragma once

/** @file
 * @ingroup motion-values
 *
 * How a property change moves: the Transition spec (duration, curve,
 * delay), `ramp()`, the transition spelled in float milliseconds, and the
 * comparator an identity prune reads two specs through.
 */

#include <sigilmotion/ease/Ease.h>

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
  Easing ease = ease::outQuad;
  std::chrono::milliseconds delay{0};

  /** ALWAYS read the curve through here, never through `ease` directly.
   *
   *  `{360ms, {}, 220ms}` is the obvious way to write "the default curve,
   *  but I need to name the delay" — and because Transition is an
   *  aggregate, that `{}` initialises `ease` to an EMPTY std::function.
   *  It compiles, and calling it throws `bad_function_call` on the first
   *  frame. This accessor substitutes the default curve for an empty
   *  function, so `{}` means what the author meant. */
  const Easing& easing() const {
    static const Easing kDefault = ease::outQuad;
    return ease ? ease : kDefault;
  }
};

/** THE CURVES live in `ease/Ease.h`, included above, because a binding,
 *  a keyed step and this spec all shape a unit position with the same
 *  value. `Transition::ease` holds an `Easing`, and every one of them
 *  converts to it:
 *
 *      {520ms, ease::outBack()}
 *      {360ms, ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f)}
 *      {200ms, ease::smoothstep}
 */

/** A value held inside [0, 1] — the range every house curve is defined
 *  on, and the one a caller computing its own progress out of two times
 *  or two distances keeps stepping outside of. One body, because the
 *  three-way `std::clamp` spelled by hand is where a NaN quietly becomes
 *  the low end. */
inline float clamp01(float value) {
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

/** A delayed ramp, in MILLISECONDS as floats.
 *
 *  Float ms rather than `std::chrono::milliseconds` on purpose: a staggered
 *  reveal computes its delay arithmetically
 *  (`ramp(tTicks * 1000 + 300 + i * 25, 400)`), and a chrono parameter
 *  would put a cast at every such site. `Transition{.duration = 400ms}`
 *  remains the spelling wherever the numbers are literals. */
inline Transition ramp(float delayMs, float durationMs,
                       Easing ease = ease::outQuad) {
  Transition transition;
  transition.duration = std::chrono::milliseconds((int)durationMs);
  transition.delay = std::chrono::milliseconds((int)delayMs);
  transition.ease = std::move(ease);
  return transition;
}

/** Same duration, same delay, same curve under `easeEqual`'s rule. */
bool transitionEqual(const Transition& left, const Transition& right);

namespace detail {
/** The spec decomposed member by member, for a comparator that wants to
 *  WALK it rather than name each field one at a time. */
inline auto fields(Transition& transition) {
  auto& [duration, ease, delay] = transition;
  return std::tie(duration, ease, delay);
}
}  // namespace detail

}  // namespace sigil::motion
