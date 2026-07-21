// Transitions: resolving an animatable float to its current value (binding,
// running ramp, or plain), and starting/retargeting Choreograph ramps when a
// reconciled plain-constant change occurs (the SwiftUI implicit-transition
// lesson: one motion per (instance, property), retarget-from-current).

#include "ComposeRuntime.h"

#include <chrono>

namespace sigil::compose {

using namespace detail;

float detail::Instance::resolveFloat(Slot slot,
                                     const PropValue<float> &v) const {
  if (const auto *binding = std::get_if<const choreograph::Output<float> *>(&v))
    return (*binding)->value();
  if (anims[slot] && anims[slot]->started)
    return anims[slot]->value.value();
  if (const float *plain = std::get_if<float>(&v))
    return *plain;
  return std::get<Transitioned<float>>(v).value;
}

namespace {

/** Starts (or retargets) a ramp on `slot` when the plain target changed.
 *  Returns true if a motion is running. */
bool transitionFloat(Composer::Impl &impl, Instance &inst, Instance::Slot slot,
                     const PropValue<float> &prevValue,
                     const PropValue<float> &nextValue,
                     const std::optional<Transition> &nodeDefault) {
  ResolvedProp<float> prev = resolveProp(prevValue, nodeDefault);
  ResolvedProp<float> next = resolveProp(nextValue, nodeDefault);
  // Snap semantics must actually LAND: a lingering ramp from an earlier
  // transition would shadow the plain description forever (resolveFloat
  // prefers a started anim), so the snap paths disconnect it.
  auto snapAnim = [&] {
    if (auto &anim = inst.anims[slot]; anim && anim->started) {
      anim->value.disconnect();
      anim->started = false;
    }
  };
  if (next.binding || !next.transition) {
    snapAnim();
    return false; // bound, or plain snap
  }
  if (prev.binding) {
    snapAnim();
    return false; // binding → constant: snap (no meaningful "from")
  }

  auto &anim = inst.anims[slot];
  // A running motion already headed at this exact target keeps flying —
  // an unrelated prop patch mid-entrance must not restart it (and must
  // never re-hold its delay).
  if (anim && anim->started && anim->value.isConnected() &&
      anim->target == next.target)
    return true;
  const float current =
      anim && anim->started ? anim->value.value() : prev.target;
  if (current == next.target)
    return anim && anim->value.isConnected();

  if (!anim)
    anim = std::make_unique<AnimatedFloat>();
  anim->value = current; // seed the retarget start point
  anim->started = true;
  anim->target = next.target;
  auto motion = impl.ticker.timeline().apply(&anim->value);
  const float delay =
      std::chrono::duration<float>(next.transition->delay).count();
  if (delay > 0)
    motion.then<choreograph::Hold>(current, delay); // the stagger primitive
  motion.then<choreograph::RampTo>(
      next.target,
      std::chrono::duration<float>(next.transition->duration).count(),
      next.transition->ease);
  return true;
}

} // namespace

/** Mount entrances: a withFrom() value plays `from → value` when the node
 *  FIRST appears (there is no prev to diff against — this is the "prev" the
 *  author declared). Skipped for snapshot()/measure() (liveOnly: no live
 *  timeline — bakes render the settled value). */
void Composer::Impl::applyMountTransitions(Instance &inst,
                                           const ElementNode &node) {
  if (liveOnly)
    return;

  auto entrance = [&](Instance::Slot slot, const PropValue<float> &v) {
    const Transitioned<float> *tr = std::get_if<Transitioned<float>>(&v);
    if (!tr)
      return;
    // withKeyframes(): the multi-segment mount path — checked BEFORE the
    // from==value guard (a shake 0→−20→0 starts and ends equal).
    if (tr->waypoints.size() >= 2) {
      auto &anim = inst.anims[slot];
      if (!anim)
        anim = std::make_unique<AnimatedFloat>();
      const float first = tr->waypoints.front().second;
      anim->value = first;
      anim->started = true;
      anim->target = tr->waypoints.back().second;
      auto motion = ticker.timeline().apply(&anim->value);
      const float lead =
          std::chrono::duration<float>(tr->spec.delay).count() +
          mountDelayCarryMs / 1000.0f +
          std::chrono::duration<float>(tr->waypoints.front().first).count();
      if (lead > 0)
        motion.then<choreograph::Hold>(first, lead);
      for (size_t i = 1; i < tr->waypoints.size(); ++i) {
        const float seg = std::chrono::duration<float>(
                              tr->waypoints[i].first -
                              tr->waypoints[i - 1].first)
                              .count();
        motion.then<choreograph::RampTo>(tr->waypoints[i].second,
                                         std::max(seg, 0.0f), tr->spec.ease);
      }
      return;
    }
    if (!tr->from || *tr->from == tr->value)
      return;
    auto &anim = inst.anims[slot];
    if (!anim)
      anim = std::make_unique<AnimatedFloat>();
    anim->value = *tr->from;
    anim->started = true;
    anim->target = tr->value;
    auto motion = ticker.timeline().apply(&anim->value);
    const float delay =
        std::chrono::duration<float>(tr->spec.delay).count() +
        mountDelayCarryMs / 1000.0f; // staggerChildren() carry
    if (delay > 0) // stagger: hold the `from` before entering
      motion.then<choreograph::Hold>(*tr->from, delay);
    motion.then<choreograph::RampTo>(
        tr->value, std::chrono::duration<float>(tr->spec.duration).count(),
        tr->spec.ease);
  };
  entrance(Instance::kOpacity, node.paint.opacity);
  entrance(Instance::kTx, node.paint.translateX);
  entrance(Instance::kTy, node.paint.translateY);
  entrance(Instance::kRotate, node.paint.rotate);
  entrance(Instance::kScale, node.paint.scale);
  entrance(Instance::kSkewX, node.paint.skewX);
  entrance(Instance::kSkewY, node.paint.skewY);
  if (node.hasTrim) {
    entrance(Instance::kTrimStart, node.trimStart);
    entrance(Instance::kTrimEnd, node.trimEnd);
    entrance(Instance::kTrimOffset, node.trimOffset);
  }
  if (node.glyphFx)
    entrance(Instance::kGlyphProgress, node.glyphFx->progress);

  // Color fill entrance: from → to through the kFillLerp progress.
  if (node.paint.fill) {
    const Transitioned<Fill> *tr =
        std::get_if<Transitioned<Fill>>(&*node.paint.fill);
    if (tr && tr->from && tr->from->kind == Fill::Kind::Color &&
        tr->value.kind == Fill::Kind::Color && !(*tr->from == tr->value)) {
      inst.fillFrom = *tr->from;
      inst.fillTo = tr->value;
      auto &anim = inst.anims[Instance::kFillLerp];
      if (!anim)
        anim = std::make_unique<AnimatedFloat>();
      anim->value = 0.0f;
      anim->started = true;
      auto motion = ticker.timeline().apply(&anim->value);
      const float delay =
          std::chrono::duration<float>(tr->spec.delay).count() +
          mountDelayCarryMs / 1000.0f; // staggerChildren() carry
      if (delay > 0)
        motion.then<choreograph::Hold>(0.0f, delay);
      motion.then<choreograph::RampTo>(
          1.0f, std::chrono::duration<float>(tr->spec.duration).count(),
          tr->spec.ease);
    }
  }
}

void Composer::Impl::applyTransitions(Instance &inst, const ElementNode &prev,
                                      const ElementNode &next) {
  const auto &nd = next.nodeTransition;
  transitionFloat(*this, inst, Instance::kOpacity, prev.paint.opacity,
                  next.paint.opacity, nd);
  transitionFloat(*this, inst, Instance::kTx, prev.paint.translateX,
                  next.paint.translateX, nd);
  transitionFloat(*this, inst, Instance::kTy, prev.paint.translateY,
                  next.paint.translateY, nd);
  transitionFloat(*this, inst, Instance::kRotate, prev.paint.rotate,
                  next.paint.rotate, nd);
  transitionFloat(*this, inst, Instance::kScale, prev.paint.scale,
                  next.paint.scale, nd);
  transitionFloat(*this, inst, Instance::kSkewX, prev.paint.skewX,
                  next.paint.skewX, nd);
  transitionFloat(*this, inst, Instance::kSkewY, prev.paint.skewY,
                  next.paint.skewY, nd);
  if (next.hasTrim || prev.hasTrim) {
    transitionFloat(*this, inst, Instance::kTrimStart, prev.trimStart,
                    next.trimStart, nd);
    transitionFloat(*this, inst, Instance::kTrimEnd, prev.trimEnd,
                    next.trimEnd, nd);
    transitionFloat(*this, inst, Instance::kTrimOffset, prev.trimOffset,
                    next.trimOffset, nd);
  }
  if (next.glyphFx || prev.glyphFx) {
    static const PropValue<float> kFullProgress = 1.0f;
    transitionFloat(*this, inst, Instance::kGlyphProgress,
                    prev.glyphFx ? prev.glyphFx->progress : kFullProgress,
                    next.glyphFx ? next.glyphFx->progress : kFullProgress,
                    nd);
  }

  // Fill: color→color lerp via a progress output. A next fill with NO
  // transition is a plain snap — disconnect any in-flight lerp so the
  // description lands (the same shadow rule as the float slots).
  bool nextFillTransitions = false;
  if (next.paint.fill) {
    ResolvedProp<Fill> nf = resolveProp(*next.paint.fill, nd);
    nextFillTransitions = !nf.binding && nf.transition != nullptr;
  }
  if (!nextFillTransitions) {
    if (auto &anim = inst.anims[Instance::kFillLerp];
        anim && anim->started) {
      anim->value.disconnect();
      anim->started = false;
    }
  }
  if (prev.paint.fill && next.paint.fill) {
    ResolvedProp<Fill> prevFill = resolveProp(*prev.paint.fill, nd);
    ResolvedProp<Fill> nextFill = resolveProp(*next.paint.fill, nd);
    if (!prevFill.binding && !nextFill.binding && nextFill.transition &&
        prevFill.target.kind == Fill::Kind::Color &&
        nextFill.target.kind == Fill::Kind::Color &&
        !(prevFill.target == nextFill.target)) {
      // Current visual color as the new "from" (retarget-from-current).
      Fill from = prevFill.target;
      auto &anim = inst.anims[Instance::kFillLerp];
      if (anim && anim->started && anim->value.isConnected()) {
        const float t = anim->value.value();
        for (int i = 0; i < 4; ++i)
          from.colorValue.vec()[i] =
              inst.fillFrom.colorValue.vec()[i] +
              (inst.fillTo.colorValue.vec()[i] -
               inst.fillFrom.colorValue.vec()[i]) * t;
      }
      inst.fillFrom = from;
      inst.fillTo = nextFill.target;
      if (!anim)
        anim = std::make_unique<AnimatedFloat>();
      anim->value = 0.0f;
      anim->started = true;
      auto motion = ticker.timeline().apply(&anim->value);
      const float delay =
          std::chrono::duration<float>(nextFill.transition->delay).count();
      if (delay > 0)
        motion.then<choreograph::Hold>(0.0f, delay);
      motion.then<choreograph::RampTo>(
          1.0f,
          std::chrono::duration<float>(nextFill.transition->duration).count(),
          nextFill.transition->ease);
    }
  }
}

} // namespace sigil::compose
