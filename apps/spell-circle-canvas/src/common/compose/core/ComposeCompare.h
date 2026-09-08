#pragma once

/** @file
 * What the identity prune is spelled in: the comparator vocabulary a
 * description is compared through, and the field pins that make a field
 * added to a props block a BUILD FAILURE rather than a stale picture.
 */

#include <sigilcore/reconcile/Reads.h>
#include <sigilmotion/values/Animated.h>

namespace sigil::compose::detail {

struct ElementNode;

// ---------------------------------------------------------------------------
// FIELD PINS — a field added to a props block is a BUILD FAILURE
//
// THE FAILURE THIS CLOSES IS INVISIBLE BY CONSTRUCTION. `propsEqual()` and
// its helpers compare a description field by field; a field left out makes
// two DIFFERENT descriptions compare EQUAL, so the patch prunes,
// `markPaintDirtyUp()` never runs, a stale picture replays, and
// `applyTransitions()` — which only runs inside the `own` branch — never
// ramps an `animate()` on that property. Nothing errors. No test fails.
// A per-axis scale omitted from `propsEqual` and from `recordBounds()`'s
// transform gate is the shape this takes in practice: the property works
// on first paint and then quietly stops responding.
//
// THE MECHANISM is SigilCore's `kFieldCount<T>`, which reads the number of
// direct non-static data members straight off an aggregate. It is EXACT
// where the obvious alternative is not: a `static_assert(sizeof(T) == N)`
// is walked straight past by a `bool` dropped into tail padding, while a
// field count changes the moment a field does. Nested aggregates count as
// ONE field each, not as their flattened members.
//
// WHAT A PIN COSTS AND WHAT IT BUYS. Adding a field breaks the field-count
// `static_assert` in Reconcile.cpp (rule on it in the comparator —
// participate, or a stated reason not to — then bump the count), and then
// — for the blocks whose fields are all comparable lanes — the field walk
// `EveryPaintPropsFieldParticipatesInEquality` /
// `EveryBoundFloatFieldParticipatesInEquality` picks the new field up
// AUTOMATICALLY and fails until the comparator notices it. Two gates, one
// of them the compiler's.
//
// NO PIN IS NEEDED for a struct whose equality is `= default`
// (LayoutProps, Corners, MarkLabel, MarkAnchor, Echo, Anchor, Across, Parts,
// Span, ContentScalars): the compiler writes the exhaustive comparison
// and cannot forget a field. A pin exists only for a comparator a human
// wrote by hand — and the honest way to retire a pin is to give the struct
// a defaulted `operator==`.
//
// CLASSES WITH PRIVATE STATE (material::skia::Paint and Effect, Region,
// Animatable, Shape, Decoration, Profile) CANNOT be pinned — reading a field
// count needs an aggregate. Their hand-written comparators sit in the same
// header or translation unit as their members, so a field and its comparison
// are read together; PaintProps (here) and propsEqual (Reconcile.cpp) are the
// pair that can drift apart unseen.

using ::sigil::core::kFieldCount;

/** THE STRUCTURAL PRUNE (Reconcile.cpp). Declared here — rather than kept
 *  in Reconcile.cpp's anonymous namespace — so the field-participation
 *  tests can call the comparator DIRECTLY. Inferring a prune from
 *  `stats().patchedNodes` instead requires re-describing the SAME node,
 *  because keyed siblings never prune into one another; a test that
 *  compares two different nodes will report a difference whatever the
 *  comparator does, and so passes even when the field is unread. */
bool propsEqual(const ElementNode& a, const ElementNode& b);
/** The shaped-binding half of the same comparator, SigilMotion's: every
 *  field of BoundFloat participates, under the pin beside its body. */
using ::sigil::motion::boundMapEqual;
/** An Animatable compared where every other animated slot is:
 *  SigilMotion's form-by-form comparator. */
using ::sigil::motion::propEqual;

/** Constant, binding, or transitioned — one animatable flattened. */
using ::sigil::motion::ResolvedProp;
using ::sigil::motion::resolveProp;

// ---------------------------------------------------------------------------
// TEXT FX — the runtime side of the fx() seam (TextFx.cpp)

/** Equal only when PROVABLY identical: two easing curves compare equal when
 *  both are the same plain function pointer, and a lambda-valued curve
 *  compares unequal, conservatively. SigilMotion's one body, so no second
 *  spelling of the rule can let two comparators disagree about whether a
 *  node may prune. */
using ::sigil::motion::easeEqual;
/** Same duration, same delay, same curve under easeEqual. */
using ::sigil::motion::transitionEqual;
/** Did the DESCRIBED transform change between two descriptions? The lanes
 *  mirror propsEqual's transform block plus travel(). Defined in
 *  Reconcile.cpp beside the comparators it is built from. */
bool describedTransformEqual(const ElementNode& a, const ElementNode& b);

}  // namespace sigil::compose::detail
