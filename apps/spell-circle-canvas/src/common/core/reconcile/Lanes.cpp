/** @file
 * The lane operations: resolving a lane for this frame, retargeting a
 * running ramp from its current value when a patch moves the target, and
 * starting a mount entrance from the value a description declared.
 */

#include "sigilcore/reconcile/Lanes.h"

#include <algorithm>
#include <chrono>

namespace sigil::core {

float resolveFloatAt(const AnimatedFloat* anim,
                     const motion::Animatable<float>& v) {
  if (const choreograph::Output<float>* binding = v.binding()) {
    // A shaped binding (bind(&out).map().to()…) runs its map here — the
    // one place a bound float is read, so every consumer gets it for free.
    if (const motion::BoundFloat* shape = v.boundMap())
      return shape->apply(binding->value());
    return binding->value();
  }
  if (anim && anim->started) return anim->value.value();
  if (const float* plain = v.plain()) return *plain;
  return v.transitioned()->value;
}

bool transitionFloatAt(motion::Ticker& ticker,
                       std::unique_ptr<AnimatedFloat>& slotAnim,
                       const motion::Animatable<float>& prevValue,
                       const motion::Animatable<float>& nextValue,
                       const std::optional<motion::Transition>& nodeDefault) {
  ResolvedProp<float> prev = resolveProp(prevValue, nodeDefault);
  ResolvedProp<float> next = resolveProp(nextValue, nodeDefault);
  // Snap semantics must actually LAND: a lingering ramp from an earlier
  // transition would shadow the plain description forever (resolveFloatAt
  // prefers a started anim), so the snap paths disconnect it.
  auto snapAnim = [&] {
    if (auto& anim = slotAnim; anim && anim->started) {
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

  auto& anim = slotAnim;
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
    // left alone it would carry the slot to a STALE target (permanent,
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

void mountEntrance(motion::Ticker& ticker,
                   std::unique_ptr<AnimatedFloat>& slotAnim,
                   const motion::Animatable<float>& v,
                   float extraDelaySeconds) {
  const motion::Transitioned<float>* tr = v.transitioned();
  if (!tr) return;
  // animate(through({…})): the multi-segment mount path — checked BEFORE
  // the from==value guard (a shake 0→−20→0 starts and ends equal).
  if (tr->waypoints.size() >= 2) {
    auto& anim = slotAnim;
    if (!anim) anim = std::make_unique<AnimatedFloat>();
    const float first = tr->waypoints.front().second;
    anim->value = first;
    anim->started = true;
    anim->target = tr->waypoints.back().second;
    auto motion = ticker.timeline().apply(&anim->value);
    const float lead =
        std::chrono::duration<float>(tr->spec.delay).count() +
        extraDelaySeconds +
        std::chrono::duration<float>(tr->waypoints.front().first).count();
    if (lead > 0) motion.then<choreograph::Hold>(first, lead);
    for (size_t i = 1; i < tr->waypoints.size(); ++i) {
      const float seg = std::chrono::duration<float>(tr->waypoints[i].first -
                                                     tr->waypoints[i - 1].first)
                            .count();
      motion.then<choreograph::RampTo>(tr->waypoints[i].second,
                                       std::max(seg, 0.0f), tr->spec.easing());
    }
    return;
  }
  if (!tr->from || *tr->from == tr->value) return;
  auto& anim = slotAnim;
  if (!anim) anim = std::make_unique<AnimatedFloat>();
  anim->value = *tr->from;
  anim->started = true;
  anim->target = tr->value;
  auto motion = ticker.timeline().apply(&anim->value);
  const float delay = std::chrono::duration<float>(tr->spec.delay).count() +
                      extraDelaySeconds;  // a staggered mount's carry
  if (delay > 0)  // stagger: hold the `from` before entering
    motion.then<choreograph::Hold>(*tr->from, delay);
  motion.then<choreograph::RampTo>(
      tr->value, std::chrono::duration<float>(tr->spec.duration).count(),
      tr->spec.easing());
}

}  // namespace sigil::core
