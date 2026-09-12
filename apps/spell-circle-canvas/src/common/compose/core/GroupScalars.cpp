/** @file
 * The scalars a `Cache::Group` subtree exposes to its value memo: every
 * live number under the root, in tree order, which is the whole of what
 * says a settled group has stopped being settled.
 */

#include <vector>

#include "ComposeRuntime.h"
#include "PaintInternal.h"

namespace sigil::compose {

using namespace detail;

/** Every animated scalar under a `Cache::Group` root, in tree order.
 *
 *  This is the whole invalidation mechanism, and therefore the whole risk.
 *  What it gathers is the set of numbers that can change what the bake looks
 *  like WITHOUT changing any description — which is exactly the set
 *  `computeVolatile` calls volatility and refuses to cache across. Two rules
 *  keep it honest:
 *
 *   - **Only LIVE slots are pushed.** A plain or settled value cannot move
 *     without a patch, and a patch calls markPaintDirtyUp() on the group.
 *     So the vector's LENGTH is part of the comparison: a motion connecting
 *     or disconnecting changes it, and the group re-bakes.
 *   - **The root's own transform and opacity are excluded.** They are
 *     applied by paint()'s matrix and saveLayer, outside the bake, and a
 *     fading group would otherwise drop its bake on every frame of the fade
 *     for a change the bake does not contain. (Its own transform moving is
 *     handled separately and more strictly — a device-pinned bake is refused
 *     outright while the node moves.) The root's CONTENT scalars are inside
 *     paintContent and are gathered like everyone else's.
 *
 *  Cost is one traversal of the subtree per frame, reading a handful of
 *  floats per node — set against the entire paint of that subtree, which is
 *  what it decides whether to skip. */
void collectGroupScalars(const Instance& inst, bool root,
                         std::vector<float>& out) {
  const ElementNode& node = *inst.description;
  const auto push = [&](Instance::Slot slot,
                        const motion::Animatable<float>& v) {
    if (motion::isLive(inst.anims[slot].get(), v))
      out.push_back(inst.resolveFloat(slot, v));
  };
  // Every slot the table can reach, in enum order (kSlotSpecs,
  // ComposeRuntime.h — the one enumeration of Instance::Slot). The root's
  // own transform and opacity are the exclusion argued above; its CONTENT
  // scalars are inside paintContent and are gathered like everyone else's.
  //
  // THE ORDER OF THIS VECTOR IS ARBITRARY BUT MUST BE STABLE. It is only
  // ever compared against the vector this same function produced on the
  // previous frame (Impl::paint, `groupScratch == inst.groupPrev`), so any
  // fixed permutation of the gathered values computes the identical
  // verdict; what would break the memo is an order that varies between
  // frames for the same tree.
  for (const SlotSpec& spec : kSlotSpecs) {
    // The root's own transform and opacity are outside its bake; its
    // content scalars and the VIEW it declares for its children (a
    // Projection lane) are inside it, since the children are.
    if (root &&
        (spec.role == SlotRole::Opacity || spec.role == SlotRole::Geometric))
      continue;
    if (const motion::Animatable<float>* v = slotValueOf(spec, node))
      push(spec.slot, *v);
  }
  // Mask gates: the same argument, over the per-mask vector. Only LIVE
  // values are pushed, so the vector's LENGTH still carries a motion
  // connecting or disconnecting.
  if (node.hasMasks()) {
    size_t slot = 0;
    for (const Mask& m : node.fxData->masks) {
      const auto pushGate = [&](const motion::Animatable<float>& v) {
        const AnimatedFloat* a =
            slot < inst.maskAnims.size() ? inst.maskAnims[slot].get() : nullptr;
        if (motion::isLive(a, v)) out.push_back(inst.resolveFloatAt(a, v));
        ++slot;
      };
      if (m.with.kind == Gate::Kind::Spans)
        for (const Spans::Term& t : m.with.where.terms) {
          pushGate(t.begin);
          pushGate(t.end);
          pushGate(t.offset);
        }
      else if (m.with.kind == Gate::Kind::Edge)
        pushGate(m.with.fraction);
    }
  }
  // fx() track progresses: the same argument again, over the per-track
  // vector. Only LIVE values are pushed, so the vector's LENGTH still
  // carries a track's motion connecting or disconnecting.
  if (node.textData)
    for (size_t i = 0; i < node.textData->tracks.size(); ++i) {
      const motion::Animatable<float>& v = node.textData->tracks[i].progress;
      const AnimatedFloat* a =
          i < inst.trackAnims.size() ? inst.trackAnims[i].get() : nullptr;
      if (motion::isLive(a, v)) out.push_back(inst.resolveFloatAt(a, v));
    }
  // The kFillLerp row (SlotRole::Bespoke): a synthesized progress with no
  // Animatable in the description, so it is read straight off the motion.
  if (inst.anims[Instance::kFillLerp] &&
      inst.anims[Instance::kFillLerp]->value.isConnected())
    out.push_back(inst.anims[Instance::kFillLerp]->value.value());
  // The kInkLerp row, the same way: the progress of an ink easing on this
  // node, which every text and mark under it repaints with.
  if (inst.anims[Instance::kInkLerp] &&
      inst.anims[Instance::kInkLerp]->value.isConnected())
    out.push_back(inst.anims[Instance::kInkLerp]->value.value());
  for (const auto& child : inst.children)
    collectGroupScalars(*child, false, out);
}

}  // namespace sigil::compose
