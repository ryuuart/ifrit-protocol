#pragma once

/** @file
 * @ingroup motion-values
 *
 * An `Animatable<float>` while it is MOVING: the held motion a ticker
 * runs for it, the value it reads as this frame, whether it is moving at
 * all, the retarget that bends a running ramp onto a new endpoint, and
 * the entrance it plays the first time it appears.
 *
 * `Animatable<T>` is the value a description carries; `AnimatedFloat` is
 * what a consumer that retains state holds beside it while a motion is
 * connected. Everything here is stated over one held motion, so a
 * consumer's storage — a fixed array, a vector, one member — is its own
 * business.
 */

#include <memory>
#include <optional>
#include <vector>

#include "sigilmotion/clock/Ticker.h"
#include "sigilmotion/values/Animatable.h"
#include "sigilmotion/values/Transition.h"

namespace sigil::motion {

/** One float property that can transition: its live value is the source
 *  of truth while a motion is writing it. */
struct AnimatedFloat {
  Animatable<float> value = animatable(0.0f);
  bool started = false;
  // Where the running motion is headed — lets a patch that does not change
  // this value's target leave the motion ALONE (no hitch, no re-held delay).
  float target = 0.0f;

  /** A motion is writing the value now. */
  [[nodiscard]] bool isMoving() const { return value.cell()->moving; }
  /** Stops whatever motion is writing the value; the number stays where
   *  the motion left it. */
  void stop();
  /** The value this frame. */
  [[nodiscard]] float current() const { return value.value(); }

  /** The motion writing the value, while there is one — what a blended
   *  retarget adds its change onto. */
  std::shared_ptr<detail::Stepped> running;
};

/** A run of held motions, in declaration order — what a consumer keeps
 *  for a list of animatables whose length is a property of the
 *  description rather than of the consumer. */
using AnimatedFloats = std::vector<std::unique_ptr<AnimatedFloat>>;

/** Constant, live, or described — one animatable flattened. */
template <typename T>
struct ResolvedProperty {
  T target{};
  /** The slot, when it holds a live or shaped value: it is already a
   *  running number, so it takes no transition. */
  const Animatable<T>* live = nullptr;
  /** How a change to `target` eases: a described motion's own timing, or
   *  the default for a constant; nothing where a change snaps. */
  std::optional<Transition> transition;
};

/** A described motion's timing as the transition a change eases by, for
 *  the child at @p place. */
template <typename T>
Transition transitionOf(const Tween<T>& tween, Place place = {}) {
  return {tween.duration.at(place), tween.delay.at(place), tween.easing(),
          tween.composition};
}

/** Reads one animatable against a transition the caller supplies as its
 *  default: a constant takes that default, a described motion keeps its
 *  own timing instead — resolved for the child at @p place — and a live
 *  value takes neither: it is already a running number. */
template <typename T>
ResolvedProperty<T> resolveProperty(const Animatable<T>& property,
                                    const std::optional<Transition>& fallback,
                                    Place place = {}) {
  ResolvedProperty<T> out;
  if (const T* constant = property.constant()) {
    out.target = *constant;
    out.transition = fallback;
  } else if (const Tween<T>* described = property.described()) {
    out.target = described->resolved(place).rest();
    out.transition = transitionOf(*described, place);
  } else {
    out.live = &property;
  }
  return out;
}

/** The value an animatable reads as this frame: a live value wins
 *  (shaped through its stages when it has any), then a running ramp, then
 *  the constant. One body, so every reader agrees. */
float resolveFloatAt(const AnimatedFloat* animated, const Animatable<float>& property);

/** Starts (or retargets) the ramp held in `held` when the target
 *  changed. Returns true if a motion is running. The motion is passed
 *  rather than an index into a store, because how many of these a
 *  consumer keeps and where is the consumer's business — one body,
 *  every storage. @p place is where the value's owner stands among its
 *  siblings, which a staggered tween resolves against. */
bool transitionFloatAt(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                       const Animatable<float>& previousValue,
                       const Animatable<float>& nextValue,
                       const std::optional<Transition>& fallback,
                       Place place = {});

/** An entrance: a tween that names `.from` plays from there — through its
 *  keyframes, or to `.to` — when it FIRST appears: there is no previous
 *  value to diff against, so `from` is the "previous" the author
 *  declared. It repeats as its `loop` and `alternate` say, and every
 *  staggered field resolves against @p place, where the owner stands among
 *  its siblings. A value with no entrance starts nothing. */
void mountEntrance(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                   const Animatable<float>& property, Place place = {});

/** IS THIS VALUE MOVING RIGHT NOW? A live value always is — the hand that
 *  writes it can stop at any frame and nothing here can see when — and a
 *  held ramp is moving while a motion is writing it.
 *
 *  This is the DECLARED half of stillness, and it is the half a
 *  description can answer on its own. What it cannot answer is whether a
 *  moving value is actually changing the number — a wave held at one
 *  phase moves nothing — which is what `settled()` is for. */
bool isLive(const AnimatedFloat* animated, const Animatable<float>& property);

/** A SYNTHESIZED 0→1 PROGRESS: hold at 0 for the delay, then ramp to 1
 *  over the transition's duration on its curve.
 *
 *  For the value a host has to interpolate ITSELF because the description
 *  carries no float to point at — a colour crossfade, a shape morph, a
 *  two-image dissolve. The host keeps the endpoints and reads this
 *  progress between them, and because the ramp is authored here rather
 *  than at each such site, the mount and the retarget of one cannot drift
 *  apart. */
void progressRamp(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                  const Transition& spec);

}  // namespace sigil::motion
