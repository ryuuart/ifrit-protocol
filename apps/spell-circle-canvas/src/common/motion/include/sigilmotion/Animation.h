#pragma once

/** @file
 * ANIMATION VALUES — the describable side of motion, with no renderer in
 * it: a transition spec, the house easing curves as EaseFn values, the
 * `animate(from(a).to(b))` / `animate(through({…}))` keyframe builders,
 * `bind(&output)` — a live choreograph::Output shaped through
 * normalise → envelope → curve → affine chain on its way to whatever
 * consumes it —
 * and `Animatable<T>`, the property SLOT that holds any one of them.
 *
 * Nothing here touches Skia, Yoga, or any drawing library: it is
 * choreograph, chrono and float math. That is deliberate, and it is what
 * lets libraries that render (and libraries that only compute geometry)
 * hold these values without linking a renderer.
 *
 * SigilMotion supplies the VALUES and the clock. A consumer supplies the
 * property slot that stores one and the runtime that reads it each frame;
 * there is no resolve surface here, because resolving depends on context
 * the consumer owns.
 */

#include <choreograph/Choreograph.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::motion {

/** How a property change animates instead of snapping.
 *  `delay` holds the CURRENT value (the `from`, for animate() entrances)
 *  before the ramp starts, which is the stagger primitive: give a set of
 *  siblings delays that step by a fixed amount and the cascade is data
 *  rather than bookkeeping. */
struct Transition {
  std::chrono::milliseconds duration{250};
  choreograph::EaseFn ease = &choreograph::easeOutQuad;
  std::chrono::milliseconds delay{0};

  /** ALWAYS read the curve through here, never through `ease` directly.
   *
   *  `{360ms, {}, 220ms}` is the obvious way to write "the default curve,
   *  but I need to name the delay" — and because Transition is an
   *  aggregate, that `{}` initialises `ease` to an EMPTY std::function.
   *  It compiles, and calling it throws `bad_function_call` on the first
   *  frame. This accessor substitutes the default curve for an empty
   *  function, so `{}` means what the author meant. */
  const choreograph::EaseFn& easing() const {
    static const choreograph::EaseFn kDefault = &choreograph::easeOutQuad;
    return ease ? ease : kDefault;
  }
};

/** The house curves, as EaseFn VALUES.
 *
 *  `Transition::ease` holds a `choreograph::EaseFn`, a plain float→float
 *  function. Choreograph's most expressive curves — back, elastic,
 *  bounce — take an extra shape parameter, so `&choreograph::easeOutBack`
 *  does not convert to an EaseFn at all and the compiler answers with a
 *  wall of overload-resolution noise. These wrappers bind the shape
 *  parameter and hand back something a Transition can hold:
 *
 *      .scale(animate(from(0.86f).to(1.0f), {520ms, ease::outBack()}))
 */
namespace ease {
/** Overshoot and settle. `s` is the overshoot amount (Penner's 1.70158
 *  overshoots by ~10%); larger exaggerates the anticipation. */
inline choreograph::EaseFn outBack(float s = 1.70158f) {
  return [s](float t) { return choreograph::easeOutBack(t, s); };
}
inline choreograph::EaseFn inBack(float s = 1.70158f) {
  return [s](float t) { return choreograph::easeInBack(t, s); };
}
inline choreograph::EaseFn inOutBack(float s = 1.70158f) {
  return [s](float t) { return choreograph::easeInOutBack(t, s); };
}
/** Ring down to rest. `a` is amplitude, `p` the period. */
inline choreograph::EaseFn outElastic(float a = 1.0f, float p = 0.3f) {
  return [a, p](float t) { return choreograph::easeOutElastic(t, a, p); };
}
inline choreograph::EaseFn inElastic(float a = 1.0f, float p = 0.3f) {
  return [a, p](float t) { return choreograph::easeInElastic(t, a, p); };
}
/** Land and bounce. */
inline choreograph::EaseFn outBounce(float a = 1.70158f) {
  return [a](float t) { return choreograph::easeOutBounce(t, a); };
}
}  // namespace ease

