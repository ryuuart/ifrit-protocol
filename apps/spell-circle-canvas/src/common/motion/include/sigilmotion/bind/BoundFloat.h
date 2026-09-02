#pragma once

/** @file
 * The evaluator of a shaped binding: the Envelope choice and the
 * BoundFloat record whose `apply()` runs the normalise → envelope →
 * curve → quantize → affine → wrap → wiggle → clamp chain on one
 * sample of the bound Output. A pure float→float map that reads no
 * clock.
 */

#include <choreograph/Choreograph.h>

#include <cstdint>
#include <tuple>

namespace sigil::motion {

/** WHICH SHAPE the normalised phase takes on its way through — the
 *  envelope stage of the binding chain below.
 *
 *  One per binding, because a phase has one shape: naming a second
 *  REPLACES the first, exactly as a second `map()` replaces the first
 *  curve. The alternative — independent flags — would have to define
 *  what a raised cosine of a trapezoid means, and there is no such
 *  thing. */
enum class Envelope : uint8_t {
  kNone,
  kPingPong,
  kCosine,
  kTrapezoid,
  kSquare,
  kWave
};

/** A live binding, SHAPED on its way to the property.
 *
 *  A bare `&output` binding lands on the property RAW, so a phase living
 *  in [0,1] cannot drive a translation in pixels without a second Output
 *  carrying pixels, updated by hand in the same steppable. These stages
 *  put that arithmetic next to the property it shapes instead:
 *
 *      .translateX(bind(&phase).target(-70, 170))
 *      .opacity(bind(&progress).map(ease::outBack()).clamp(0, 1))
 *      .scaleX(bind(&hp).source(0, maxHp))
 *
 *  The stages always run in this order, whatever order they were called
 *  in:
 *
 *    1. `source(lo, hi)` normalises the SOURCE range onto [0,1];
 *    2. the ENVELOPE — `pingPong`/`cosine`/`trapezoid`/`square`/`wave` —
 *       turns the one-way phase into a SHAPE across that span: there and
 *       back, a swell, a hold between two ramps, a pulse, or a shape of
 *       the caller's own;
 *    3. `map(ease)` shapes it (any `choreograph::EaseFn`, so the whole
 *       `ease::` namespace and every choreograph curve fits);
 *    4. the affine chain — `scale`/`offset`/`target`/`invert` — composes
 *       in CALL ORDER, so `.scale(240).offset(-70)` is `v*240 - 70` and
 *       `.offset(-70).scale(240)` is `(v-70)*240`, each reading the way
 *       it looks. `clamp` always applies last.
 *    5. `wrap(period)` folds the post-affine value into [0, period) —
 *       the looping-phase stage.
 *    6. `wiggle(amount, …)` adds smooth procedural noise in OUTPUT
 *       units, after the affine chain and any wrap, and before `clamp`.
 *       Phased off the normalised input rather than off a clock; see the
 *       verb for what that buys.
 *
 *  A shaped binding costs no more storage than a bare one: the map rides
 *  the out-of-line block `Animatable` already allocates for its
 *  transitioned form, so sizeof(Animatable) is unchanged and a property
 *  that never shapes anything pays nothing. */
struct BoundFloat {
  const choreograph::Output<float>* source = nullptr;
  float inScale = 1.0f, inOffset = 0.0f;  // source(): pre-curve normalise
  choreograph::EaseFn curve;              // map()
  bool clampInput = false;                // window(): clamp before the curve
  // The envelope stage: the shape, and the trapezoid's four corners in
  // NORMALISED phase. The corners are stored non-decreasing, so a zero-length
  // shoulder is an instant cut rather than a division by zero.
  Envelope envelope = Envelope::kNone;
  float riseStart = 0.0f, holdStart = 0.0f, holdEnd = 1.0f, fallEnd = 1.0f;
  // square(): the ON fraction of each period, stored clamped to [0,1].
  float duty = 0.5f;
  // wave(): the caller's own periodic shape, read on the folded phase.
  choreograph::EaseFn waveFn;
  int steps = 0;                      // quantize(): 0 = continuous
  float scale = 1.0f, offset = 0.0f;  // the affine chain
  bool clamped = false;
  float lo = 0.0f, hi = 1.0f;
  // wiggle(): the procedural noise stage. amount == 0 disengages it
  // entirely, at the cost of one float compare.
  float wiggleAmount = 0.0f;     // peak displacement, in OUTPUT units
  float wiggleFrequency = 2.0f;  // cycles per unit of NORMALISED input
  uint32_t wiggleSeed = 0;
  int wiggleOctaves = 1;
  float wiggleFalloff = 0.5f;
  // wrap(): fold the post-affine value into [0, period). 0 = no wrap.
  float wrapPeriod = 0.0f;

  /** Runs the chain on one sample of the bound Output. */
  float apply(float v) const;
};

/** Equal only when PROVABLY identical: two easing curves compare equal
 *  when both are the same plain function pointer. A lambda-valued curve
 *  compares unequal, conservatively, because a std::function holding one
 *  cannot be inspected.
 *
 *  ONE BODY for every curve slot in the library — the two on the record
 *  above, a Transition's, a Spread's distribution — because a second
 *  spelling of this rule would let two comparators disagree about
 *  whether the value that holds a curve may prune. It lives at the
 *  bottom of the library because the record above is the lowest thing in
 *  it that carries a curve. */
bool easeEqual(const choreograph::EaseFn& a, const choreograph::EaseFn& b);

/** Shaped bindings prune like anything else: same Output, same affine,
 *  same curve under easeEqual's rule. A re-describe that only changes the
 *  CURVE must NOT prune — the map is read live, so a pruned node would
 *  keep shaping through the old one forever. EVERY FIELD OF BoundFloat
 *  APPEARS in the body, under the pin beside it. */
bool boundMapEqual(const BoundFloat& a, const BoundFloat& b);

namespace detail {
/** The record decomposed member by member, for a comparator that wants to
 *  WALK it rather than name each field one at a time. Counting the fields
 *  does not need this: `core::kFieldCount<T>` reads any aggregate. */
inline auto fields(BoundFloat& v) {
  auto& [source, inScale, inOffset, curve, clampInput, envelope, riseStart,
         holdStart, holdEnd, fallEnd, duty, waveFn, steps, scale, offset,
         clamped, lo, hi, wiggleAmount, wiggleFrequency, wiggleSeed,
         wiggleOctaves, wiggleFalloff, wrapPeriod] = v;
  return std::tie(source, inScale, inOffset, curve, clampInput, envelope,
                  riseStart, holdStart, holdEnd, fallEnd, duty, waveFn, steps,
                  scale, offset, clamped, lo, hi, wiggleAmount, wiggleFrequency,
                  wiggleSeed, wiggleOctaves, wiggleFalloff, wrapPeriod);
}
}  // namespace detail

}  // namespace sigil::motion
