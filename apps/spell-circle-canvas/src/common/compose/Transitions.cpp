// Transitions: resolving an animatable float to its current value (binding,
// running ramp, or plain), and starting/retargeting Choreograph ramps when a
// reconciled plain-constant change occurs (the SwiftUI implicit-transition
// lesson: one motion per (instance, property), retarget-from-current).

#include "ComposeRuntime.h"

#include <chrono>

namespace sigil::compose {

using namespace detail;

float detail::Instance::resolveFloat(Slot slot,
                                     const Animatable<float> &v) const {
  if (const choreograph::Output<float> *binding = v.binding()) {
    // A shaped binding (bind(&out).map().to()…) runs its map here — the
    // one place a bound float is read, so trim, glyph progress, every
    // transform and the hit test all get it for free.
    if (const BoundFloat *shape = v.boundMap())
      return shape->apply(binding->value());
    return binding->value();
  }
  if (anims[slot] && anims[slot]->started)
    return anims[slot]->value.value();
  if (const float *plain = v.plain())
    return *plain;
  return v.transitioned()->value;
}

float detail::Instance::resolveFloatAt(const AnimatedFloat *anim,
                                       const Animatable<float> &v) const {
  if (const choreograph::Output<float> *binding = v.binding()) {
    if (const BoundFloat *shape = v.boundMap())
      return shape->apply(binding->value());
    return binding->value();
  }
  if (anim && anim->started)
    return anim->value.value();
  if (const float *plain = v.plain())
    return *plain;
  return v.transitioned()->value;
}

namespace {

/** Starts (or retargets) a ramp held in `slotAnim` when the plain target
 *  changed. Returns true if a motion is running. The slot is passed as
 *  the HELD MOTION rather than as an index because the span endpoints'
 *  count is a property of the description, not of the kernel — one body,
 *  two storages (the fixed property array and the span vector). */
bool transitionFloatAt(Composer::Impl &impl, Instance &inst,
                       std::unique_ptr<AnimatedFloat> &slotAnim,
                       const Animatable<float> &prevValue,
                       const Animatable<float> &nextValue,
                       const std::optional<Transition> &nodeDefault) {
  ResolvedProp<float> prev = resolveProp(prevValue, nodeDefault);
  ResolvedProp<float> next = resolveProp(nextValue, nodeDefault);
  // Snap semantics must actually LAND: a lingering ramp from an earlier
  // transition would shadow the plain description forever (resolveFloat
  // prefers a started anim), so the snap paths disconnect it.
  auto snapAnim = [&] {
    if (auto &anim = slotAnim; anim && anim->started) {
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

  auto &anim = slotAnim;
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
      next.transition->easing());
  return true;
}

/** The fixed-property spelling of the same operation. */
bool transitionFloat(Composer::Impl &impl, Instance &inst, Instance::Slot slot,
                     const Animatable<float> &prevValue,
                     const Animatable<float> &nextValue,
                     const std::optional<Transition> &nodeDefault) {
  return transitionFloatAt(impl, inst, inst.anims[slot], prevValue, nextValue,
                           nodeDefault);
}

/** Every animatable span endpoint of a node's stroke passes, in
 *  declaration order — the order Instance::spanAnims is indexed by, and
 *  the order SpanInput::values arrives in. */
std::vector<const Animatable<float> *> spanEndpoints(const ElementNode &node) {
  std::vector<const Animatable<float> *> out;
  if (!node.strokeData)
    return out;
  for (const StrokePass &pass : node.strokeData->passes)
    for (const Spans::Term &term : pass.where.terms) {
      out.push_back(&term.begin);
      out.push_back(&term.end);
      out.push_back(&term.offset);
    }
  return out;
}

/** Every animatable number a node's MASK GATES carry, in declaration order
 *  — the order Instance::maskAnims is indexed by. Three per Spans term,
 *  one per Edge fraction, none for Shape or Alpha.
 *
 *  SEPARATE PER MASK, which is the point: three masks running at three
 *  rates each own their slots, so `animate(to(x))` on the second retargets
 *  the second and nothing else. */
std::vector<const Animatable<float> *> gateEndpoints(const ElementNode &node) {
  std::vector<const Animatable<float> *> out;
  if (!node.fxData)
    return out;
  for (const Mask &m : node.fxData->masks) {
    if (m.with.kind == Gate::Kind::Spans)
      for (const Spans::Term &term : m.with.where.terms) {
        out.push_back(&term.begin);
        out.push_back(&term.end);
        out.push_back(&term.offset);
      }
    else if (m.with.kind == Gate::Kind::Edge)
      out.push_back(&m.with.fraction);
  }
  return out;
}

} // namespace

/** Mount entrances: an animate(from(a).to(b)) value plays `from → value` when the node
 *  FIRST appears (there is no prev to diff against — this is the "prev" the
 *  author declared). Skipped for snapshot()/measure() (liveOnly: no live
 *  timeline — bakes render the settled value). */
void Composer::Impl::applyMountTransitions(Instance &inst,
                                           const ElementNode &node) {
  if (liveOnly)
    return;

  auto entranceAt = [&](std::unique_ptr<AnimatedFloat> &slotAnim,
                        const Animatable<float> &v) {
    const Transitioned<float> *tr = v.transitioned();
    if (!tr)
      return;
    // animate(through({…})): the multi-segment mount path — checked BEFORE the
    // from==value guard (a shake 0→−20→0 starts and ends equal).
    if (tr->waypoints.size() >= 2) {
      auto &anim = slotAnim;
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
                                         std::max(seg, 0.0f), tr->spec.easing());
      }
      return;
    }
    if (!tr->from || *tr->from == tr->value)
      return;
    auto &anim = slotAnim;
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
        tr->spec.easing());
  };
  // Every slot the table can reach (kSlotSpecs, ComposeRuntime.h — the one
  // enumeration of Instance::Slot). A mount entrance asks nothing of a
  // slot's ROLE: the description either declared a `from` or it did not.
  for (const SlotSpec &spec : kSlotSpecs)
    if (const Animatable<float> *v = slotValueOf(spec, node))
      entranceAt(inst.anims[spec.slot], *v);
  // Span reveals: `.stroke(spans::upTo(animate(...)), brush)` is a mount
  // entrance like any other — the reveal is a property of the PASS, so
  // its motions live in a per-description vector rather than a slot.
  {
    const std::vector<const Animatable<float> *> ends = spanEndpoints(node);
    inst.spanAnims.resize(ends.size());
    for (size_t i = 0; i < ends.size(); ++i)
      entranceAt(inst.spanAnims[i], *ends[i]);
  }
  // Mask gates: `.mask(by::spans(spans::upTo(animate(...))))` and
  // `.mask(by::edge(90, animate(...)))` are mount entrances like any other.
  {
    const std::vector<const Animatable<float> *> gates = gateEndpoints(node);
    inst.maskAnims.resize(gates.size());
    for (size_t i = 0; i < gates.size(); ++i)
      entranceAt(inst.maskAnims[i], *gates[i]);
  }

  // The kFillLerp row (SlotRole::Bespoke): from → to through a synthesized
  // 0→1 progress, because the description holds an Animatable<Fill> and no
  // float for the table to point at.
  if (node.paint.fill) {
    const Transitioned<Fill> *tr =
        node.paint.fill->transitioned();
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
          tr->spec.easing());
    }
  }
}

