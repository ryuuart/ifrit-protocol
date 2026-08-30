#pragma once

/** @file
 * SHAPED BINDINGS — `bind(&output)`, a live choreograph::Output shaped
 * through a normalise → envelope → curve → affine → wrap → wiggle → clamp
 * chain on its way to whatever consumes it, and the value-noise field
 * behind `wiggle()`.
 *
 * Choreograph, cmath and integers only. The chain is a pure float→float
 * map of the bound Output: it reads no clock, so a consumer that renders
 * headlessly stays a pure function of what its Outputs hold.
 */

#include <choreograph/Choreograph.h>

#include <cstdint>
#include <functional>
#include <utility>

namespace sigil::motion {

namespace detail {

/** The noise field behind `Bound::wiggle()`. Deliberately private to this
 *  library and not shared with any GPU hash: those are bit-matched to
 *  compute kernels and pinned by parity tests, so borrowing one would tie
 *  an animation curve to a GPU ABI and let a future parity fix silently
 *  move every wiggle written against this one.
 *
 *  A 32-bit avalanche: every input bit reaches every output bit, so
 *  adjacent lattice cells and adjacent SEEDS come out uncorrelated. */
uint32_t wiggleHash(uint32_t x);

/** The lattice value at integer cell @p cell for @p seed, in [-1, 1).
 *  The seed is hashed BEFORE it is mixed with the cell, so seeds 0 and 1
 *  are as independent as seeds 0 and 5731. */
float wiggleLattice(int32_t cell, uint32_t seed);

/** ONE octave of 1-D VALUE noise, quintic-smoothed.
 *
 *  Value noise, not white noise, is the whole point: `wiggle()` exists
 *  because a property that teleports to a new random number every sample
 *  is not what any motion designer means by a shake — it reads as
 *  strobing, not as movement. The quintic fade (`6t⁵−15t⁴+10t³`,
 *  Perlin's improved-noise curve) is C² at the lattice, so neither the
 *  value nor its velocity nor its acceleration kinks as the phase
 *  crosses a cell boundary; the cheaper cubic smoothstep leaves a visible
 *  tick in a slow drift.
 *
 *  Out-of-range phases answer 0 rather than reinterpreting a float too
 *  large for `int32_t` (UB). A phase that big is a bug upstream, and a
 *  frozen wiggle is a debuggable symptom where UB is not. */
float wiggleOctave(float x, uint32_t seed);

/** Fractal sum of @p octaves octaves, each half the wavelength and
 *  @p falloff the amplitude of the one before — and NORMALISED by the
 *  weight sum, so the result stays in [-1, 1] whatever the octave count.
 *
 *  The normalisation is the point. `Bound::wiggle`'s `amount` promises a
 *  peak displacement in the property's own units, and a promise that
 *  changed as you added detail would not be one. Adding an octave here
 *  changes the TEXTURE, never the size. (After Effects does not
 *  normalise, which is why adding octaves there gets louder and has to be
 *  corrected by hand.) */
float wiggleNoise(float x, uint32_t seed, int octaves, float falloff);

}  // namespace detail

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

/** Builder for BoundFloat — see the doc above. Converts implicitly into
 *  any `Animatable<float>` property. */
class Bound {
 public:
  // The parameter is `out` rather than `source` because source() is a
  // stage name on this class, and a parameter of that name would shadow
  // it inside the constructor.
  explicit Bound(const choreograph::Output<float>* out) { m_b.source = out; }

