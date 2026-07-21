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
  const float current =
      anim && anim->started ? anim->value.value() : prev.target;
  if (current == next.target)
    return anim && anim->value.isConnected();

  if (!anim)
    anim = std::make_unique<AnimatedFloat>();
  anim->value = current; // seed the retarget start point
  anim->started = true;
  impl.ticker.timeline()
      .apply(&anim->value)
      .then<choreograph::RampTo>(
          next.target,
          std::chrono::duration<float>(next.transition->duration).count(),
          next.transition->ease);
  return true;
}

} // namespace

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
      ticker.timeline()
          .apply(&anim->value)
          .then<choreograph::RampTo>(
              1.0f,
              std::chrono::duration<float>(nextFill.transition->duration)
                  .count(),
              nextFill.transition->ease);
    }
  }
}

} // namespace sigil::compose