/** Posterize TIME at a declared rate: `floor(t·hz)/hz`, held still
 *  between steps.
 *
 *  Not the same operation as `Bound::quantize`, despite the similar name.
 *  This one snaps unbounded seconds to a RATE; that one snaps a
 *  normalised [0,1] value to a COUNT OF LEVELS. The different word marks
 *  the different contract.
 *
 *  A template on ONE type, so every call site keeps its own precision:
 *  `quantizeTime(t, 8.0)` is double math, `quantizeTime(ft, 8.0f)` is
 *  float. That makes replacing a hand-written `std::floor(t * hz) / hz`
 *  with this call bit-identical rather than merely close.
 *
 *  `hz <= 0` answers the input unchanged — the spelling of
 *  "continuous". */
template <typename T>
inline T quantizeTime(T t, T hz) {
  return hz > T(0) ? std::floor(t * hz) / hz : t;
}

template <typename T>
struct Transitioned {
  /** Value-initialized, not default-initialized. `animate(through({}))`
   *  builds one of these and then fills nothing; for a scalar T,
   *  default-initialization would leave the property reading whatever
   *  was on the stack. An empty keyframe list is a degenerate ask, but
   *  it has to be a DETERMINATE one — zero. */
  T value{};
  Transition spec;
  /** animate(from(a).to(b)): where the value ENTERS from when the node
   *  first mounts. Empty for a plain `animate(to(v))` — no entrance, only
   *  change transitions. */
  std::optional<T> from{};
  /** animate(through({...})): the mount-time path as (absolute time,
   *  value) pairs — multi-segment entrances (damped overshoots) that one
   *  from→to ramp can't shape. Mount-only choreography: later changes
   *  retarget to `value` like any `animate(to(v))`. */
  std::vector<std::pair<std::chrono::milliseconds, T>> waypoints{};
};

/** The argument builders for animate(). Nobody spells these types; they
 *  exist so the call site reads `animate(from(a).to(b), spec)`. Beware
 *  that a local variable named `from` or `to` shadows the factory —
 *  qualify the call (`motion::from(...)`) at any site that has one.
 *
 *  TWO SHAPES, ONE VERB, and the difference is the whole grammar:
 *
 *  - `to(v)` ALONE is RAMP ON CHANGE. It says nothing about mounting; it
 *    says that whenever the described value differs from what the
 *    instance holds, the property ramps to the new one instead of
 *    snapping. Re-describing a different `to(v)` retargets from the
 *    CURRENT value.
 *  - `from(a).to(b)` is a MOUNT ENTRANCE. The path plays once, when the
 *    node first appears; afterwards the property behaves exactly like
 *    `to(b)`.
 *
 *  So an author who wants "this number should never jump" writes the
 *  first, and an author who wants "this should fly in" writes the
 *  second — one verb, and the argument says which. */
template <typename T>
struct FromTo {
  T from;
  T to;
};
/** The ramp-on-change argument: `animate(to(v), spec)`. */
template <typename T>
struct To {
  T value;
};
template <typename T>
struct From {
  T value;
  FromTo<T> to(T target) { return {std::move(value), std::move(target)}; }
};
template <typename T>
From<T> from(T v) {
  return {std::move(v)};
}
template <typename T>
To<T> to(T v) {
  return {std::move(v)};
}

template <typename T>
struct Waypoints {
  std::vector<std::pair<std::chrono::milliseconds, T>> frames;
};

/** The waypoint path for the common float case, spelled without a
 *  template argument. A nested braced list is a non-deduced context, so
 *  the generic overload below cannot deduce its element type and has to
 *  be told it explicitly. */
inline Waypoints<float> through(
    std::initializer_list<std::pair<std::chrono::milliseconds, float>> frames) {
  return {{frames.begin(), frames.end()}};
}
/** Any other value type — `through<SkColor4f>({...})`. */
template <typename T>
Waypoints<T> through(
    std::vector<std::pair<std::chrono::milliseconds, T>> frames) {
  return {std::move(frames)};
}

