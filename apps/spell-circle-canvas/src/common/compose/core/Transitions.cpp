/** @file
 * Transitions: the lanes a description carries, enumerated by family; the
 * patch that retargets each lane's ramp from its current value and the
 * mount that plays a declared entrance, both through SigilCore's lane
 * operations (one motion per (instance, property), retarget-from-current);
 * the per-frame reads of every animated lane a recording bakes; and the
 * cascade order a stagger deals its units in.
 */

#include <algorithm>
#include <chrono>
#include <numeric>
#include <span>

#include "ComposeRuntime.h"
#include "PaintInternal.h"

namespace sigil::compose {

using namespace detail;

float detail::Instance::resolveFloat(Slot slot,
                                     const Animatable<float>& v) const {
  return motion::resolveFloatAt(anims[slot].get(), v);
}

float detail::Instance::resolveFloatAt(const AnimatedFloat* anim,
                                       const Animatable<float>& v) const {
  return motion::resolveFloatAt(anim, v);
}

namespace {

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

/** Core's run-of-a-family read, over the lane list lanes() filled. */
std::span<const Lane> familyLanes(const std::vector<Lane>& lanes,
                                  LaneFamily family) {
  return motion::familyLanes(std::span<const Lane>(lanes), family);
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

  // staggerChildren()'s carry is the extra lead every entrance on this
  // node holds for, in seconds.
  const float carrySeconds = mountDelayCarryMs / 1000.0f;
  auto entranceAt = [&](std::unique_ptr<AnimatedFloat>& slotAnim,
                        const Animatable<float>& v) {
    motion::mountEntrance(ticker, slotAnim, v, carrySeconds);
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
      motion::progressRamp(ticker, inst.anims[Instance::kFillLerp], tr->spec,
                           mountDelayCarryMs / 1000.0f);  // stagger carry
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
  motion::retargetSlots(ticker,
                        std::span<std::unique_ptr<AnimatedFloat>>(inst.anims),
                        familyLanes(prevLanes, LaneFamily::Slot),
                        familyLanes(nextLanes, LaneFamily::Slot), nd);

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
  for (const LaneFamily family : kPositionalFamilies)
    motion::retargetFamily(ticker, familyAnims(inst, family),
                           familyLanes(prevLanes, family),
                           familyLanes(nextLanes, family), nd);

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
      motion::progressRamp(ticker, anim, *nextFill.transition, 0.0f);
    }
  }
}

// ---------------------------------------------------------------------------
// The lane reads: every animated number a recording can be baked with,
// resolved for this frame by ONE body per lane. The volatility walk, the
// released scan and the paint-side probe all call these, so the three
// compares cannot drift apart.

std::vector<float> detail::Instance::resolveGateValues() const {
  std::vector<float> values;
  const ElementNode& node = *desc;
  if (!node.hasMasks()) return values;
  size_t slot = 0;
  const auto push = [&](const Animatable<float>& v) {
    const AnimatedFloat* a =
        slot < maskAnims.size() ? maskAnims[slot].get() : nullptr;
    values.push_back(resolveFloatAt(a, v));
    ++slot;
  };
  for (const Mask& m : node.fxData->masks) {
    if (m.with.kind == Gate::Kind::Spans)
      for (const Spans::Term& t : m.with.where.terms) {
        push(t.begin);
        push(t.end);
        push(t.offset);
      }
    else if (m.with.kind == Gate::Kind::Edge)
      push(m.with.fraction);
  }
  return values;
}

float detail::Instance::resolvePathAt() const {
  if (!desc || !desc->textData) return 0.0f;
  const std::optional<TextPath>& baseline = desc->textData->onPath;
  if (!baseline) return 0.0f;
  return resolveFloat(kTextPathAt, baseline->at);
}

std::vector<float> detail::Instance::resolveTrackValues() const {
  std::vector<float> values;
  const std::span<const Track> tracks =
      desc->textData ? std::span<const Track>(desc->textData->tracks)
                     : std::span<const Track>();
  values.reserve(tracks.size());
  for (size_t i = 0; i < tracks.size(); ++i) {
    const AnimatedFloat* a =
        i < trackAnims.size() ? trackAnims[i].get() : nullptr;
    values.push_back(resolveFloatAt(a, tracks[i].progress));
  }
  return values;
}

Fill detail::Instance::resolveBoundFill() const {
  const ElementNode& node = *desc;
  if (node.paint.fill)
    if (const choreograph::Output<Fill>* binding = node.paint.fill->binding())
      return binding->value();
  return {};
}

std::array<float, 2> detail::Instance::resolvePatternOffset() const {
  // Only the TOP-LEVEL bound offset of the node's fill material is a
  // scalar-lane input. A nested one (in a blend layer, in a child slot)
  // keeps the material on the opaque live path — see
  // animatedBeyondBoundOffset — and never reaches this lane. All-zero when
  // unbound, matching the ContentScalars guard, so a node without the
  // channel compares equal to itself forever.
  const Material* m = liveMaterialOf(*desc);
  if (!m || !m->hasBoundOffset()) return {};
  const SkPoint pan = m->boundOffsetValue();
  return {pan.x(), pan.y()};
}

}  // namespace sigil::compose
