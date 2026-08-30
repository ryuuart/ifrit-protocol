#pragma once

/** @file
 * Internal to the kernel — the slot table: one row per Instance::Slot,
 * naming the property it carries, its role and how it is read, so that a
 * slot added to the enum without a row is a build failure.
 */

#include <iterator>

#include "Instance.h"

namespace sigil::compose::detail {

// ---------------------------------------------------------------------------
// THE SLOT TABLE — a slot added to Instance::Slot is a BUILD FAILURE
//
// THE FAILURE THIS CLOSES IS THE SAME ONE ComposeInternal.h's FIELD PINS
// CLOSE, ONE LEVEL UP. Four functions consume `Instance::Slot` —
// `collectGroupScalars` and `computeVolatile` (Volatility.cpp),
// `applyMountTransitions` and `applyTransitions` (Transitions.cpp). Were
// each of them to enumerate the slots by hand, a slot appended to the enum
// would compile perfectly while being absent from any of them, and every
// one of those absences is silent:
//
//   - absent from `applyTransitions`  → `animate()` on the property never
//     ramps; it snaps, and looks like a missing transition spec.
//   - absent from `applyMountTransitions` → `animate(from().to())` plays no
//     entrance; the node just appears at its settled value.
//   - absent from `computeVolatile` → the property is not volatility, so an
//     ancestor caches across it and the motion FREEZES in a replayed
//     picture. Invisible in any still.
//   - absent from `collectGroupScalars` → a `Cache::Group` holds a bake
//     while the property moves, i.e. blits last frame's pixels.
//
// No test catches any of these: nothing errors, and a still frame of the
// affected node looks exactly right.
//
// THE MECHANISM: one row per enum value, INDEX-ALIGNED, under asserts that
// make a missing row, a duplicated row and a misordered row all fail to
// compile. What the rows carry is the only thing the four consumers ever
// want from a slot —
//
//   `of`      the description's Animatable for it (null when this node does
//             not carry the block that holds it), and
//   `role`    which of the three questions it answers.
//
// The three roles are not a taxonomy invented for the table; they are the
// split `computeVolatile` has to make anyway, which sorts its slots into
// opacity (applied by paint()'s saveLayer), geometric (applied by paint()'s
// matrix, so it moves the device rect and refuses a device-pinned bake) and
// content (rebuilds what the node RECORDS). The other three consumers each
// read that split or ignore it; none needs a fourth thing, which is why one
// table serves all four.
//
// WHY A TABLE HERE AND A NAMED SUBTRACTION IN computeVolatile. That
// function's content terms are a heterogeneous bag of booleans (a bound
// fill, an animated image frame, a live effect) with no enum behind them,
// so the only thing that can hold them together is a single named
// expression every consumer subtracts from. These twelve slots are an
// ENUMERATED AXIS instead, and an enumerated axis can be counted by the
// compiler.

/** Which of the three questions a slot answers — the axis `computeVolatile`
 *  already split on, named so the other three consumers can read it. */
enum class SlotRole : uint8_t {
  /** Applied by paint()'s saveLayer, OUTSIDE the node's content: a fading
   *  node replays its picture, and does not move its device rect. */
  Opacity,
  /** Applied by paint()'s matrix. Moves the device rect, so a device-space
   *  bake is refused while it runs (`Instance::transformLive`). */
  Geometric,
  /** Rebuilds what the node RECORDS, so its own picture is invalidated. */
  Content,
  /** No `Animatable<float>` in the description AT ALL, so the table cannot
   *  reach it and every consumer keeps its own handling. Costs a written
   *  reason in `bespoke`, which the assert below enforces. */
  Bespoke,
};

/** One row per `Instance::Slot`, index-aligned with the enum. */
struct SlotSpec {
  Instance::Slot slot;
  SlotRole role;
  /** The description's animatable for this slot on this node, or nullptr
   *  when the node does not carry the block that holds it (a node with no
   *  `travel()` has no `t`). Null for a Bespoke row — call it through
   *  slotValueOf(), which answers nullptr for those. */
  const Animatable<float>* (*of)(const ElementNode&);
  /** The standing endpoint a PATCH ramps from or to when `of` answers
   *  nullptr on one side of the diff — a node that GAINS or LOSES the block
   *  has no previous/next value, so the field's OWN DEFAULT is the
   *  endpoint. Pinned to the real default by
   *  ComposeSlotPins.EverySlotRowReachesItsOwnFieldAtItsStandingDefault;
   *  unused by a slot whose `of` never answers nullptr. */
  float standing;
  /** Why this slot is out of the table's reach. Non-null IFF the role is
   *  Bespoke — asserted below, so the escape hatch cannot be taken blank. */
  const char* bespoke;
};

inline constexpr SlotSpec kSlotSpecs[] = {
    {Instance::kOpacity, SlotRole::Opacity,
     [](const ElementNode& n) { return &n.paint.opacity; }, 1.0f, nullptr},
    {Instance::kTx, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.translateX; }, 0.0f, nullptr},
    {Instance::kTy, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.translateY; }, 0.0f, nullptr},
    {Instance::kRotate, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.rotate; }, 0.0f, nullptr},
    {Instance::kScale, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.scale; }, 1.0f, nullptr},
    // kFillLerp is a 0→1 PROGRESS the composer synthesizes for a
    // colour→colour `animate()`; the description holds an
    // `Animatable<Fill>` and no float anywhere, so there is nothing for
    // `of` to return. Its four call sites are hand-written beside the loop
    // that walks this table, each labelled "the kFillLerp row".
    {Instance::kFillLerp, SlotRole::Bespoke, nullptr, 0.0f,
     "a progress scalar over paint.fill's Transitioned<Fill> — there is no "
     "Animatable<float> in the description to point at"},
    {Instance::kSkewX, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.skewX; }, 0.0f, nullptr},
    {Instance::kSkewY, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.skewY; }, 0.0f, nullptr},
    {Instance::kScaleX, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.scaleX; }, 1.0f, nullptr},
    {Instance::kScaleY, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.scaleY; }, 1.0f, nullptr},
    // travel(): the `t` lane moves the node exactly as tx/ty do, so it is
    // the GEOMETRIC half and a device-space bake is refused while it runs.
    {Instance::kMotionT, SlotRole::Geometric,
     [](const ElementNode& n) -> const Animatable<float>* {
       return n.motionData ? &n.motionData->t : nullptr;
     },
     0.0f, nullptr},
    // onPath(): `at` is WHERE ALONG the baseline the run sits, so moving it
    // re-places every glyph INSIDE the node's own box and leaves the box
    // where it was. That is the CONTENT half, not the geometric one — the
    // recording is rebuilt, the device rect is not — and the resolved value
    // joins ContentScalars so a marquee that stops running releases like any
    // other settled scalar.
    {Instance::kTextPathAt, SlotRole::Content,
     [](const ElementNode& n) -> const Animatable<float>* {
       return n.textData && n.textData->onPath ? &n.textData->onPath->at
                                               : nullptr;
     },
     0.0f, nullptr},
};

static_assert(std::size(kSlotSpecs) == (size_t)Instance::kSlots,
              "every Instance::Slot needs a row here — a slot added without "
              "one is SILENTLY absent from collectGroupScalars, "
              "computeVolatile, applyMountTransitions and applyTransitions, "
              "and every one of those absences is invisible (no error, no "
              "failing test): see the comment above this table");

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
inline const Animatable<float>* slotValueOf(const SlotSpec& spec,
                                            const ElementNode& node) {
  return spec.of ? spec.of(node) : nullptr;
}

// ---- cross-TU paint/shape helpers -----------------------------------------

}  // namespace sigil::compose::detail
