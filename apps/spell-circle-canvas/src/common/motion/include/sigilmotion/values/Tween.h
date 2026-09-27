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
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Interpolate.h>

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
 *      element.transition({.duration = 320ms, .ease = ease::outBack()})
 *
 *  A TWEEN WITH NO ENDPOINTS IS A TRANSITION: the timing every plain value
 *  on a node eases by when a later description changes it. A consumer
 *  that takes a tween that way reads its `duration`, `delay` (a
 *  `stagger()` resolved for the child), curve and `composition`, and
 *  ignores `from`, `to`, the keyframes and the repeat, because the
 *  endpoints are the value's own old and new.
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
  /** The curve; empty reads as `ease::outQuad` (`easing()`), and a
   *  collective that runs a unit's progress through it — a text track —
   *  reads empty as straight. */
  Easing ease;
  int loop = 0;
  /** Held at the end of every pass before the next one starts, when the
   *  motion loops — anime.js's `loopDelay`. */
  Duration loopDelay{};
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
   *  tween with no `from` starts at `to`. Moves through `interpolate()`,
   *  the line between two values of `T` the engine moves by too. */
  [[nodiscard]] T at(Duration time) const
    requires Interpolable<T>
  {
    const T start = from ? from->value() : rest();
    const Duration length = duration.value();
    const Duration wait = delay.value();
    // A tween with no keyframes is one step to where it rests.
    const Keyframe<T> single{rest(), length, {}};
    const Keyframe<T>* steps = keyframes.empty() ? &single : keyframes.data();
    const size_t count = keyframes.empty() ? 1 : keyframes.size();
    const Duration share = length / (double)count;
    Duration pass{};
    for (size_t index = 0; index < count; ++index)
      pass += steps[index].duration.value_or(share);
    // A looping pass ends on its hold, where the value rests at the last
    // step's end until the next pass starts.
    if (loop != 0) pass += loopDelay;
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
    for (size_t step = 0; step < count; ++step) {
      const Duration stepLength = steps[step].duration.value_or(share);
      if (into <= stepLength) {
        const float unit =
            stepLength > Duration{} ? (float)(into / stepLength) : 1.0f;
        const Easing& curve = steps[step].ease ? steps[step].ease : easing();
        return interpolate(at, steps[step].to, curve(unit));
      }
      into -= stepLength;
      at = steps[step].to;
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
         left.loopDelay == right.loopDelay && left.alternate == right.alternate &&
         left.composition == right.composition;
}

/** A TWEEN AS A COLLECTIVE'S SCHEDULE READS IT: its delay — a
 *  `stagger()` or `cues()` resolved over the run's units — the length of
 *  one unit's motion, and whether it loops, holding `loopDelay` between
 *  passes and running every other one backwards under `alternate`; with
 *  @p within, a second stagger inside every beat. A collective loops for
 *  as long as its master progress wraps, so any `loop` other than zero
 *  loops. A staggered `duration` reads as a unit alone's: one schedule
 *  has one beat length. */
template <typename T>
Timing timingOf(const Tween<T>& tween,
                std::optional<Staggered<Duration>> within = std::nullopt) {
  Timing timing;
  timing.delay = tween.delay;
  timing.duration = tween.duration.value();
  timing.loop = tween.loop != 0;
  timing.loopDelay = tween.loopDelay;
  timing.alternate = tween.alternate;
  timing.within = std::move(within);
  return timing;
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