void Composer::Impl::applyTransitions(Instance &inst, const ElementNode &prev,
                                      const ElementNode &next) {
  const auto &nd = next.nodeTransition;
  // Every slot the table can reach (kSlotSpecs, ComposeRuntime.h — the one
  // enumeration of Instance::Slot). A patch asks nothing of a slot's ROLE
  // either; what it needs is the pair of endpoints, and the ONE extra fact
  // the table carries for it: a node that GAINS or LOSES the block holding
  // a slot (a `travel()` path, kinetic text) has no previous or next value
  // there, so the field's own default stands in as the endpoint. That is
  // the same "positional list" rule the span endpoints below use.
  for (const SlotSpec &spec : kSlotSpecs) {
    const Animatable<float> *p = slotValueOf(spec, prev);
    const Animatable<float> *n = slotValueOf(spec, next);
    if (!p && !n)
      continue; // neither description carries it: nothing to ramp
    const Animatable<float> standing = spec.standing;
    transitionFloat(*this, inst, spec.slot, p ? *p : standing,
                    n ? *n : standing, nd);
  }

  // Span reveals. The endpoint list is positional, so a description that
  // changes the SHAPE of its pass list (a pass added, a term added) drops
  // the running motions rather than carrying them onto endpoints that now
  // mean something else — the same rule keys enforce for whole nodes.
  {
    const std::vector<const Animatable<float> *> prevEnds = spanEndpoints(prev);
    const std::vector<const Animatable<float> *> nextEnds = spanEndpoints(next);
    if (prevEnds.size() != nextEnds.size()) {
      inst.spanAnims.clear();
      inst.spanAnims.resize(nextEnds.size());
      // Settled values show through (resolveFloatAt falls back to the
      // description); an ENTRANCE is a mount thing, and this node is not
      // mounting.
    } else {
      // applyMountTransitions sizes this vector — and it RETURNS EARLY on a
      // liveOnly composer (snapshot/measure), so a patch is the first thing
      // to touch it there. Size it here too rather than indexing an empty
      // vector.
      if (inst.spanAnims.size() != nextEnds.size())
        inst.spanAnims.resize(nextEnds.size());
      for (size_t i = 0; i < nextEnds.size(); ++i)
        transitionFloatAt(*this, inst, inst.spanAnims[i], *prevEnds[i],
                          *nextEnds[i], nd);
    }
  }

  // Mask gates, by the same rule and for the same reason. This is what
  // makes the retarget case work: an element that writes ONE mask in both
  // branches of an if/else keeps a stable slot index, so
  // `animate(to(span))` ramps from wherever the gate is now instead of
  // mounting from scratch. Write two masks in one branch and one in the
  // other and the shape changed — the motions drop, deliberately, rather
  // than carrying onto a number that now means something else.
  {
    const std::vector<const Animatable<float> *> prevGates =
        gateEndpoints(prev);
    const std::vector<const Animatable<float> *> nextGates =
        gateEndpoints(next);
    if (prevGates.size() != nextGates.size()) {
      inst.maskAnims.clear();
      inst.maskAnims.resize(nextGates.size());
    } else {
      if (inst.maskAnims.size() != nextGates.size())
        inst.maskAnims.resize(nextGates.size());
      for (size_t i = 0; i < nextGates.size(); ++i)
        transitionFloatAt(*this, inst, inst.maskAnims[i], *prevGates[i],
                          *nextGates[i], nd);
    }
  }

  // The kFillLerp row (SlotRole::Bespoke): color→color lerp via a
  // synthesized progress output. A next fill with NO transition is a plain
  // snap — disconnect any in-flight lerp so the description lands (the same
  // shadow rule as the float slots).
  bool nextFillTransitions = false;
  if (next.paint.fill) {
    ResolvedProp<Fill> nf = resolveProp(*next.paint.fill, nd);
    // Only a COLOR target can continue a color lerp: a shader/none fill
    // with a transition must still disconnect the running lerp, or the
    // node keeps painting a color no description contains until the old
    // motion self-expires (then pops).
    nextFillTransitions = !nf.binding && nf.transition != nullptr &&
                          nf.target.kind == Fill::Kind::Color;
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
          nextFill.transition->easing());
    }
  }
}

} // namespace sigil::compose
