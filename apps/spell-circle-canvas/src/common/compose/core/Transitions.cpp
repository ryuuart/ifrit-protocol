/** @file
 * Transitions: resolving an animatable float to its current value (binding,
 * running ramp, or plain), and starting/retargeting Choreograph ramps when a
 * reconciled plain-constant change occurs (the SwiftUI implicit-transition
 * lesson: one motion per (instance, property), retarget-from-current).
 */

#include <chrono>
#include <span>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

float detail::Instance::resolveFloat(Slot slot,
                                     const Animatable<float>& v) const {
  if (const choreograph::Output<float>* binding = v.binding()) {
    // A shaped binding (bind(&out).map().to()…) runs its map here — the
    // one place a bound float is read, so trim, glyph progress, every
    // transform and the hit test all get it for free.
    if (const BoundFloat* shape = v.boundMap())
      return shape->apply(binding->value());
    return binding->value();
  }
  if (anims[slot] && anims[slot]->started) return anims[slot]->value.value();
  if (const float* plain = v.plain()) return *plain;
  return v.transitioned()->value;
}

float detail::Instance::resolveFloatAt(const AnimatedFloat* anim,
                                       const Animatable<float>& v) const {
  if (const choreograph::Output<float>* binding = v.binding()) {
    if (const BoundFloat* shape = v.boundMap())
      return shape->apply(binding->value());
    return binding->value();
  }
  if (anim && anim->started) return anim->value.value();
  if (const float* plain = v.plain()) return *plain;
  return v.transitioned()->value;
}