/** A MOUNT ENTRANCE: the runtime manufactures the motion, as against
 *  `bind()`, where the caller drives an Output itself. The first word at
 *  the call site names who owns the motion.
 *
 *      .opacity(animate(from(0.0f).to(1.0f), {400ms}))
 *      .scale(animate(from(0.86f).to(1.0f), {520ms, ease::outBack()}))
 *
 *  The path plays once, when the property's owner first appears — the
 *  same idea as a CSS animation-on-enter. Afterwards the property behaves
 *  exactly like `animate(to(v), spec)`: later changes retarget from the
 *  CURRENT value, and re-describing the same animate() does not restart
 *  the entrance.
 *
 *  A consumer that bakes a still rather than running frames resolves this
 *  to its settled value; an entrance needs a mount to play.
 *
 *  `spec` defaults to the house Transition, so name one only when the
 *  beat matters. */
template <typename T>
Transitioned<T> animate(FromTo<T> ft, Transition spec = {}) {
  return {std::move(ft.to), std::move(spec), std::move(ft.from)};
}

/** RAMP ON CHANGE, with a transition of this property's own:
 *
 *      .opacity(animate(to(dimmed ? 0.4f : 1.0f), {180ms}))
 *
 *  No entrance — the property starts out holding the value. Every LATER
 *  description carrying a different value ramps to it over `spec` instead
 *  of snapping, retargeting from wherever the property currently is.
 *  There is one motion per property, so a change mid-ramp bends the ramp
 *  rather than queueing a second one behind it. */
template <typename T>
Transitioned<T> animate(To<T> t, Transition spec = {}) {
  return {std::move(t.value), std::move(spec)};
}

/** The keyframe path: absolute (time, value) waypoints played through on
 *  first appearance — the multi-segment entrances, such as a damped
 *  overshoot, that a single from→to ramp cannot shape:
 *
 *    .translateX(animate(through(
 *        {{0ms, 40.f}, {200ms, -20.f}, {300ms, 10.f}, {400ms, 0.f}})))
 *
 *  `ease` applies PER SEGMENT. A leading time greater than zero holds the
 *  first value, which is a built-in delay; `Transition::delay` still adds
 *  on top of it. Once the path completes the value behaves like
 *  `animate(to(last))`. */
template <typename T>
Transitioned<T> animate(Waypoints<T> w,
                        choreograph::EaseFn ease = &choreograph::easeOutQuad) {
  Transitioned<T> t;
  t.spec.ease = std::move(ease);
  if (!w.frames.empty()) {
    t.from = w.frames.front().second;
    t.value = w.frames.back().second;
    t.spec.duration = w.frames.back().first;
  }
  t.waypoints = std::move(w.frames);
  return t;
}

