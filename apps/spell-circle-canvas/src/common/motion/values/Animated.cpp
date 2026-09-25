/** @file
 * The operations on a held motion: reading the value for this frame,
 * retargeting a running ramp from where it is when the target moves, and
 * starting an entrance from the value the description declared.
 */

#include "sigilmotion/values/Animated.h"

#include <algorithm>
#include <chrono>

#include "sigilmotion/bind/BoundFloat.h"

namespace sigil::motion {

float resolveFloatAt(const AnimatedFloat* animated, const Animatable<float>& property) {
  if (const choreograph::Output<float>* binding = property.binding()) {
    // A shaped binding (bind(&out).map().to()…) runs its map here — the
    // one place a bound float is read, so every consumer gets it for free.
    if (const BoundFloat* shape = property.boundMap())
      return shape->apply(binding->value());
    return binding->value();
  }
  if (animated && animated->started) return animated->value.value();
  if (const float* plain = property.plain()) return *plain;
  return property.transitioned()->value;
}

bool transitionFloatAt(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                       const Animatable<float>& previousValue,
                       const Animatable<float>& nextValue,
                       const std::optional<Transition>& fallback) {
  ResolvedProperty<float> prev = resolveProperty(previousValue, fallback);
  ResolvedProperty<float> next = resolveProperty(nextValue, fallback);
  // Snap semantics must actually LAND: a lingering ramp from an earlier
  // transition would shadow the plain description forever (resolveFloatAt
  // prefers a started anim), so the snap paths disconnect it.
  auto snapAnim = [&] {
    if (auto& anim = held; anim && anim->started) {
      anim->value.disconnect();
      anim->started = false;
    }
  };
  if (next.binding || !next.transition) {
    snapAnim();
    return false;  // bound, or plain snap
  }
  if (prev.binding) {
    snapAnim();
    return false;  // binding → constant: snap (no meaningful "from")
  }

  auto& anim = held;
  // A running motion already headed at this exact target keeps flying —
  // an unrelated prop patch mid-entrance must not restart it (and must
  // never re-hold its delay).
  if (anim && anim->started && anim->value.isConnected() &&
      anim->target == next.target)
    return true;
  const float current =
      anim && anim->started ? anim->value.value() : prev.target;
  if (current == next.target) {
    // The value COINCIDES with the new target, but a connected motion that
    // passed the keeps-flying guard is provably headed somewhere else —
    // left alone it would carry the value to a STALE target (permanent,
    // since identical re-describes prune). Disconnect; the description's
    // own value (== next.target) shows through.
    if (anim && anim->started && anim->value.isConnected() &&
        anim->target != next.target)
      snapAnim();
    return anim && anim->value.isConnected();
  }

  if (!anim) anim = std::make_unique<AnimatedFloat>();
  anim->value = current;  // seed the retarget start point
  anim->started = true;
  anim->target = next.target;
  auto motion = ticker.timeline().apply(&anim->value);
  const float delay =
      std::chrono::duration<float>(next.transition->delay).count();
  if (delay > 0)
    motion.then<choreograph::Hold>(current, delay);  // the stagger primitive
  motion.then<choreograph::RampTo>(
      next.target,
      std::chrono::duration<float>(next.transition->duration).count(),
      next.transition->easing());
  return true;
}

void mountEntrance(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                   const Animatable<float>& property, float extraDelaySeconds) {
  const Transitioned<float>* transitioned = property.transitioned();
  if (!transitioned) return;
  // animate(through({…})): the multi-segment entrance — checked BEFORE
  // the from==value guard (a shake 0→−20→0 starts and ends equal).
  if (transitioned->waypoints.size() >= 2) {
    auto& anim = held;
    if (!anim) anim = std::make_unique<AnimatedFloat>();
    const float first = transitioned->waypoints.front().second;
    anim->value = first;
    anim->started = true;
    anim->target = transitioned->waypoints.back().second;
    auto motion = ticker.timeline().apply(&anim->value);
    const float lead =
        std::chrono::duration<float>(transitioned->spec.delay).count() +
        extraDelaySeconds +
        std::chrono::duration<float>(transitioned->waypoints.front().first).count();
    if (lead > 0) motion.then<choreograph::Hold>(first, lead);
    for (size_t i = 1; i < transitioned->waypoints.size(); ++i) {
      const float seg = std::chrono::duration<float>(transitioned->waypoints[i].first -
                                                     transitioned->waypoints[i - 1].first)
                            .count();
      motion.then<choreograph::RampTo>(transitioned->waypoints[i].second,
                                       std::max(seg, 0.0f), transitioned->spec.easing());
    }
    return;
  }
  if (!transitioned->from || *transitioned->from == transitioned->value) return;
  auto& anim = held;
  if (!anim) anim = std::make_unique<AnimatedFloat>();
  anim->value = *transitioned->from;
  anim->started = true;
  anim->target = transitioned->value;
  auto motion = ticker.timeline().apply(&anim->value);
  const float delay = std::chrono::duration<float>(transitioned->spec.delay).count() +
                      extraDelaySeconds;  // a staggered entrance's carry
  if (delay > 0)  // stagger: hold the `from` before entering
    motion.then<choreograph::Hold>(*transitioned->from, delay);
  motion.then<choreograph::RampTo>(
      transitioned->value, std::chrono::duration<float>(transitioned->spec.duration).count(),
      transitioned->spec.easing());
}

bool isLive(const AnimatedFloat* animated, const Animatable<float>& property) {
  return property.binding() != nullptr || (animated && animated->value.isConnected());
}

void progressRamp(Ticker& ticker, std::unique_ptr<AnimatedFloat>& held,
                  const Transition& spec, float extraDelaySeconds) {
  if (!held) held = std::make_unique<AnimatedFloat>();
  held->value = 0.0f;
  held->started = true;
  held->target = 1.0f;
  auto ramp = ticker.timeline().apply(&held->value);
  const float delay =
      std::chrono::duration<float>(spec.delay).count() + extraDelaySeconds;
  if (delay > 0) ramp.then<choreograph::Hold>(0.0f, delay);
  ramp.then<choreograph::RampTo>(
      1.0f, std::chrono::duration<float>(spec.duration).count(), spec.easing());
}

}  // namespace sigil::motion
