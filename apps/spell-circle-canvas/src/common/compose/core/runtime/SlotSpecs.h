#pragma once

/** @file
 * Internal to the kernel — the slot table: one row per Instance::Slot,
 * naming the property it carries, its role and how it is read, so that a
 * slot added to the enum without a row is a build failure.
 */

#include <iterator>

#include "Instance.h"

namespace sigil::compose::detail {

// Each fixed animation slot has one index-aligned row. Transitions,
// volatility and group scalar collection share this enumeration so a new
// slot cannot be omitted by one of them. The accessor locates the declared
// animatable; the role describes its effect on painting and cache validity.
// A synthesized progress slot has no float declaration to read and must
// carry a written reason for its bespoke handling.

/** How a slot's motion affects painting and cache validity. */
enum class SlotRole : uint8_t {
  /** Applied by paint()'s saveLayer, OUTSIDE the node's content: a fading
   *  node replays its picture, and does not move its device rect. */
  Opacity,
  /** Applied by paint()'s matrix. Moves the device rect, so a device-space
   *  bake is refused while it runs (`Instance::transformLive`). */
  Geometric,
  /** Rebuilds what the node RECORDS, so its own picture is invalidated. */
  Content,
  /** Applied by the CHILDREN's matrices — the view this node declares for
   *  the planes under it. It moves the device rect of every child that
   *  projects, and the node's own bakes contain those children, so it is
   *  refused like a moving transform; unlike one, it is INSIDE a group
   *  root's bake rather than outside it, so a group root gathers it where
   *  it skips its own transform. The node's own recording is guarded by
   *  the children it projects, which are moving whenever it is. */
  Projection,
  /** No `Animatable<float>` in the description AT ALL, so the table cannot
   *  reach it and every consumer keeps its own handling. Costs a written
   *  reason in `bespoke`, which the assert below enforces. */
  Bespoke,
};

/** One row per `Instance::Slot`, index-aligned with the enum. */
struct SlotSpec {
  Instance::Slot slot;
  SlotRole role;
  /** This slot's animatable on this node, or nullptr when the node does
   *  not carry the block that holds it (a node with no `travel()` has no
   *  `t`). A PROPERTY lane is read off the computed style, a positional
   *  one off the block of the description that holds it — which is why the
   *  row is handed both. Null for a Bespoke row — call it through
   *  slotValueOf(), which answers nullptr for those. */
  const motion::Animatable<float>* (*of)(StyledNode);
  /** The standing endpoint a PATCH ramps from or to when `of` answers
   *  nullptr on one side of the diff — a node that GAINS or LOSES the block
   *  has no previous/next value, so the field's OWN DEFAULT is the
   *  endpoint. Must match the declared field's default; unused by a slot
   *  whose `of` never answers nullptr. */
  float standing;
  /** Why this slot is out of the table's reach. Non-null IFF the role is
   *  Bespoke — asserted below, so the escape hatch cannot be taken blank. */
  const char* bespoke;
};

inline constexpr SlotSpec kSlotSpecs[] = {
    {Instance::kOpacity, SlotRole::Opacity,
     [](StyledNode s) { return &s.style.paint.opacity; }, 1.0f, nullptr},
    {Instance::kTx, SlotRole::Geometric,
     [](StyledNode s) { return &s.style.paint.translateX; }, 0.0f, nullptr},
    {Instance::kTy, SlotRole::Geometric,
     [](StyledNode s) { return &s.style.paint.translateY; }, 0.0f, nullptr},
    {Instance::kRotate, SlotRole::Geometric,
     [](StyledNode s) { return &s.style.paint.rotate; }, 0.0f, nullptr},
    {Instance::kScale, SlotRole::Geometric,
     [](StyledNode s) { return &s.style.paint.scale; }, 1.0f, nullptr},
    // kFillLerp is a 0→1 PROGRESS the composer synthesizes for a
    // colour→colour `animate()`; the description holds an
    // `Animatable<Fill>` and no float anywhere, so there is nothing for
    // `of` to return, so its consumers handle the progress separately.
    {Instance::kFillLerp, SlotRole::Bespoke, nullptr, 0.0f,
     "a progress scalar over paint.fill's Tween<Fill> — there is no "
     "Animatable<float> in the description to point at"},
    {Instance::kSkewX, SlotRole::Geometric,
     [](StyledNode s) { return &s.style.paint.skewX; }, 0.0f, nullptr},
    {Instance::kSkewY, SlotRole::Geometric,
     [](StyledNode s) { return &s.style.paint.skewY; }, 0.0f, nullptr},
    {Instance::kScaleX, SlotRole::Geometric,
     [](StyledNode s) { return &s.style.paint.scaleX; }, 1.0f, nullptr},
    {Instance::kScaleY, SlotRole::Geometric,
     [](StyledNode s) { return &s.style.paint.scaleY; }, 1.0f, nullptr},
    // travel(): the `t` lane moves the node exactly as tx/ty do, so it is
    // the GEOMETRIC half and a device-space bake is refused while it runs.
    {Instance::kMotionT, SlotRole::Geometric,
     [](StyledNode s) -> const motion::Animatable<float>* {
       return s.node.motionData ? &s.node.motionData->t : nullptr;
     },
     0.0f, nullptr},
    // textOnPath(): `at` is WHERE ALONG the baseline the run sits, so moving it
    // re-places every glyph INSIDE the node's own box and leaves the box
    // where it was. That is the CONTENT half, not the geometric one — the
    // recording is rebuilt, the device rect is not — and the resolved value
    // joins ContentScalars so a marquee that stops running releases like any
    // other settled scalar.
    {Instance::kTextPathAt, SlotRole::Content,
     [](StyledNode s) -> const motion::Animatable<float>* {
       return s.node.textData && s.node.textData->onPath
                  ? &s.node.textData->onPath->at
                  : nullptr;
     },
     0.0f, nullptr},
    // The depth lanes (DepthData). The four that turn or move THIS plane
    // are geometric exactly as rotate and translateX are: the plane's
    // projected rect moves with them. A node with no depth block answers
    // nullptr, so a plane that never turns costs the four consumers nothing.
    {Instance::kRotateX, SlotRole::Geometric,
     [](StyledNode s) -> const motion::Animatable<float>* {
       return s.node.depthData ? &s.node.depthData->rotateX : nullptr;
     },
     0.0f, nullptr},
    {Instance::kRotateY, SlotRole::Geometric,
     [](StyledNode s) -> const motion::Animatable<float>* {
       return s.node.depthData ? &s.node.depthData->rotateY : nullptr;
     },
     0.0f, nullptr},
    {Instance::kTranslateZ, SlotRole::Geometric,
     [](StyledNode s) -> const motion::Animatable<float>* {
       return s.node.depthData ? &s.node.depthData->translateZ : nullptr;
     },
     0.0f, nullptr},
    {Instance::kScaleZ, SlotRole::Geometric,
     [](StyledNode s) -> const motion::Animatable<float>* {
       return s.node.depthData ? &s.node.depthData->scaleZ : nullptr;
     },
     1.0f, nullptr},
    // perspective(): the view the CHILDREN are seen through — the one
    // lane whose motion lands on other nodes' device rects.
    {Instance::kPerspective, SlotRole::Projection,
     [](StyledNode s) -> const motion::Animatable<float>* {
       return s.node.depthData ? &s.node.depthData->perspective : nullptr;
     },
     0.0f, nullptr},
    // kInkLerp is the same kind of row as kFillLerp: a 0→1 progress the
    // composer synthesizes for an ink easing from one colour to another
    // under the node's transition. The description holds a colour in its
    // cascade block and no float, so there is nothing for `of` to return;
    // its call sites are hand-written beside the loops, each labelled
    // "the kInkLerp row", and the cascade pass is the one reader of the
    // colour it produces.
    {Instance::kInkLerp, SlotRole::Bespoke, nullptr, 0.0f,
     "a progress scalar over the declared ink's colour change — there is "
     "no Animatable<float> in the description to point at"},
};

static_assert(std::size(kSlotSpecs) == (size_t)Instance::kSlots,
              "each Instance::Slot requires one index-aligned kSlotSpecs row");

/** Index alignment and the Bespoke invariant, checked at compile time.
 *  Stronger than the size assert alone: it also catches a row inserted in
 *  the wrong place, a slot named twice, and a row with no accessor that did
 *  not declare itself out of reach. */
constexpr bool slotTableWellFormed() {
  for (size_t i = 0; i < std::size(kSlotSpecs); ++i) {
    if (kSlotSpecs[i].slot != (Instance::Slot)i)
      return false;  // rows must be index-aligned with the enum
    const bool bespoke = kSlotSpecs[i].role == SlotRole::Bespoke;
    if (bespoke != (kSlotSpecs[i].of == nullptr))
      return false;  // no accessor ⇔ declared out of the table's reach
    if (bespoke != (kSlotSpecs[i].bespoke != nullptr))
      return false;  // …and the declaration carries a written reason
  }
  return true;
}
static_assert(slotTableWellFormed(),
              "kSlotSpecs must be index-aligned with Instance::Slot, and a "
              "row with no accessor must declare SlotRole::Bespoke with a "
              "written reason");

/** The row's animatable on this node, or nullptr. A Bespoke row always
 *  answers nullptr, so a consumer that walks the table without special-casing
 *  one is INERT for it rather than dereferencing a null function pointer. */
inline const motion::Animatable<float>* slotValueOf(const SlotSpec& spec,
                                                    StyledNode styled) {
  return spec.of ? spec.of(styled) : nullptr;
}

/** The same, for the common caller: a mounted node, read through the style
 *  computed for it. */
inline const motion::Animatable<float>* slotValueOf(const SlotSpec& spec,
                                                    const Instance& inst) {
  return slotValueOf(spec, inst.styled());
}

}  // namespace sigil::compose::detail
