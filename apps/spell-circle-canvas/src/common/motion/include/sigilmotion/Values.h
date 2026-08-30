#pragma once

/** @file
 * ANIMATION VALUES — the describable side of motion, with no renderer in
 * it: a transition spec, the house easing curves as EaseFn values, the
 * `animate(from(a).to(b))` / `animate(through({…}))` keyframe builders,
 * and `Animatable<T>`, the property SLOT that holds any one of them or a
 * shaped `bind()` binding.
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
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "sigilmotion/Bind.h"

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
/** A value together with how it should move when it changes. Assigning
 *  a new value does not jump to it: the transition spec says how to get
 *  there, and the optional entrance and waypoint paths say what happens
 *  the first time the value is mounted. */
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
/** The half-built entrance argument: `from(a)` on its own is not yet an
 *  animation, and `.to(b)` completes it. */
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
/** The mount-time path argument: `animate(through({...}))`. Each frame
 *  pairs an ABSOLUTE time from the start of the path with the value to
 *  hold at that time. */
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

// ---------------------------------------------------------------------------
// Two arithmetic helpers over the clock and the transition

/** A delayed ramp, in MILLISECONDS as floats.
 *
 *  Float ms rather than `std::chrono::milliseconds` on purpose: a staggered
 *  reveal computes its delay arithmetically
 *  (`ramp(tTicks * 1000 + 300 + i * 25, 400)`), and a chrono parameter
 *  would put a cast at every such site. `Transition{.duration = 400ms}`
 *  remains the spelling wherever the numbers are literals. */
inline Transition ramp(float delayMs, float durationMs,
                       choreograph::EaseFn ease = &choreograph::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durationMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(ease);
  return t;
}

/** A wrapping phase in [0, 1): `t` seconds over a `period`-second loop —
 *  the marching-ants offset, the orbiting comet, the scrolling marquee,
 *  the scanline creep.
 *
 *  A non-positive period gives 0 rather than the NaN the bare `fmod` would
 *  produce, and a negative `t` wraps forward instead of returning a
 *  negative phase, so the result is always in range whatever the caller
 *  hands in.
 *
 *  Deliberately narrow. The two neighbouring signals, `0.5 + 0.5·sin(t·k)`
 *  and `min(1, t/k)`, are one short expression each and are not here. */
inline float phase(double t, double period) {
  if (!(period > 0)) return 0.0f;
  const double p = std::fmod(t / period, 1.0);
  return (float)(p < 0 ? p + 1.0 : p);
}

}  // namespace sigil::motion
