/** @file
 * The hand-written comparators over the animation values: curves by
 * function-pointer identity, transitions field by field, shaped bindings
 * field by field under their pin.
 */

#include "sigilcore/reconcile/Compare.h"

namespace sigil::core {

bool easeEqual(const choreograph::EaseFn& a, const choreograph::EaseFn& b) {
  const bool aSet = (bool)a, bSet = (bool)b;
  if (aSet != bSet) return false;
  if (!aSet) return true;
  using Ptr = float (*)(float);
  const Ptr* pa = a.target<Ptr>();
  const Ptr* pb = b.target<Ptr>();
  return pa && pb && *pa == *pb;  // lambdas: unequal (conservative)
}

static_assert(detail::kFieldCount<motion::Transition> == 3,
              "Transition gained or lost a field — rule on it in "
              "transitionEqual() below, then bump this count.");
bool transitionEqual(const motion::Transition& a, const motion::Transition& b) {
  return a.duration == b.duration && a.delay == b.delay &&
         easeEqual(a.easing(), b.easing());  // `ease` is read through easing()
}

static_assert(detail::kFieldCount<motion::BoundFloat> == 24,
              "BoundFloat gained or lost a field. boundMapEqual() below "
              "compares it BY HAND: rule on the new field (participate, or "
              "a stated reason not to), then bump this count. A miss is "
              "silent — the node prunes and keeps shaping through the old "
              "map forever.");
bool boundMapEqual(const motion::BoundFloat& a, const motion::BoundFloat& b) {
  return a.source == b.source && a.inScale == b.inScale &&
         a.inOffset == b.inOffset && a.clampInput == b.clampInput &&
         a.envelope == b.envelope && a.riseStart == b.riseStart &&
         a.holdStart == b.holdStart && a.holdEnd == b.holdEnd &&
         a.fallEnd == b.fallEnd && a.duty == b.duty && a.steps == b.steps &&
         a.scale == b.scale && a.offset == b.offset && a.clamped == b.clamped &&
         a.lo == b.lo && a.hi == b.hi && a.wiggleAmount == b.wiggleAmount &&
         a.wiggleFrequency == b.wiggleFrequency &&
         a.wiggleSeed == b.wiggleSeed && a.wiggleOctaves == b.wiggleOctaves &&
         a.wiggleFalloff == b.wiggleFalloff && a.wrapPeriod == b.wrapPeriod &&
         // The two curve slots compare under the same conservative rule: a
         // plain function is compared by identity, a capturing lambda is
         // unequal to everything and the binding re-patches every describe.
         easeEqual(a.curve, b.curve) && easeEqual(a.waveFn, b.waveFn);
}

}  // namespace sigil::core