namespace detail {

/** The noise field behind `Bound::wiggle()`. Deliberately private to this
 *  library and not shared with any GPU hash: those are bit-matched to
 *  compute kernels and pinned by parity tests, so borrowing one would tie
 *  an animation curve to a GPU ABI and let a future parity fix silently
 *  move every wiggle written against this one.
 *
 *  A 32-bit avalanche: every input bit reaches every output bit, so
 *  adjacent lattice cells and adjacent SEEDS come out uncorrelated. */
inline uint32_t wiggleHash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

/** The lattice value at integer cell @p cell for @p seed, in [-1, 1).
 *  The seed is hashed BEFORE it is mixed with the cell, so seeds 0 and 1
 *  are as independent as seeds 0 and 5731. */
inline float wiggleLattice(int32_t cell, uint32_t seed) {
  const uint32_t h =
      wiggleHash((uint32_t)cell * 0x9e3779b9u ^ wiggleHash(seed + 0x85ebca6bu));
  return (float)(h >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

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
inline float wiggleOctave(float x, uint32_t seed) {
  const float base = std::floor(x);
  if (!(base > -2.0e9f && base < 2.0e9f)) return 0.0f;
  const float t = x - base;
  const int32_t cell = (int32_t)base;
  const float u = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
  const float a = wiggleLattice(cell, seed);
  const float b = wiggleLattice(cell + 1, seed);
  return a + (b - a) * u;
}

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
inline float wiggleNoise(float x, uint32_t seed, int octaves, float falloff) {
  const int n = octaves < 1 ? 1 : (octaves > 8 ? 8 : octaves);
  float sum = 0.0f, weight = 0.0f, amp = 1.0f, freq = 1.0f;
  for (int i = 0; i < n; ++i) {
    sum += amp * wiggleOctave(x * freq, seed + (uint32_t)i * 0x9e3779b9u);
    weight += amp;
    amp *= falloff;
    freq *= 2.0f;
  }
  return weight > 0.0f ? sum / weight : 0.0f;
}

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

  float apply(float v) const {
    v = v * inScale + inOffset;
    if (clampInput) v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    // The noise PHASE is read here — off the normalised input, before the
    // curve shapes it and before the affine chain puts it in output
    // units. Phase from the schedule, amplitude in output units; see
    // Bound::wiggle for why that pairing is the useful one.
    const float phase = v;
    // THE ENVELOPE, on the phase and before the curve. It sits after the
    // noise phase is read for the same reason `wrap` does: the shake reads
    // the SCHEDULE, so a ping-ponged phase does not retrace the identical
    // shake on the way back and a trapezoid's dark stretch does not freeze
    // it. It sits before `map` so the curve shapes what the envelope
    // PRODUCED — any curve through (0,0) and (1,1) rounds a trapezoid's
    // shoulders while leaving its hold at exactly 1 and its dark at exactly
    // 0 — and before the affine chain, which is what puts the shape into
    // the property's own units.
    switch (envelope) {
      case Envelope::kNone:
        break;
      case Envelope::kPingPong: {
        // A triangle of period 1: there and back across the span, and then
        // again, so a phase that keeps climbing keeps bouncing.
        const float u = v - std::floor(v);
        v = u < 0.5f ? u + u : 2.0f - (u + u);
        break;
      }
      case Envelope::kCosine:
        // Periodic by construction — the same repeat, from the cosine
        // itself rather than from a fold.
        v = 0.5f - 0.5f * std::cos(6.28318530717958648f * v);
        break;
      case Envelope::kTrapezoid:
        // Dark outside [riseStart, fallEnd] — NOT folded into one span,
        // because `window()` clamps its input to exactly 1 and a fold would
        // read that as the START of the next pass, turning a settled beat
        // dark. A repeating trapezoid rides a phase that already wraps.
        if (v <= riseStart || v >= fallEnd)
          v = 0.0f;
        else if (v < holdStart)
          // Reachable only when the corners differ, because the test above
          // already answered every v at or below riseStart — which is what
          // makes a zero-length shoulder an instant cut rather than a
          // division by zero.
          v = (v - riseStart) / (holdStart - riseStart);
        else if (v <= holdEnd)
          v = 1.0f;
        else
          v = (fallEnd - v) / (fallEnd - holdEnd);
        break;
      case Envelope::kSquare: {
        // The pulse, folded on the same period pingPong folds on: 1 across
        // the first `duty` of each period, 0 across the rest. PHASE 0 IS
        // ON — `u < duty` answers 1 at exactly 0 — because a pulse's owner
        // is born at the start of its cycle and must be born visible; a
        // shape answering 0 at exactly 0 would blank that one instant at
        // every seam of a wrapping phase.
        const float u = v - std::floor(v);
        v = u < duty ? 1.0f : 0.0f;
        break;
      }
      case Envelope::kWave: {
        // The caller's own shape, read on the folded phase u ∈ [0,1) — so
        // whatever the function draws across one period, the signal
        // repeats it. An empty function passes the folded phase through.
        const float u = v - std::floor(v);
        v = waveFn ? waveFn(u) : u;
        break;
      }
    }
    if (curve) v = curve(v);
    if (steps > 1) v = std::round(v * (float)(steps - 1)) / (float)(steps - 1);
    v = v * scale + offset;
    // AFTER the affine chain, BEFORE wiggle: the noise phase above reads
    // the unwrapped schedule, so a wrapped phase wiggles continuously
    // across the seam instead of repeating its shake every lap.
    if (wrapPeriod > 0.0f) {
      v = std::fmod(v, wrapPeriod);
      if (v < 0.0f) v += wrapPeriod;
    }
    if (wiggleAmount != 0.0f)
      v += wiggleAmount * detail::wiggleNoise(phase * wiggleFrequency,
                                              wiggleSeed, wiggleOctaves,
                                              wiggleFalloff);
    if (clamped) v = v < lo ? lo : (v > hi ? hi : v);
    return v;
  }
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
  Bound& source(float lo, float hi) {
    const float span = hi - lo;
    m_b.inScale = span != 0.0f ? 1.0f / span : 0.0f;
    m_b.inOffset = span != 0.0f ? -lo / span : 0.0f;
    return *this;
  }
  /** `source()` that also CLAMPS the normalised value to [0,1].
   *
   *  Prefer this whenever the range names a beat inside a longer
   *  timeline. The curve runs after normalisation, so with plain
   *  `source()` an Output outside the named range feeds the easing a
   *  value outside [0,1] — and none of the `ease::` curves is defined
   *  there. `window()` says "this beat and nothing else", which is what
   *  a range on a multi-beat timeline nearly always means. */
  Bound& window(float lo, float hi) {
    source(lo, hi);
    m_b.clampInput = true;
    return *this;
  }
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
  Bound& pingPong() {
    m_b.envelope = Envelope::kPingPong;
    return *this;
  }
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
  Bound& cosine() {
    m_b.envelope = Envelope::kCosine;
    return *this;
  }
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
                   float fallEnd) {
    m_b.envelope = Envelope::kTrapezoid;
    m_b.riseStart = riseStart;
    m_b.holdStart = holdStart < riseStart ? riseStart : holdStart;
    m_b.holdEnd = holdEnd < m_b.holdStart ? m_b.holdStart : holdEnd;
    m_b.fallEnd = fallEnd < m_b.holdEnd ? m_b.holdEnd : fallEnd;
    return *this;
  }
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
  Bound& square(float duty = 0.5f) {
    m_b.envelope = Envelope::kSquare;
    m_b.duty = duty < 0.0f ? 0.0f : (duty > 1.0f ? 1.0f : duty);
    return *this;
  }
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
  Bound& wave(choreograph::EaseFn shape) {
    m_b.envelope = Envelope::kWave;
    m_b.waveFn = std::move(shape);
    return *this;
  }
  /** Shape the (normalised) value — any choreograph easing, including the
   *  parameterised `ease::` family. */
  Bound& map(choreograph::EaseFn curve) {
    m_b.curve = std::move(curve);
    return *this;
  }
  Bound& scale(float s) {
    m_b.scale *= s;
    m_b.offset *= s;
    return *this;
  }
  Bound& offset(float o) {
    m_b.offset += o;
    return *this;
  }
  /** Map [0,1] onto the TARGET range [lo,hi] — exactly
   *  `scale(hi-lo).offset(lo)`, spelled the way you think about it. */
  Bound& target(float lo, float hi) { return scale(hi - lo).offset(lo); }
  /** 1 − v, composed correctly with whatever affine stages came before
   *  rather than applied to the raw input. */
  Bound& invert() {
    m_b.scale = -m_b.scale;
    m_b.offset = 1.0f - m_b.offset;
    return *this;
  }
  /** Snap to @p steps discrete levels across [0,1], BEFORE the affine
   *  chain — so the levels are evenly spaced in the normalised value and
   *  land wherever the affine stages then put them. A count of levels,
   *  not a rate; `quantizeTime` is the rate-on-seconds operation. */
  Bound& quantize(int steps) {
    m_b.steps = steps > 1 ? steps : 0;
    return *this;
  }
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
  Bound& wrap(float period) {
    m_b.wrapPeriod = period > 0.0f ? period : 0.0f;
    return *this;
  }
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
                int octaves = 1, float falloff = 0.5f) {
    m_b.wiggleAmount = amount;
    m_b.wiggleFrequency = frequency;
    m_b.wiggleSeed = seed;
    m_b.wiggleOctaves = octaves < 1 ? 1 : (octaves > 8 ? 8 : octaves);
    m_b.wiggleFalloff =
        falloff < 0.0f ? 0.0f : (falloff > 1.0f ? 1.0f : falloff);
    return *this;
  }
  /** Bound the OUTPUT; always applied last, whenever it is written. */
  Bound& clamp(float lo, float hi) {
    m_b.clamped = true;
    m_b.lo = lo;
    m_b.hi = hi;
    return *this;
  }