  /** Normalise the SOURCE's own range onto [0,1] before everything else.
   *  `bind(&hp).source(0, maxHp)` is the health-bar spelling.
   *
   *  Named for the stage rather than for a direction. A bound chain has
   *  two ranges — the one the Output speaks and the one the property
   *  wants — and calling them `from`/`to` would collide with the authored
   *  `animate(from(a).to(b))` words, which mean the endpoints of a single
   *  ramp instead. `source` and `target` name the two ranges; `from` and
   *  `to` stay the ramp's. */
  Bound& source(float lo, float hi);
  /** `source()` that also CLAMPS the normalised value to [0,1].
   *
   *  Prefer this whenever the range names a beat inside a longer
   *  timeline. The curve runs after normalisation, so with plain
   *  `source()` an Output outside the named range feeds the easing a
   *  value outside [0,1] — and none of the `ease::` curves is defined
   *  there. `window()` says "this beat and nothing else", which is what
   *  a range on a multi-beat timeline nearly always means. */
  Bound& window(float lo, float hi);
  /** THERE AND BACK: the value runs 0 → 1 → 0 across the source span, so
   *  a sweep, a marquee or a highlight RETURNS instead of jumping back to
   *  the start.
   *
   *  A triangle of period 1 in the normalised phase, so it also repeats:
   *  bind a phase that climbs forever and `.source(0, period).pingPong()`
   *  bounces forever, while `.window(a, b).pingPong()` gives one out-and-
   *  back inside that beat and rests dark on either side.
   *
   *  The peak is at the MIDDLE of the span, which is where a there-and-
   *  back turns around; `cosine()` is the same journey eased. */
  Bound& pingPong();
  /** THE SWELL: a raised cosine — 0 at both ends of the source span, 1 at
   *  its middle, eased into and out of both. The breath.
   *
   *      .opacity(bind(&seconds).source(0, 7.2f).cosine())
   *
   *  This is what a beat that comes BACK needs and a window cannot give:
   *  `window()` is one-way, so a swell written with one arrives and stays.
   *  Periodic in the normalised phase, so a monotonic seconds Output
   *  breathes for as long as it runs rather than for one span only.
   *
   *  Shaped like `pingPong()` but smooth at the turn: use this when the
   *  value is a swell and that one when it is a traverse. */
  Bound& cosine();
  /** RAMP UP, HOLD, RAMP DOWN — the loop envelope, so a cycle can CUT
   *  while it is dark.
   *
   *      .opacity(bind(&cycle).source(0, 15.f).trapezoid(0, .03f, .84f, 1))
   *
   *  All four corners are positions in the NORMALISED phase, read in
   *  order: 0 before @p riseStart, ramping 0→1 across
   *  [@p riseStart, @p holdStart], exactly 1 across
   *  [@p holdStart, @p holdEnd], ramping 1→0 across
   *  [@p holdEnd, @p fallEnd], and 0 after. Each corner is held to at
   *  least the one before it, so a shoulder of zero length is an instant
   *  cut and out-of-order corners cannot ask for a negative ramp.
   *
   *  The ramps are LINEAR, and `map()` runs on what this produced: any
   *  curve through (0,0) and (1,1) rounds both shoulders while leaving
   *  the hold at exactly 1 and the dark at exactly 0, so the shoulder
   *  shape is a separate decision from where the corners are.
   *
   *  Not periodic, unlike the other two envelopes: it names positions
   *  inside ONE pass, and a repeating sheet rides a phase that already
   *  wraps. */
  Bound& trapezoid(float riseStart, float holdStart, float holdEnd,
                   float fallEnd);
  /** THE PULSE: 1 across the first @p duty of each period of the
   *  normalised phase, 0 across the rest — the blinking caret, the beacon,
   *  the strobe gate, folded on the same period `pingPong()` folds on so a
   *  monotonic seconds Output pulses forever.
   *
   *      .opacity(bind(&secs).source(0, 1.06f).square(0.62f / 1.06f)
   *                   .target(0.10f, 1.0f))
   *
   *  PHASE 0 IS ON. A pulse's owner is born at the start of its cycle —
   *  a caret is born visible — and the fold makes phase 1 the same
   *  instant as phase 0, so the ON test is `u < duty`, true at exactly 0.
   *  A shape that answered 0 there would blank its owner for one instant
   *  at every seam of a wrapping phase.
   *
   *  The two levels are exactly 0 and 1; put the resting and lit values
   *  in with the affine chain, as the caret above does. @p duty is
   *  clamped to [0,1]: 0 is never on, 1 is always on. `map()` still runs
   *  on the result, but a curve through (0,0) and (1,1) leaves both
   *  levels where they are — a pulse has no shoulders to round. */
  Bound& square(float duty = 0.5f);
  /** THE CUSTOM WAVEFORM — the escape hatch when none of the named shapes
   *  is the one you mean. @p shape is evaluated on the FOLDED phase
   *  u ∈ [0,1), so whatever it draws across one period the signal repeats:
   *  `wave([](float u) { return u * u; })` is a sawtooth with a curved
   *  ramp, and every named envelope could have been written this way.
   *
   *  Same slot as the named shapes: naming this after `pingPong()`,
   *  `cosine()`, `trapezoid()` or `square()` replaces them, and vice
   *  versa.
   *
   *  THE FUNCTION IS PART OF THE BINDING'S IDENTITY, compared as `map()`'s
   *  curve is: a consumer that prunes on equality can compare a PLAIN
   *  function pointer and does, while a capturing lambda compares unequal
   *  to everything — so a described binding carrying one re-patches on
   *  every describe instead of pruning. Name the shape as a free function
   *  where that cost matters. */
  Bound& wave(choreograph::EaseFn shape);
  /** Shape the (normalised) value — any choreograph easing, including the
   *  parameterised `ease::` family. */
  Bound& map(choreograph::EaseFn curve);
  Bound& scale(float s);
  Bound& offset(float o);
  /** Map [0,1] onto the TARGET range [lo,hi] — exactly
   *  `scale(hi-lo).offset(lo)`, spelled the way you think about it. */
  Bound& target(float lo, float hi);
  /** 1 − v, composed correctly with whatever affine stages came before
   *  rather than applied to the raw input. */
  Bound& invert();
  /** Snap to @p steps discrete levels across [0,1], BEFORE the affine
   *  chain — so the levels are evenly spaced in the normalised value and
   *  land wherever the affine stages then put them. A count of levels,
   *  not a rate; `quantizeTime` is the rate-on-seconds operation. */
  Bound& quantize(int steps);
  /** LOOP the shaped value: fold the post-affine value into
   *  [0, @p period), floor-convention — so a DESCENDING schedule wraps
   *  up into range instead of going negative, which `std::fmod` alone
   *  would not do. For a positive schedule,
   *  `bind(&seconds).scale(k).wrap(1.0f)` is bit-identical to
   *  `std::fmod(seconds * k, 1.0f)`.
   *
   *  Sits AFTER the affine chain and BEFORE `wiggle` and `clamp`, so a
   *  wrapped phase still wiggles continuously across the seam (the noise
   *  phase reads the unwrapped input) and a clamp still bounds the
   *  folded value. `period <= 0` is a no-op rather than a division, so
   *  "no wrap" is representable, and it is the default. */
  Bound& wrap(float period);
  /** PROCEDURAL NOISE on the way out — camera shake, handheld drift,
   *  organic jitter, turbulence. All five parameters are defaulted, so
   *  `.wiggle()` alone is ±1 output unit at 2 cycles per unit of input.
   *
   *  @param amount    PEAK displacement, in the PROPERTY'S OWN UNITS —
   *                   pixels on translateX, degrees on rotate, alpha on
   *                   opacity. Bounded: the noise is normalised to
   *                   [-1, 1], so the value never leaves ±amount of what
   *                   the chain would otherwise have produced.
   *  @param frequency Cycles per unit of NORMALISED input. Bind a phase
   *                   ramping in SECONDS with no source() and this is
   *                   literally Hz; bind a [0,1] progress and it is
   *                   cycles across the beat.
   *  @param seed      Which noise. Same seed ⇒ same wiggle, always;
   *                   different seed ⇒ an independent one. This is not a
   *                   nicety — a two-axis shake whose x and y share a
   *                   seed moves the layer along a diagonal, which is the
   *                   one thing a shake must not do.
   *  @param octaves   1 is a smooth drift; 2–3 adds the fine tremble that
   *                   makes an impact read as an impact. Clamped to 1..8.
   *  @param falloff   How much quieter each octave is than the last.
   *                   Clamped to [0,1]; near 1 reads as turbulence, near
   *                   0 as a drift with a whisper on it.
   *
   *  TWO PROPERTIES, both load-bearing:
   *
   *  1. **It reads NO CLOCK.** The noise is a pure function of the bound
   *     Output, sampled at the NORMALISED input (after
   *     `source()`/`window()`, before `map()`). "Wiggle over time" is
   *     therefore spelled by binding a phase that ramps with time. That
   *     keeps `apply()` a pure float→float map, which is what lets a
   *     headless renderer stay a pure function of what its Outputs hold
   *     and produce byte-identical frames for a given frame index. A
   *     `steady_clock` read inside `apply()` would make every such frame
   *     a function of wall time instead.
   *
   *     Sampling the phase BEFORE the curve is the other half of it: the
   *     curve shapes the SIGNAL, not the schedule, so `.map()` does not
   *     make the shake ease out along with the value, and `.quantize()`
   *     does not make it stair-step. Under `window()` the phase clamps
   *     with the input, so a wiggle scoped to a beat holds still outside
   *     that beat.
   *
   *  2. **The amplitude is added AFTER the affine chain**, before
   *     `clamp()`. `wiggle(30)` therefore means 30 pixels on a
   *     `.target(-70, 170)` translation. Adding it to the normalised
   *     input instead would make `amount` a fraction of a range the
   *     author has not named yet at that point in the chain, so the same
   *     number would mean a fraction of a pixel on one property and a
   *     dozen pixels on the next. `clamp()` still applies last, so a
   *     wiggled opacity still lands in [0,1].
   *
   *  A shake rig, both axes, in two lines. Note the different seeds and
   *  the `scale(0)`, which drops the phase's own contribution so the
   *  property holds at rest and ONLY the noise moves it (the free
   *  `wiggle(&out, …)` below is that spelling, named):
   *
   *      .translateX(bind(&seconds).scale(0).wiggle(12.f, 7.f, 1))
   *      .translateY(bind(&seconds).scale(0).wiggle(12.f, 7.f, 2))
   */
  Bound& wiggle(float amount = 1.0f, float frequency = 2.0f, uint32_t seed = 0,
                int octaves = 1, float falloff = 0.5f);
  /** Bound the OUTPUT; always applied last, whenever it is written. */
  Bound& clamp(float lo, float hi);

