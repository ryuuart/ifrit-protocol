#pragma once

/** @file
 * The comparators an identity prune is built from, for the animation
 * values every description carries: two easing curves, two transitions,
 * two shaped bindings and two animatable slots — each equal only when
 * provably identical, so a description that cannot be compared re-patches
 * rather than pruning into a stale predecessor.
 */

#include <sigilmotion/bind/BoundFloat.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Transition.h>

#include <cstddef>
#include <tuple>
#include <utility>

namespace sigil::core {

namespace detail {

// FIELD PINS. A comparator written by hand can leave a field out, and the
// failure is invisible by construction: two different values compare
// equal, the node prunes, and it keeps whatever the old value produced for
// as long as it lives. A structured binding names every direct non-static
// data member of a struct, and the count is a hard error the moment the
// struct changes — so each hand-written comparator sits beside a
// `static_assert(kFieldCount<T> == N)`, and adding a field fails the build
// until someone rules on it in the comparator and bumps the count.

inline auto fields(motion::BoundFloat& v) {
  auto& [source, inScale, inOffset, curve, clampInput, envelope, riseStart,
         holdStart, holdEnd, fallEnd, duty, waveFn, steps, scale, offset,
         clamped, lo, hi, wiggleAmount, wiggleFrequency, wiggleSeed,
         wiggleOctaves, wiggleFalloff, wrapPeriod] = v;
  return std::tie(source, inScale, inOffset, curve, clampInput, envelope,
                  riseStart, holdStart, holdEnd, fallEnd, duty, waveFn, steps,
                  scale, offset, clamped, lo, hi, wiggleAmount, wiggleFrequency,
                  wiggleSeed, wiggleOctaves, wiggleFalloff, wrapPeriod);
}
inline auto fields(motion::Transition& v) {
  auto& [duration, ease, delay] = v;
  return std::tie(duration, ease, delay);
}
template <typename T>
auto fields(motion::Transitioned<T>& v) {
  auto& [value, spec, from, waypoints] = v;
  return std::tie(value, spec, from, waypoints);
}

/** How many direct non-static data members @p T has, as the pinned
 *  decomposition above sees them. */
template <class T>
inline constexpr std::size_t kFieldCount =
    std::tuple_size_v<decltype(fields(std::declval<T&>()))>;

}  // namespace detail

/** Equal only when PROVABLY identical: two easing curves compare equal when
 *  both are the same plain function pointer. A lambda-valued curve compares
 *  unequal, conservatively, because a std::function holding one cannot be
 *  inspected. One body, because a second spelling of this rule would let
 *  two comparators disagree about whether a node may prune. */
bool easeEqual(const choreograph::EaseFn& a, const choreograph::EaseFn& b);

/** Same duration, same delay, same curve under easeEqual's rule. */
bool transitionEqual(const motion::Transition& a, const motion::Transition& b);

/** Shaped bindings prune like anything else: same Output, same affine,
 *  same curve under easeEqual's conservative rule. A re-describe that
 *  only changes the CURVE must NOT prune — the map is read live, so a
 *  pruned node would keep shaping through the old one forever. EVERY
 *  FIELD OF BoundFloat APPEARS in the body, under the pin beside it. */
bool boundMapEqual(const motion::BoundFloat& a, const motion::BoundFloat& b);

static_assert(detail::kFieldCount<motion::Transitioned<float>> == 4,
              "Transitioned gained or lost a field — rule on it in "
              "propEqual() below, then bump this count.");
/** Two animatable slots are equal when they take the same form and that
 *  form's contents are equal: a plain value by `==`, a transitioned value
 *  by target, origin, waypoints and spec, a shaped binding by
 *  boundMapEqual, and a bare binding by the Output's identity — the
 *  pointer, not the number behind it. */
template <typename T>
bool propEqual(const motion::Animatable<T>& a, const motion::Animatable<T>& b) {
  if (a.index() != b.index()) return false;
  if (const T* plainA = a.plain()) return *plainA == *b.plain();
  if (const motion::Transitioned<T>* trA = a.transitioned()) {
    const motion::Transitioned<T>* trB = b.transitioned();
    return trA->value == trB->value && trA->from == trB->from &&
           trA->waypoints == trB->waypoints &&
           transitionEqual(trA->spec, trB->spec);
  }
  if (const motion::BoundFloat* mapA = a.boundMap())
    return boundMapEqual(*mapA, *b.boundMap());
  return a.binding() == b.binding();
}

}  // namespace sigil::core
