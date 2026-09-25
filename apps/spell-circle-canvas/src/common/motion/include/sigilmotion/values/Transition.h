#pragma once

/** @file
 * @ingroup motion-values
 *
 * `Transition`, how a node's plain values ease when they change — the
 * duration, the delay, the curve and the composition — and the
 * comparator an identity prune reads two of them through.
 */

#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Tween.h>

#include <chrono>
#include <tuple>
#include <utility>

namespace sigil::motion {

/** HOW A PLAIN VALUE EASES WHEN IT CHANGES, instead of snapping — what a
 *  node's or a rule's `.transition()` carries for every plain value on it.
 *  A duration alone is the common case, and every consumer's verb takes
 *  one: `.transition(320ms)`.
 *
 *  `delay` holds the value where it stands before the ease starts, which
 *  is the stagger primitive: give a set of siblings delays that step by a
 *  fixed amount and the cascade is data rather than bookkeeping. */
struct Transition {
  Duration duration = std::chrono::milliseconds(250);
  Duration delay{};
  Easing ease = ease::outQuad;
  Composition composition = Composition::Replace;

  /** ALWAYS read the curve through here, never through `ease` directly.
   *
   *  `{.duration = 360ms, .ease = {}}` is an aggregate's way of naming an
   *  EMPTY function, and calling one throws on the first frame. This
   *  accessor substitutes the default curve for an empty function, so an
   *  empty curve means what its author meant. */
  const Easing& easing() const {
    static const Easing kDefault = ease::outQuad;
    return ease ? ease : kDefault;
  }
};

/** A value held inside [0, 1] — the range every house curve is defined
 *  on, and the one a caller computing its own progress out of two times
 *  or two distances keeps stepping outside of. One body, because the
 *  three-way `std::clamp` spelled by hand is where a NaN quietly becomes
 *  the low end. */
inline float clamp01(float value) {
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

/** Same duration, same delay, same curve under `easeEqual`'s rule, same
 *  composition. */
bool transitionEqual(const Transition& left, const Transition& right);

namespace detail {
/** The spec decomposed member by member, for a comparator that wants to
 *  WALK it rather than name each field one at a time. */
inline auto fields(Transition& transition) {
  auto& [duration, delay, ease, composition] = transition;
  return std::tie(duration, delay, ease, composition);
}
}  // namespace detail

}  // namespace sigil::motion