  const BoundFloat& value() const { return m_b; }

 private:
  BoundFloat m_b;
};

/** `bind(&output)` — a binding you can shape. `&output` on its own still
 *  works and stays the zero-overhead form. */
Bound bind(const choreograph::Output<float>* source);
/** PURE NOISE around rest — `bind(&out).scale(0).wiggle(…)`, named.
 *
 *  For a shake rig the bound Output is a SCHEDULE, not a value: the
 *  property should sit at rest and only the noise should move it.
 *  Spelling that through `bind()` needs a `.scale(0)` whose omission is
 *  not an error but a slow drift off screen, as the property tracks the
 *  phase itself. Hence a separate word for the case:
 *
 *      .translateX(wiggle(&seconds, 12.f, 7.f, 1))   // ±12 px @ 7 Hz
 *      .translateY(wiggle(&seconds, 12.f, 7.f, 2))   // ← different seed
 *      .rotate(wiggle(&seconds, 1.5f, 5.f, 3))       // ±1.5°
 *
 *  Different seeds per axis is the whole rig: shared seeds move the
 *  layer along a diagonal. It returns an ordinary `Bound`, so
 *  `.offset(restValue)` parks the shake somewhere other than zero and
 *  `.clamp()` still bounds it. */
Bound wiggle(const choreograph::Output<float>* source, float amount = 1.0f,
             float frequency = 2.0f, uint32_t seed = 0, int octaves = 1,
             float falloff = 0.5f);

}  // namespace sigil::motion