  const BoundFloat& value() const { return m_b; }

 private:
  BoundFloat m_b;
};

/** `bind(&output)` — a binding you can shape. `&output` on its own still
 *  works and stays the zero-overhead form. */
inline Bound bind(const choreograph::Output<float>* source) {
  return Bound{source};
}

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
inline Bound wiggle(const choreograph::Output<float>* source,
                    float amount = 1.0f, float frequency = 2.0f,
                    uint32_t seed = 0, int octaves = 1, float falloff = 0.5f) {
  Bound b{source};
  b.scale(0.0f).wiggle(amount, frequency, seed, octaves, falloff);
  return b;
}
/**
 * An ANIMATABLE property value — the slot type behind every property
 * that can move. It holds exactly one of: a constant, a constant with
 * its own transition, a live Choreograph binding, or that binding shaped
 * through `bind()`. A constant is animatable too; the name says what the
 * slot CAN hold, not what it is doing.
 *
 * The caller owns any bound `Output` and the clock that steps it. A slot
 * outliving the Output it points at dangles.
 *
 * Stored compactly rather than as a variant: `Transitioned<T>` is the fat
 * form — spec, entrance value and waypoint list — while most properties
 * on most objects are plain constants, so both fat forms share one
 * out-of-line block and the slot itself stays small. Consumers that carry
 * many of these per object depend on that; do not inline the payload.
 */
