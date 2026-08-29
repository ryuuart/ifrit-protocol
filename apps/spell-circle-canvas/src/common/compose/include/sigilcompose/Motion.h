#pragma once

/** @file
 * SigilCompose animation vocabulary — SigilMotion's Transition, the `ease::`
 * curves, the animate() keyframe builders, the shaped bind() binding,
 * quantizeTime and Animatable, under the compose name every property slot
 * is spelled in. Nothing here is defined by this library; the re-export
 * exists so that authoring never has to name a second one.
 */

#include <sigilmotion/Animation.h>

namespace sigil::compose {

/** SigilMotion under the name authoring spells it. Inside this namespace
 *  `motion::` already resolves through the enclosing `sigil`; the alias
 *  makes the same spelling hold for a translation unit that brought this
 *  namespace in with a using-directive and nothing else. */
namespace motion = ::sigil::motion;

// ---------------------------------------------------------------------------
// Animation values

/** ANIMATION VALUES — Transition, the `ease::` curves, the animate()
 *  keyframe builders and the shaped `bind()` binding — are defined in
 *  SigilMotion (<sigilmotion/Animation.h>). None of them touches Skia,
 *  Yoga or the kernel, so other libraries can speak them without linking a
 *  drawing library; SigilCompose already links SigilMotion.
 *
 *  The re-export is permanent, not a shim. These types appear in compose's
 *  own signatures (Element::transition(), animate()'s spec argument, every
 *  Animatable property), so they are part of compose's authoring surface
 *  whoever defines them. `compose::Transition` and `motion::Transition`
 *  name one entity, not two competing spellings. */
using motion::animate;
using motion::bind;
using motion::Bound;
using motion::BoundFloat;
using motion::From;
using motion::from;
using motion::FromTo;
using motion::through;
using motion::To;
using motion::to;
using motion::Transition;
using motion::Transitioned;
using motion::Waypoints;
using motion::wiggle;
namespace ease = motion::ease;
/** The `floor(t·hz)/hz` time quantizer — the same arithmetic
 *  `Material::quantizeTime(hz)` applies to a shader's uTime, re-exported
 *  so host steppables quantizing their OWN schedules spell it the same
 *  way instead of hand-rolling it.
 *
 *  The derivation verb itself is `Ticker::derive(dst, bind(&src)…)`, a
 *  Ticker member rather than a free factory, so it needs no re-export and
 *  cannot collide with compose's `derive::` namespace (the geometry derive
 *  phase below), which already owns that word at namespace scope. */
using motion::quantizeTime;

/** `Animatable<T>` — THE PROPERTY SLOT: a value that can move. Four
 *  forms: a plain T, a Transitioned<T>, a live `choreograph::Output<T>*`,
 *  or that binding shaped through bind(). Defined in SigilMotion
 *  (<sigilmotion/Animation.h>) for the same reason as everything above —
 *  no Skia, Yoga or kernel type appears anywhere in it.
 *
 *  The two write paths meet in this one type. A plain T (or a
 *  Transitioned<T>) is DESCRIBED state: it changes only when the author
 *  describes again, and the reconciler's property comparison can see that
 *  it did. A bound `Output<T>*` is a per-frame value the host writes, and
 *  it compares BY IDENTITY — the pointer, not the number behind it — so a
 *  node holding one is declared volatile and does not cache. That is why
 *  handing back a freshly constructed Output at a new address breaks
 *  pruning even when the value is unchanged: the address is the property.
 *
 *  Compose owns RESOLUTION, not the value: an Animatable is resolved
 *  against a PaintContext, taking node transitions, stagger, mount
 *  entrances and the per-frame composer state into account. SigilMotion
 *  supplies the value; compose decides what a described change means to a
 *  node. */
using motion::Animatable;

}  // namespace sigil::compose
