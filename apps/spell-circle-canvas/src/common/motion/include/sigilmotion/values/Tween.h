#pragma once

/** @file
 * @ingroup motion-values
 *
 * `Tween<T>`, the one description of a motion: where it comes from, where
 * it goes or the keyframes it passes through, how long it takes, when it
 * starts, its curve, whether it repeats, and how a change mid-flight
 * composes — and `animate()`, which makes a property value of one.
 */

#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/time/Duration.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::motion {

/** HOW A CHANGE MID-FLIGHT COMPOSES with the motion already running.
 *
 *  `Replace` cuts the running motion and starts again from the value on
 *  screen — the new motion begins at rest. `Blend` keeps the running
 *  motion going and adds the change on top of it as a motion of its own,
 *  so the velocity the value had carries through the retarget instead of
 *  stopping dead: a pointer-follow or a scrubbed value stays smooth
 *  however often its target moves. */
enum class Composition : uint8_t { Replace, Blend };

/** ONE STEP of a keyframed tween: where it goes, how long it takes, and
 *  its curve. A step with no duration takes the tween's duration divided
 *  by the number of steps; a step with no curve takes the tween's. */
template <typename T>
struct Keyframe {
  T to{};
  std::optional<Duration> duration;
  Easing ease;

  bool operator==(const Keyframe& other) const {
    return to == other.to && duration == other.duration &&
           easeEqual(ease, other.ease);
  }
};

/** A MOTION, described.
 *
 *      animate({.from = 0.0f, .to = 1.0f, .duration = 320ms})   // an entrance
 *      animate({.to = lifted ? -8.0f : 0.0f})                   // eases on change
 *      animate({.from = 0.8f, .keyframes = {{.to = 1.07f, .duration = 80ms},
 *                                           {.to = 1.0f}}})     // a path
 *
 *  TWO SHAPES, ONE STRUCT, and the difference is the whole grammar:
 *
 *  - `.from` NAMED is an ENTRANCE. The motion plays once, when the
 *    property's owner first appears — from `from` through the keyframes
 *    or to `to` — and afterwards the property behaves as though only `.to`
 *    had been written.
 *  - `.to` ALONE eases on change. It says nothing about mounting: the
 *    property starts out holding `.to`, and whenever a later description
 *    carries a different one the property eases there from wherever it is,
 *    over this tween's timing, instead of snapping.
 *
 *  `delay` holds the value where it stands before the motion starts.
 *  `from`, `to`, `duration` and `delay` each take a plain value or a
 *  `stagger()`, which a host resolves from the child's place among its
 *  siblings: `.delay = stagger(40ms)` is the one-line cascade. `loop` plays the motion that many more times
 *  (-1 for ever) and `alternate` plays every other one backwards.
 *  `composition` says what a change mid-flight does to the motion already
 *  running.
 *
 *  Fields are declared in the order a designated initialiser names them:
 *  `from` before `to`. */
template <typename T>
struct Tween {
  std::optional<Staggered<T>> from;
  std::optional<Staggered<T>> to;
  /** The path after `from`, step by step. Non-empty, the last step's `to`
   *  is where the motion comes to rest and `.to` is not read. */
  std::vector<Keyframe<T>> keyframes;
  Staggered<Duration> duration = Duration(std::chrono::milliseconds(250));
  Staggered<Duration> delay = Duration{};
  Easing ease = ease::outQuad;
  int loop = 0;
  bool alternate = false;
  Composition composition = Composition::Replace;

  /** WHERE THE MOTION COMES TO REST: the last keyframe's `to`, else `to`,
   *  else `from` — for a child alone; `resolved()` places it first. */
  [[nodiscard]] T rest() const {
    if (!keyframes.empty()) return keyframes.back().to;
    if (to) return to->value();
    if (from) return from->value();
    return T{};
  }
  /** THIS TWEEN FOR THE CHILD AT @p place: every staggered field resolved
   *  to that child's plain value. */
  [[nodiscard]] Tween resolved(Place place) const {
    Tween out = *this;
    if (from) out.from = from->at(place);
    if (to) out.to = to->at(place);
    out.duration = duration.at(place);
    out.delay = delay.at(place);
    return out;
  }
  /** Whether any field differs per child. */
  [[nodiscard]] bool isStaggered() const {
    return (from && from->isStaggered()) || (to && to->isStaggered()) ||
           duration.isStaggered() || delay.isStaggered();
  }
  /** The curve, with an empty one read as the default. */
  [[nodiscard]] const Easing& easing() const {
    static const Easing kDefault = ease::outQuad;
    return ease ? ease : kDefault;
  }
  /** Whether the motion plays when its owner first appears. */
  [[nodiscard]] bool isEntrance() const { return from.has_value(); }

  /** THE VALUE @p time AFTER THE MOTION STARTS, read with no engine: a
   *  function of the time and nothing else, so a bake, a scrub, a shader's
   *  uniform and a value read three times in one frame all get the same
   *  number. The delay holds `from`; the passes repeat as `loop` and
   *  `alternate` say; past the last pass the value is where it rests. A
   *  tween with no `from` starts at `to`. Needs `T` to add, subtract and
   *  scale by a float. */
  [[nodiscard]] T at(Duration time) const {
    const T start = from ? from->value() : rest();
    const Duration length = duration.value();
    const Duration wait = delay.value();
    std::vector<Keyframe<T>> steps = keyframes;
    if (steps.empty()) steps.push_back({rest(), length, {}});
    const Duration share = length / (double)steps.size();
    Duration pass{};
    for (const Keyframe<T>& step : steps) pass += step.duration.value_or(share);
    if (time <= wait) return start;
    Duration into = time - wait;
    int index = 0;
    if (pass > Duration{}) {
      index = (int)std::floor(into / pass);
      if (loop >= 0 && index > loop) return (alternate && loop % 2 == 1) ? start : rest();
      into -= pass * (double)index;
    }
    if (alternate && index % 2 == 1) into = pass - into;
    T at = start;
    for (const Keyframe<T>& step : steps) {
      const Duration stepLength = step.duration.value_or(share);
      if (into <= stepLength) {
        const float unit =
            stepLength > Duration{} ? (float)(into / stepLength) : 1.0f;
        const Easing& curve = step.ease ? step.ease : easing();
        return at + (step.to - at) * curve(unit);
      }
      into -= stepLength;
      at = step.to;
    }
    return at;
  }
};

/** Same endpoints, same keyframes, same timing, same curves under
 *  `easeEqual`'s rule, same repeat and the same composition. */
template <typename T>
bool tweenEqual(const Tween<T>& left, const Tween<T>& right) {
  return left.from == right.from && left.to == right.to &&
         left.keyframes == right.keyframes &&
         left.duration == right.duration && left.delay == right.delay &&
         easeEqual(left.easing(), right.easing()) && left.loop == right.loop &&
         left.alternate == right.alternate &&
         left.composition == right.composition;
}

template <typename T>
class Animatable;

/** A PROPERTY VALUE THAT MOVES: `.opacity(animate({.from = 0.0f, .to = 1.0f}))`.
 *  The runtime manufactures the motion, as against `animatable()`, where
 *  the caller writes the value itself. A consumer that bakes a still
 *  rather than running frames resolves it to where it comes to rest. */
Animatable<float> animate(Tween<float> tween);

/** The same, for any other value type: `animate<Fill>({.to = accent})`. */
template <typename T>
Animatable<T> animate(Tween<T> tween);

}  // namespace sigil::motion
