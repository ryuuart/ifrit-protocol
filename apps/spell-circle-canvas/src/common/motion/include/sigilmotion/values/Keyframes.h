#pragma once

/** @file
 * The keyframe builders: Transitioned<T>, the value together with how
 * it moves, and the `animate(from(a).to(b))`, `animate(to(v))` and
 * `animate(through({…}))` spellings that build one.
 */

#include <choreograph/Choreograph.h>

#include <chrono>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include "sigilmotion/values/Transition.h"

namespace sigil::motion {

template <typename T>
/** A value together with how it should move when it changes. Assigning
 *  a new value does not jump to it: the transition spec says how to get
 *  there, and the optional entrance and waypoint paths say what happens
 *  the first time the value is mounted. */
struct Transitioned {
  /** Value-initialized, not default-initialized. `animate(through({}))`
   *  builds one of these and then fills nothing; for a scalar T,
   *  default-initialization would leave the property reading whatever
   *  was on the stack. An empty keyframe list is a degenerate ask, but
   *  it has to be a DETERMINATE one — zero. */
  T value{};
  Transition spec;
  /** animate(from(a).to(b)): where the value ENTERS from when the node
   *  first mounts. Empty for a plain `animate(to(v))` — no entrance, only
   *  change transitions. */
  std::optional<T> from{};
  /** animate(through({...})): the mount-time path as (absolute time,
   *  value) pairs — multi-segment entrances (damped overshoots) that one
   *  from→to ramp can't shape. Mount-only choreography: later changes
   *  retarget to `value` like any `animate(to(v))`. */
  std::vector<std::pair<std::chrono::milliseconds, T>> waypoints{};
};

/** The argument builders for animate(). Nobody spells these types; they
 *  exist so the call site reads `animate(from(a).to(b), spec)`. Beware
 *  that a local variable named `from` or `to` shadows the factory —
 *  qualify the call (`motion::from(...)`) at any site that has one.
 *
 *  TWO SHAPES, ONE VERB, and the difference is the whole grammar:
 *
 *  - `to(v)` ALONE is RAMP ON CHANGE. It says nothing about mounting; it
 *    says that whenever the described value differs from what the
 *    instance holds, the property ramps to the new one instead of
 *    snapping. Re-describing a different `to(v)` retargets from the
 *    CURRENT value.
 *  - `from(a).to(b)` is a MOUNT ENTRANCE. The path plays once, when the
 *    node first appears; afterwards the property behaves exactly like
 *    `to(b)`.
 *
 *  So an author who wants "this number should never jump" writes the
 *  first, and an author who wants "this should fly in" writes the
 *  second — one verb, and the argument says which. */
template <typename T>
struct FromTo {
  T from;
  T to;
};
/** The ramp-on-change argument: `animate(to(v), spec)`. */
template <typename T>
struct To {
  T value;
};
/** The half-built entrance argument: `from(a)` on its own is not yet an
 *  animation, and `.to(b)` completes it. */
template <typename T>
struct From {
  T value;
  FromTo<T> to(T target) { return {std::move(value), std::move(target)}; }
};
template <typename T>
From<T> from(T v) {
  return {std::move(v)};
}
template <typename T>
To<T> to(T v) {
  return {std::move(v)};
}

template <typename T>
/** The mount-time path argument: `animate(through({...}))`. Each frame
 *  pairs an ABSOLUTE time from the start of the path with the value to
 *  hold at that time. */
struct Waypoints {
  std::vector<std::pair<std::chrono::milliseconds, T>> frames;
};

/** The waypoint path for the common float case, spelled without a
 *  template argument. A nested braced list is a non-deduced context, so
 *  the generic overload below cannot deduce its element type and has to
 *  be told it explicitly. */
inline Waypoints<float> through(
    std::initializer_list<std::pair<std::chrono::milliseconds, float>> frames) {
  return {{frames.begin(), frames.end()}};
}
/** Any other value type — `through<SkColor4f>({...})`. */
template <typename T>
Waypoints<T> through(
    std::vector<std::pair<std::chrono::milliseconds, T>> frames) {
  return {std::move(frames)};
}

/** A MOUNT ENTRANCE: the runtime manufactures the motion, as against
 *  `bind()`, where the caller drives an Output itself. The first word at
 *  the call site names who owns the motion.
 *
 *      .opacity(animate(from(0.0f).to(1.0f), {400ms}))
 *      .scale(animate(from(0.86f).to(1.0f), {520ms, ease::outBack()}))
 *
 *  The path plays once, when the property's owner first appears — the
 *  same idea as a CSS animation-on-enter. Afterwards the property behaves
 *  exactly like `animate(to(v), spec)`: later changes retarget from the
 *  CURRENT value, and re-describing the same animate() does not restart
 *  the entrance.
 *
 *  A consumer that bakes a still rather than running frames resolves this
 *  to its settled value; an entrance needs a mount to play.
 *
 *  `spec` defaults to the house Transition, so name one only when the
 *  beat matters. */
template <typename T>
Transitioned<T> animate(FromTo<T> ft, Transition spec = {}) {
  return {std::move(ft.to), std::move(spec), std::move(ft.from)};
}

/** RAMP ON CHANGE, with a transition of this property's own:
 *
 *      .opacity(animate(to(dimmed ? 0.4f : 1.0f), {180ms}))
 *
 *  No entrance — the property starts out holding the value. Every LATER
 *  description carrying a different value ramps to it over `spec` instead
 *  of snapping, retargeting from wherever the property currently is.
 *  There is one motion per property, so a change mid-ramp bends the ramp
 *  rather than queueing a second one behind it. */
template <typename T>
Transitioned<T> animate(To<T> t, Transition spec = {}) {
  return {std::move(t.value), std::move(spec)};
}

/** The keyframe path: absolute (time, value) waypoints played through on
 *  first appearance — the multi-segment entrances, such as a damped
 *  overshoot, that a single from→to ramp cannot shape:
 *
 *    .translateX(animate(through(
 *        {{0ms, 40.f}, {200ms, -20.f}, {300ms, 10.f}, {400ms, 0.f}})))
 *
 *  `ease` applies PER SEGMENT. A leading time greater than zero holds the
 *  first value, which is a built-in delay; `Transition::delay` still adds
 *  on top of it. Once the path completes the value behaves like
 *  `animate(to(last))`. */
template <typename T>
Transitioned<T> animate(Waypoints<T> w,
                        choreograph::EaseFn ease = &choreograph::easeOutQuad) {
  Transitioned<T> t;
  t.spec.ease = std::move(ease);
  if (!w.frames.empty()) {
    t.from = w.frames.front().second;
    t.value = w.frames.back().second;
    t.spec.duration = w.frames.back().first;
  }
  t.waypoints = std::move(w.frames);
  return t;
}

}  // namespace sigil::motion