namespace {

/** Starts (or retargets) a ramp held in `slotAnim` when the plain target
 *  changed. Returns true if a motion is running. The slot is passed as
 *  the HELD MOTION rather than as an index because the span endpoints'
 *  count is a property of the description, not of the kernel — one body,
 *  two storages (the fixed property array and the span vector). */
bool transitionFloatAt(Composer::Impl& impl, Instance& inst,
                       std::unique_ptr<AnimatedFloat>& slotAnim,
                       const Animatable<float>& prevValue,
                       const Animatable<float>& nextValue,
                       const std::optional<Transition>& nodeDefault) {
  ResolvedProp<float> prev = resolveProp(prevValue, nodeDefault);
  ResolvedProp<float> next = resolveProp(nextValue, nodeDefault);
  // Snap semantics must actually LAND: a lingering ramp from an earlier
  // transition would shadow the plain description forever (resolveFloat
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
  auto motion = impl.ticker.timeline().apply(&anim->value);
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

/** The fixed-property spelling of the same operation. */
bool transitionFloat(Composer::Impl& impl, Instance& inst, Instance::Slot slot,
                     const Animatable<float>& prevValue,
                     const Animatable<float>& nextValue,
                     const std::optional<Transition>& nodeDefault) {
  return transitionFloatAt(impl, inst, inst.anims[slot], prevValue, nextValue,
                           nodeDefault);
}

/** The vector holding a positional family's motions on the instance. */
std::vector<std::unique_ptr<AnimatedFloat>>& familyAnims(Instance& inst,
                                                         LaneFamily family) {
  switch (family) {
    case LaneFamily::Span:
      return inst.spanAnims;
    case LaneFamily::Gate:
      return inst.maskAnims;
    case LaneFamily::Track:
      return inst.trackAnims;
    case LaneFamily::Slot:
      break;
  }
  SkASSERT(false);  // a Slot lane lives in the fixed array, not a vector
  return inst.trackAnims;
}

/** The contiguous run of @p family's lanes in a list lanes() filled. */
std::span<const Lane> familyLanes(const std::vector<Lane>& lanes,
                                  LaneFamily family) {
  size_t begin = 0;
  while (begin < lanes.size() && lanes[begin].slot.family != family) ++begin;
  size_t end = begin;
  while (end < lanes.size() && lanes[end].slot.family == family) ++end;
  return std::span<const Lane>(lanes).subspan(begin, end - begin);
}

constexpr LaneFamily kPositionalFamilies[] = {
    LaneFamily::Span, LaneFamily::Gate, LaneFamily::Track};

}  // namespace

void Composer::Impl::lanes(const ElementNode& node, std::vector<Lane>& out) {
  out.clear();
  // Every slot the table can reach (kSlotSpecs, ComposeRuntime.h — the one
  // enumeration of Instance::Slot), one lane per row whether or not this
  // node carries the field: a Bespoke row answers nullptr and so does a
  // node without the block that holds the slot.
  for (const SlotSpec& spec : kSlotSpecs)
    out.push_back({slotValueOf(spec, node),
                   {LaneFamily::Slot, (size_t)spec.slot},
                   spec.standing});
  // Every animatable span endpoint of a node's stroke passes, in
  // declaration order — the order Instance::spanAnims is indexed by, and
  // the order SpanInput::values arrives in.
  if (node.strokeData) {
    size_t i = 0;
    for (const StrokePass& pass : node.strokeData->passes)
      for (const Spans::Term& term : pass.where.terms) {
        out.push_back({&term.begin, {LaneFamily::Span, i++}, 0.0f});
        out.push_back({&term.end, {LaneFamily::Span, i++}, 0.0f});
        out.push_back({&term.offset, {LaneFamily::Span, i++}, 0.0f});
      }
  }
  // Every animatable number a node's MASK GATES carry, in declaration order
  // — the order Instance::maskAnims is indexed by. Three per Spans term,
  // one per Edge fraction, none for Shape or Alpha.
  //
  // SEPARATE PER MASK, which is the point: three masks running at three
  // rates each own their slots, so `animate(to(x))` on the second retargets
  // the second and nothing else.
  if (node.fxData) {
    size_t i = 0;
    for (const Mask& m : node.fxData->masks) {
      if (m.with.kind == Gate::Kind::Spans)
        for (const Spans::Term& term : m.with.where.terms) {
          out.push_back({&term.begin, {LaneFamily::Gate, i++}, 0.0f});
          out.push_back({&term.end, {LaneFamily::Gate, i++}, 0.0f});
          out.push_back({&term.offset, {LaneFamily::Gate, i++}, 0.0f});
        }
      else if (m.with.kind == Gate::Kind::Edge)
        out.push_back({&m.with.fraction, {LaneFamily::Gate, i++}, 0.0f});
    }
  }
  // Every fx() TRACK's master progress, in declaration order — the order
  // Instance::trackAnims is indexed by.
  //
  // SEPARATE PER TRACK, which is the point: a rise and a loop on one text
  // node run at their own rates, so `animate(to(1))` on the second retargets
  // the second and leaves the first alone.
  if (node.textData) {
    size_t i = 0;
    for (const Track& t : node.textData->tracks)
      out.push_back({&t.progress, {LaneFamily::Track, i++}, 0.0f});
  }
}

std::vector<Lane> Composer::Impl::lanes(const ElementNode& node) {
  std::vector<Lane> out;
  lanes(node, out);
  return out;
}

/** Mount entrances: an animate(from(a).to(b)) value plays `from → value` when
 * the node FIRST appears (there is no prev to diff against — this is the "prev"
 * the author declared). Skipped for snapshot()/measure() (liveOnly: no live
 *  timeline — bakes render the settled value). */
void Composer::Impl::applyMountTransitions(Instance& inst,
                                           const ElementNode& node) {
  if (liveOnly) return;

  auto entranceAt = [&](std::unique_ptr<AnimatedFloat>& slotAnim,
                        const Animatable<float>& v) {
    const Transitioned<float>* tr = v.transitioned();
    if (!tr) return;
    // animate(through({…})): the multi-segment mount path — checked BEFORE the
    // from==value guard (a shake 0→−20→0 starts and ends equal).
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
          mountDelayCarryMs / 1000.0f +
          std::chrono::duration<float>(tr->waypoints.front().first).count();
      if (lead > 0) motion.then<choreograph::Hold>(first, lead);
      for (size_t i = 1; i < tr->waypoints.size(); ++i) {
        const float seg =
            std::chrono::duration<float>(tr->waypoints[i].first -
                                         tr->waypoints[i - 1].first)
                .count();
        motion.then<choreograph::RampTo>(
            tr->waypoints[i].second, std::max(seg, 0.0f), tr->spec.easing());
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
                        mountDelayCarryMs / 1000.0f;  // staggerChildren() carry
    if (delay > 0)  // stagger: hold the `from` before entering
      motion.then<choreograph::Hold>(*tr->from, delay);
    motion.then<choreograph::RampTo>(
        tr->value, std::chrono::duration<float>(tr->spec.duration).count(),
        tr->spec.easing());
  };
  // Every lane the node carries. A mount entrance asks nothing of a slot's
  // ROLE: the description either declared a `from` or it did not.
  // The positional families are entrances like any other —
  //   span reveals: `.stroke(spans::upTo(animate(...)), brush)` — the
  //   reveal is a property of the PASS, so its motions live in a
  //   per-description vector rather than a slot;
  //   mask gates: `.mask(by::spans(spans::upTo(animate(...))))` and
  //   `.mask(by::edge(90, animate(...)))`;
  //   fx() tracks: `.fx({.progress = animate(...)})`, each track owning its
  //   slot.
  // Each family's vector is sized to the description before its lanes run.
  static thread_local std::vector<Lane> nodeLanes;
  lanes(node, nodeLanes);
  for (const Lane& lane : familyLanes(nodeLanes, LaneFamily::Slot))
    if (lane.value) entranceAt(inst.anims[lane.slot.index], *lane.value);
  for (const LaneFamily family : kPositionalFamilies) {
    const std::span<const Lane> members = familyLanes(nodeLanes, family);
    std::vector<std::unique_ptr<AnimatedFloat>>& anims =
        familyAnims(inst, family);
    anims.resize(members.size());
    for (size_t i = 0; i < members.size(); ++i)
      entranceAt(anims[i], *members[i].value);
  }

  // The kFillLerp row (SlotRole::Bespoke): from → to through a synthesized
  // 0→1 progress, because the description holds an Animatable<Fill> and no
  // float for the table to point at.
  if (node.paint.fill) {
    const Transitioned<Fill>* tr = node.paint.fill->transitioned();
    if (tr && tr->from && tr->from->kind == Fill::Kind::Color &&
        tr->value.kind == Fill::Kind::Color && !(*tr->from == tr->value)) {
      inst.fillFrom = *tr->from;
      inst.fillTo = tr->value;
      auto& anim = inst.anims[Instance::kFillLerp];
      if (!anim) anim = std::make_unique<AnimatedFloat>();
      anim->value = 0.0f;
      anim->started = true;
      auto motion = ticker.timeline().apply(&anim->value);
      const float delay =
          std::chrono::duration<float>(tr->spec.delay).count() +
          mountDelayCarryMs / 1000.0f;  // staggerChildren() carry
      if (delay > 0) motion.then<choreograph::Hold>(0.0f, delay);
      motion.then<choreograph::RampTo>(
          1.0f, std::chrono::duration<float>(tr->spec.duration).count(),
          tr->spec.easing());
    }
  }
}

void Composer::Impl::applyTransitions(Instance& inst, const ElementNode& prev,
                                      const ElementNode& next) {
  const auto& nd = next.nodeTransition;
  // Every slot the table can reach (kSlotSpecs, ComposeRuntime.h — the one
  // enumeration of Instance::Slot). A patch asks nothing of a slot's ROLE
  // either; what it needs is the pair of endpoints, and the ONE extra fact
  // the table carries for it: a node that GAINS or LOSES the block holding
  // a slot (a `travel()` path, kinetic text) has no previous or next value
  // there, so the field's own default stands in as the endpoint. That is
  // the same "positional list" rule the span endpoints below use.
  static thread_local std::vector<Lane> prevLanes, nextLanes;
  lanes(prev, prevLanes);
  lanes(next, nextLanes);
  {
    const std::span<const Lane> p = familyLanes(prevLanes, LaneFamily::Slot);
    const std::span<const Lane> n = familyLanes(nextLanes, LaneFamily::Slot);
    for (size_t i = 0; i < n.size(); ++i) {
      if (!p[i].value && !n[i].value)
        continue;  // neither description carries it: nothing to ramp
      const Animatable<float> standing = n[i].standing;
      transitionFloat(*this, inst, (Instance::Slot)n[i].slot.index,
                      p[i].value ? *p[i].value : standing,
                      n[i].value ? *n[i].value : standing, nd);
    }
  }

  // The positional families, each by the same rule. The lane list is
  // positional, so a description that changes the SHAPE of a family (a
  // pass added, a term added, a mask or a track added or removed) drops the
  // running motions rather than carrying them onto endpoints that now mean
  // something else — the same rule keys enforce for whole nodes.
  //
  // Span reveals: settled values show through (resolveFloatAt falls back to
  // the description); an ENTRANCE is a mount thing, and this node is not
  // mounting.
  //
  // Mask gates: this is what makes the retarget case work. An element that
  // writes ONE mask in both branches of an if/else keeps a stable slot
  // index, so `animate(to(span))` ramps from wherever the gate is now
  // instead of mounting from scratch. Write two masks in one branch and one
  // in the other and the shape changed — the motions drop, deliberately,
  // rather than carrying onto a number that now means something else.
  //
  // fx() tracks: an element that writes the same NUMBER of tracks in both
  // branches of an if/else keeps stable slot indices, so `animate(to(1))`
  // on the second track ramps from wherever that track's progress is now.
  // Add or remove a track and the shape changed — the motions drop rather
  // than carrying onto a progress that now drives a different effect.
  for (const LaneFamily family : kPositionalFamilies) {
    const std::span<const Lane> p = familyLanes(prevLanes, family);
    const std::span<const Lane> n = familyLanes(nextLanes, family);
    std::vector<std::unique_ptr<AnimatedFloat>>& anims =
        familyAnims(inst, family);
    if (p.size() != n.size()) {
      anims.clear();
      anims.resize(n.size());
    } else {
      // applyMountTransitions sizes this vector — and it RETURNS EARLY on a
      // liveOnly composer (snapshot/measure), so a patch is the first thing
      // to touch it there. Size it here too rather than indexing an empty
      // vector.
      if (anims.size() != n.size()) anims.resize(n.size());
      for (size_t i = 0; i < n.size(); ++i)
        transitionFloatAt(*this, inst, anims[i], *p[i].value, *n[i].value, nd);
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
    if (auto& anim = inst.anims[Instance::kFillLerp]; anim && anim->started) {
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
      auto& anim = inst.anims[Instance::kFillLerp];
      if (anim && anim->started && anim->value.isConnected()) {
        const float t = anim->value.value();
        for (int i = 0; i < 4; ++i)
          from.colorValue.vec()[i] = inst.fillFrom.colorValue.vec()[i] +
                                     (inst.fillTo.colorValue.vec()[i] -
                                      inst.fillFrom.colorValue.vec()[i]) *
                                         t;
      }
      inst.fillFrom = std::move(from);
      inst.fillTo = nextFill.target;
      if (!anim) anim = std::make_unique<AnimatedFloat>();
      anim->value = 0.0f;
      anim->started = true;
      auto motion = ticker.timeline().apply(&anim->value);
      const float delay =
          std::chrono::duration<float>(nextFill.transition->delay).count();
      if (delay > 0) motion.then<choreograph::Hold>(0.0f, delay);
      motion.then<choreograph::RampTo>(
          1.0f,
          std::chrono::duration<float>(nextFill.transition->duration).count(),
          nextFill.transition->easing());
    }
  }
}

}  // namespace sigil::compose
