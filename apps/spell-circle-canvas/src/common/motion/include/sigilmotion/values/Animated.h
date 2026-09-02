#pragma once

/** @file
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

#include <choreograph/Choreograph.h>

#include <memory>
#include <optional>
#include <vector>

#include "sigilmotion/clock/Ticker.h"
#include "sigilmotion/values/Animatable.h"
#include "sigilmotion/values/Transition.h"

namespace sigil::motion {

/** One float property that can transition: the Choreograph output is the
 *  source of truth while a motion is connected. */
struct AnimatedFloat {
  choreograph::Output<float> value{0.0f};
  bool started = false;
  // Where the running motion is headed — lets a patch that does not change
  // this value's target leave the motion ALONE (no hitch, no re-held delay).
  float target = 0.0f;
};

/** A run of held motions, in declaration order — what a consumer keeps
 *  for a list of animatables whose length is a property of the
 *  description rather than of the consumer. */
using AnimatedFloats = std::vector<std::unique_ptr<AnimatedFloat>>;

/** Constant, binding, or transitioned — one animatable flattened. */
template <typename T>
struct ResolvedProp {
  T target{};
  const choreograph::Output<T>* binding = nullptr;
  const Transition* transition = nullptr;  // the value's own or the default
};

/** Reads one animatable against a transition the caller supplies as its
 *  default: a plain value takes that default, a transitioned value keeps
 *  its own spec instead, and a binding takes neither — it is already a
 *  running curve. */
template <typename T>
ResolvedProp<T> resolveProp(const Animatable<T>& v,
                            const std::optional<Transition>& fallback) {
  ResolvedProp<T> out;
  if (const T* plain = v.plain()) {
    out.target = *plain;
    if (fallback) out.transition = &*fallback;
  } else if (const Transitioned<T>* tr = v.transitioned()) {
    out.target = tr->value;
    out.transition = &tr->spec;
  } else {
    out.binding = v.binding();
  }
  return out;
}

/** The value an animatable reads as this frame: a bound Output wins
 *  (shaped through its map when it has one), then a running ramp, then
 *  the plain value. One body, so every reader agrees. */
float resolveFloatAt(const AnimatedFloat* anim, const Animatable<float>& v);

/** Starts (or retargets) the ramp held in `held` when the plain target
 *  changed. Returns true if a motion is running. The motion is passed
 *  rather than an index into a store, because how many of these a
 *  consumer keeps and where is the consumer's business — one body,
 *  every storage. */
bool transitionFloatAt(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                       const Animatable<float>& prevValue,
                       const Animatable<float>& nextValue,
                       const std::optional<Transition>& fallback);

/** An entrance: an animate(from(a).to(b)) value plays `from → value` when
 *  it FIRST appears — there is no previous value to diff against, so this
 *  is the "previous" the author declared — and a waypoint list plays its
 *  segments in turn. `extraDelaySeconds` is what the caller adds before
 *  the declared delay (a staggered entrance). A value with no entrance
 *  starts nothing. */
void mountEntrance(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                   const Animatable<float>& v, float extraDelaySeconds);

/** IS THIS VALUE MOVING RIGHT NOW? A slot with a live binding always is —
 *  the host writes the Output every frame and nothing here can see when
 *  it stops — and a slot with a ramp is moving while the ramp is
 *  CONNECTED.
 *
 *  Connected, not started. `AnimatedFloat::started` says a motion was
 *  begun and stays true for the rest of the value's life, so a reader
 *  that asks it can never learn that an entrance has landed and a node
 *  can cache again. `Output::isConnected()` is the fact: choreograph
 *  disconnects the output when its motion finishes.
 *
 *  This is the DECLARED half of stillness, and it is the half a
 *  description can answer on its own. What it cannot answer is whether a
 *  connected motion is actually changing the number — a wave held at one
 *  phase moves nothing — which is what `settled()` is for. */
bool isLive(const AnimatedFloat* anim, const Animatable<float>& v);

/** A SYNTHESIZED 0→1 PROGRESS: hold at 0 for the delay, then ramp to 1
 *  over the transition's duration on its curve.
 *
 *  For the value a host has to interpolate ITSELF because the description
 *  carries no float to point at — a colour crossfade, a shape morph, a
 *  two-image dissolve. The host keeps the endpoints and reads this
 *  progress between them, and because the ramp is authored here rather
 *  than at each such site, the mount and the retarget of one cannot drift
 *  apart. `extraDelaySeconds` is what the caller adds before the
 *  transition's own delay, exactly as `mountEntrance` takes it. */
void progressRamp(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                  const Transition& spec, float extraDelaySeconds);

}  // namespace sigil::motion