template <typename T>
class Animatable {
 public:
  Animatable() = default;
  Animatable(T v) : m_plain(std::move(v)) {}
  Animatable(Transitioned<T> t) : m_kind(Kind::kAnim) {
    extra().anim = std::move(t);
  }
  Animatable(const choreograph::Output<T>* bound)
      : m_kind(Kind::kBound), m_bound(bound) {}
  /** bind(&out).…  — a shaped binding. Float properties only; the extra
   *  block is the same one the transitioned form allocates, so this adds
   *  nothing to sizeof(Animatable) and nothing to a slot that never uses
   *  it. */
  Animatable(const Bound& b) : m_kind(Kind::kBoundMapped) {
    m_bound = b.value().source;
    extra().bound = b.value();
  }
  Animatable(const Animatable& other) { *this = other; }
  Animatable(Animatable&&) noexcept = default;
  Animatable& operator=(const Animatable& other) {
    if (this == &other) return *this;
    m_kind = other.m_kind;
    m_plain = other.m_plain;
    m_bound = other.m_bound;
    m_extra = other.m_extra ? std::make_unique<Extra>(*other.m_extra) : nullptr;
    return *this;
  }
  Animatable& operator=(Animatable&&) noexcept = default;

  /** Which form holds: 0 plain, 1 transitioned, 2 bound, 3 shaped
   *  binding.
   *
   *  A stable discriminant and nothing more: any consumer comparing two
   *  animatable values needs one. The numbering is part of the public
   *  behaviour — a shaped binding sorts AFTER a bare one rather than
   *  taking its place — so append new forms at the end and never
   *  renumber. */
  int index() const { return (int)m_kind; }
  const T* plain() const { return m_kind == Kind::kPlain ? &m_plain : nullptr; }
  const Transitioned<T>* transitioned() const {
    return m_kind == Kind::kAnim ? &m_extra->anim : nullptr;
  }
  /** The bound Output, shaped or not, so a consumer asking only "is this
   *  driven live?" reads one accessor and does not have to know which of
   *  the two bound forms it holds. */
  const choreograph::Output<T>* binding() const {
    return m_kind == Kind::kBound || m_kind == Kind::kBoundMapped ? m_bound
                                                                  : nullptr;
  }
  /** The shaping, if this binding has any. */
  const BoundFloat* boundMap() const {
    return m_kind == Kind::kBoundMapped ? &m_extra->bound : nullptr;
  }

 private:
  enum class Kind : uint8_t { kPlain, kAnim, kBound, kBoundMapped };
  /** The out-of-line block for the two FAT forms. They are mutually
   *  exclusive, so one pointer carries both and a slot holding neither
   *  allocates nothing at all. */
  struct Extra {
    Transitioned<T> anim{};
    BoundFloat bound{};
  };
  Extra& extra() {
    if (!m_extra) m_extra = std::make_unique<Extra>();
    return *m_extra;
  }

  Kind m_kind = Kind::kPlain;
  T m_plain{};
  const choreograph::Output<T>* m_bound = nullptr;
  std::unique_ptr<Extra> m_extra;
};

}  // namespace sigil::motion
