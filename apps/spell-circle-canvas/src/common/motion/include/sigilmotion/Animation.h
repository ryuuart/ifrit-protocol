#pragma once

/** @file
 * ANIMATION VALUES — the describable side of motion, with no renderer in
 * it: a transition spec, the house easing curves as EaseFn values, the
 * `animate(from(a).to(b))` / `animate(through({…}))` keyframe builders,
 * `bind(&output)` — a live choreograph::Output shaped through
 * normalise → curve → affine chain on its way to whatever consumes it —
 * and `Animatable<T>`, the property SLOT that holds any one of them.
 *
 * These lived in <sigilcompose/Compose.h> until 2026-07-29. Nothing here
 * touches Skia, Yoga, or the compose kernel — it is choreograph, chrono
 * and float math — so it sits with FrameClock/Ticker in SigilMotion,
 * where SigilWorld and SigilShape can reach it without swallowing a
 * drawing library. SigilCompose re-exports the whole surface into
 * `sigil::compose`, so authored code keeps its spelling (see the
 * re-export block in Compose.h and ROADMAP §37).
 *
 * SigilMotion supplies the VALUES and the clock; the consumer supplies
 * the property slot that stores one and the runtime that reads it each
 * frame (compose's Animatable/Composer is one such consumer).
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

/** How a reconciled property change animates instead of snapping.
 *  `delay` holds the CURRENT value (the `from`, for animate() entrances)
 *  before the ramp starts — the stagger primitive: describe a battery of
 *  children whose delays step by 60–90ms and the cascade is data, not
 *  bookkeeping (the §8 stagger law at the Element level). */
struct Transition {
  std::chrono::milliseconds duration{250};
  choreograph::EaseFn ease = &choreograph::easeOutQuad;
  std::chrono::milliseconds delay{0};

  /** `{360ms, {}, 220ms}` is the obvious way to write "the default curve,
   *  but I need to name the delay" — and because Transition is an
   *  aggregate, that `{}` initialises `ease` to an EMPTY std::function,
   *  which compiles fine and throws `bad_function_call` on the first
   *  frame. (It took down a gallery scene that way.) Every read of the
   *  curve goes through here, so `{}` means what the author meant. */
  const choreograph::EaseFn &easing() const {
    static const choreograph::EaseFn kDefault = &choreograph::easeOutQuad;
    return ease ? ease : kDefault;
  }
};

/** The house curves, as EaseFn VALUES.
 *
 *  `Transition::ease` holds a `choreograph::EaseFn` (float→float), and
 *  choreograph's most useful curves — back, elastic, bounce — take a
 *  shape parameter with a default, so `&choreograph::easeOutBack` will
 *  not convert and the error is a wall of overload-resolution noise. Every
 *  overshoot entrance in the gallery hit this and every one of them
 *  silently settled for easeOutQuint instead. These bind the parameter:
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
} // namespace ease

template <typename T> struct Transitioned {
  /** Value-initialized, not default-initialized: `animate(through({}))`
   *  builds one of these and then fills nothing, and for a scalar T that
   *  left the property reading whatever was on the stack (§32 review
   *  REV-11). An empty keyframe list is a degenerate ask, but it must be
   *  a DETERMINATE one — zero. */
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
 *  exist so the call site reads `animate(from(a).to(b), spec)`. In
 *  path-heavy code a local named `from` (or `to`) shadows the factory —
 *  qualify `compose::from(...)` at those sites (sigillum_aemeth does,
 *  twice).
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
template <typename T> struct FromTo {
  T from;
  T to;
};
/** The ramp-on-change argument: `animate(to(v), spec)`. */
template <typename T> struct To {
  T value;
};
template <typename T> struct From {
  T value;
  FromTo<T> to(T target) { return {std::move(value), std::move(target)}; }
};
template <typename T> From<T> from(T v) { return {std::move(v)}; }
template <typename T> To<T> to(T v) { return {std::move(v)}; }

template <typename T> struct Waypoints {
  std::vector<std::pair<std::chrono::milliseconds, T>> frames;
};

/** The waypoint path, as a float list needing no template argument — a
 *  nested braced list is a non-deduced context, which is why the generic
 *  form below has to be told `<float>`. Every waypoint path in the corpus
 *  is float. */
inline Waypoints<float>
through(std::initializer_list<std::pair<std::chrono::milliseconds, float>>
            frames) {
  return {{frames.begin(), frames.end()}};
}
/** Any other value type — `through<SkColor4f>({...})`. */
template <typename T>
Waypoints<T>
through(std::vector<std::pair<std::chrono::milliseconds, T>> frames) {
  return {std::move(frames)};
}

/** COMPOSER-MANUFACTURED motion on a property: the composer runs the
 *  clock, as against a bound Output, where you do. The first word at the
 *  call site names the OWNER of the motion (the §32 grammar).
 *
 *      .opacity(animate(from(0.0f).to(1.0f), {400ms}))
 *      .scale(animate(from(0.86f).to(1.0f), {520ms, ease::outBack()}))
 *
 *  Both forms are MOUNT choreography (CSS animation-on-enter / GSAP
 *  from() and keyframes): the path plays when the node first appears,
 *  and afterwards the property behaves exactly like `animate(to(v), spec)`
 *  — later changes retarget from the CURRENT value, and re-describing the
 *  same animate() prunes clean (an entrance is a mount thing, not a
 *  per-render restart). staggerChildren() adds order·each to every
 *  entrance in a child subtree, on top of Transition::delay.
 *  snapshot()/measure() ignore entrances — a bake renders the settled
 *  value.
 *
 *  from→to works on every float slot (opacity/transforms/skew/trim/glyph
 *  progress) and on color fills (a mount-time color sweep). `spec`
 *  defaults to the house Transition (250 ms, easeOutQuad); name a spec
 *  when the beat matters. */
template <typename T>
Transitioned<T> animate(FromTo<T> ft, Transition spec = {}) {
  return {std::move(ft.to), std::move(spec), std::move(ft.from)};
}

/** RAMP ON CHANGE — the per-property override of the node-level
 *  `.transition(spec)` policy:
 *
 *      .opacity(animate(to(dimmed ? 0.4f : 1.0f), {180ms}))
 *
 *  No entrance: the node mounts holding the value. Every LATER describe
 *  that carries a different value ramps to it over `spec` instead of
 *  snapping, retargeting from wherever the property currently is (one
 *  motion per (instance, property) — a change mid-ramp bends the ramp, it
 *  does not queue a second). */
template <typename T> Transitioned<T> animate(To<T> t, Transition spec = {}) {
  return {std::move(t.value), std::move(spec)};
}

/** The keyframe path: absolute (time, value) waypoints played through on
 *  first appearance — the damped-overshoot entrances one from→to ramp
 *  can't shape (P3R's cursor: +40 → −20 → +10 → 0):
 *
 *    .translateX(animate(through(
 *        {{0ms, 40.f}, {200ms, -20.f}, {300ms, 10.f}, {400ms, 0.f}})))
 *
 *  `ease` applies PER SEGMENT. A leading time > 0 holds the first value
 *  (a built-in delay; Transition::delay and staggerChildren() still add
 *  on top). After the path completes the value behaves like
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

/** A live binding, SHAPED on its way to the property.
 *
 *  A bare `&output` binding lands on the property RAW, which was the
 *  most-cited wall in the library: a phase that lives in [0,1] — what
 *  trim(), opacity() and every progress want — could not drive a
 *  translation in pixels without a SECOND Output carrying pixels, updated
 *  in the same steppable. Five independent studies arrived here from five
 *  unrelated directions (a shimmer band, a health bar, a lattice
 *  assembling, a pen tip trailing a drawn curve, a card entrance), and
 *  every one of them paid for it in duplicated Outputs and easing written
 *  in the tick loop, far from the property it shapes.
 *
 *      .translateX(bind(&phase).target(-70, 170))
 *      .opacity(bind(&progress).map(ease::outBack()).clamp(0, 1))
 *      .scaleX(bind(&hp).source(0, maxHp))
 *
 *  Three stages, always in this order:
 *
 *    1. `source(lo, hi)` normalises the SOURCE range onto [0,1];
 *    2. `map(ease)` shapes it (any `choreograph::EaseFn`, so the whole
 *       `ease::` namespace and every choreograph curve fits);
 *    3. the affine chain — `scale`/`offset`/`target`/`invert` — composes
 *       in CALL ORDER, so `.scale(240).offset(-70)` is `v*240 - 70` and
 *       `.offset(-70).scale(240)` is `(v-70)*240`, each reading the way
 *       it looks. `clamp` always applies last.
 *
 *  Costs nothing a bare binding does not: still paint-only, still read
 *  through the pointer each frame, still no relayout. sizeof(Animatable)
 *  is unchanged — the map rides the same out-of-line block the
 *  transitioned form already allocates. */
struct BoundFloat {
  const choreograph::Output<float> *source = nullptr;
  float inScale = 1.0f, inOffset = 0.0f; // from(): pre-curve normalisation
  choreograph::EaseFn curve;             // map()
  bool clampInput = false;               // window(): clamp before the curve
  int steps = 0;                         // quantize(): 0 = continuous
  float scale = 1.0f, offset = 0.0f;     // the affine chain
  bool clamped = false;
  float lo = 0.0f, hi = 1.0f;

  float apply(float v) const {
    v = v * inScale + inOffset;
    if (clampInput)
      v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    if (curve)
      v = curve(v);
    if (steps > 1)
      v = std::round(v * (float)(steps - 1)) / (float)(steps - 1);
    v = v * scale + offset;
    if (clamped)
      v = v < lo ? lo : (v > hi ? hi : v);
    return v;
  }
};

/** Builder for BoundFloat — see the doc above. Converts implicitly into
 *  any `Animatable<float>` property. */
class Bound {
public:
  // The parameter is `out`, not `source`, because source() is now a stage
  // name on this class and a parameter of that name would shadow it.
  explicit Bound(const choreograph::Output<float> *out) { m_b.source = out; }

  /** Normalise the SOURCE's own range onto [0,1] before everything else.
   *  `bind(&hp).source(0, maxHp)` is the health-bar spelling.
   *
   *  Named for the STAGE, not for a direction: a bound chain has two
   *  ranges, the one the Output speaks and the one the property wants,
   *  and `from`/`to` made them read as the endpoints of a single ramp —
   *  the authored `animate(from(a).to(b))` words, meaning something else
   *  entirely (ROADMAP §33 ruling 3). The authored from/to keep their
   *  words; the stages get theirs. */
  Bound &source(float lo, float hi) {
    const float span = hi - lo;
    m_b.inScale = span != 0.0f ? 1.0f / span : 0.0f;
    m_b.inOffset = span != 0.0f ? -lo / span : 0.0f;
    return *this;
  }
  /** `source()` that also CLAMPS the normalised value to [0,1].
   *
   *  A window is the whole point of `source(lo, hi)` on a multi-beat
   *  timeline, and the curve runs after it — so with plain `source()` an
   *  Output outside the window feeds the easing a value outside its
   *  domain, and none of `ease::` is total. `.map(ease::outBack())` on a
   *  five-beat sequence is a trap for exactly that reason. `window()` is
   *  `source()` for the case where you meant "this beat and nothing
   *  else", which is nearly always. */
  Bound &window(float lo, float hi) {
    source(lo, hi);
    m_b.clampInput = true;
    return *this;
  }
  /** Shape the (normalised) value — any choreograph easing, including the
   *  parameterised `ease::` family. */
  Bound &map(choreograph::EaseFn curve) {
    m_b.curve = std::move(curve);
    return *this;
  }
  Bound &scale(float s) {
    m_b.scale *= s;
    m_b.offset *= s;
    return *this;
  }
  Bound &offset(float o) {
    m_b.offset += o;
    return *this;
  }
  /** Map [0,1] onto the TARGET range [lo,hi] — `scale(hi-lo).offset(lo)`,
   *  spelled the way you think about it. The other half of the stage
   *  rename (see source()). */
  Bound &target(float lo, float hi) { return scale(hi - lo).offset(lo); }
  /** 1 − v, composed properly with whatever came before. */
  Bound &invert() {
    m_b.scale = -m_b.scale;
    m_b.offset = 1.0f - m_b.offset;
    return *this;
  }
  /** Snap to @p steps discrete levels across [0,1] BEFORE the affine
   *  chain — the period-authentic move, not an approximation of one.
   *  Winamp's volume slider is literally `round(percent * 28)` and its
   *  28 sprite frames are what a user of it remembers; a smooth slider
   *  sampled at draw time is a different widget. */
  Bound &quantize(int steps) {
    m_b.steps = steps > 1 ? steps : 0;
    return *this;
  }
  /** Bound the OUTPUT; always applied last, whenever it is written. */
  Bound &clamp(float lo, float hi) {
    m_b.clamped = true;
    m_b.lo = lo;
    m_b.hi = hi;
    return *this;
  }

  const BoundFloat &value() const { return m_b; }

private:
  BoundFloat m_b;
};

/** `bind(&output)` — a binding you can shape. `&output` on its own still
 *  works and stays the zero-overhead form. */
inline Bound bind(const choreograph::Output<float> *source) {
  return Bound{source};
}
/**
 * An ANIMATABLE property value — the slot type behind every property
 * that can move. It accepts one of: a constant (snaps, or ramps under a
 * node-level transition), a constant with its own transition, a live
 * Choreograph binding stepped by the Ticker (paint-only; the caller
 * owns the Output and composes motions on ticker.timeline()), or that
 * binding shaped through bind(). A constant is animatable too — the
 * name says what the slot CAN do, not what it is doing.
 *
 * Stored compactly (this used to be a std::variant): Transitioned<T> is
 * the fat form — from/waypoints/spec, ~100 B for a float — and most
 * properties on most nodes are plain constants, so the transitioned
 * payload lives out-of-line. The compaction was driven by SigilCompose,
 * where eight Animatable<float>s ride every node's PaintProps and the
 * block-split rule took it from 856 B to ~250 B; those numbers are the
 * EVIDENCE for the layout, not a dependency on it, and the layout is
 * unchanged by the move here (sizeof(Animatable<float>) == 24 on
 * arm64-osx, before and after — ROADMAP §37).
 */
template <typename T> class Animatable {
public:
  Animatable() = default;
  Animatable(T v) : m_plain(std::move(v)) {}
  Animatable(Transitioned<T> t) : m_kind(Kind::kAnim) {
    extra().anim = std::move(t);
  }
  Animatable(const choreograph::Output<T> *bound)
      : m_kind(Kind::kBound), m_bound(bound) {}
  /** bind(&out).…  — a shaped binding. Float properties only; the extra
   *  block is the same one the transitioned form allocates, so this adds
   *  nothing to sizeof(Animatable) and nothing to a node that never uses
   *  it. */
  Animatable(const Bound &b) : m_kind(Kind::kBoundMapped) {
    m_bound = b.value().source;
    extra().bound = b.value();
  }
  Animatable(const Animatable &other) { *this = other; }
  Animatable(Animatable &&) noexcept = default;
  Animatable &operator=(const Animatable &other) {
    m_kind = other.m_kind;
    m_plain = other.m_plain;
    m_bound = other.m_bound;
    m_extra = other.m_extra ? std::make_unique<Extra>(*other.m_extra) : nullptr;
    return *this;
  }
  Animatable &operator=(Animatable &&) noexcept = default;

  /** Which form holds: 0 plain, 1 transitioned, 2 bound, 3 shaped
   *  binding.
   *
   *  This is the ORDER THE OLD std::variant HAD, preserved through the
   *  compaction below so that a shaped binding sorts after a bare one
   *  rather than replacing it. It is a stable DISCRIMINANT and nothing
   *  more — any consumer diffing two animatable values wants one, and
   *  compose's reconciler (propEqual, Reconcile.cpp) is merely the first
   *  such consumer. An earlier reading took "for the reconciler's
   *  compare" to mean this type encodes kernel semantics and therefore
   *  could never leave SigilCompose; it does not, and it did (ROADMAP
   *  §37). Do not re-derive that conclusion from this comment. */
  int index() const { return (int)m_kind; }
  const T *plain() const {
    return m_kind == Kind::kPlain ? &m_plain : nullptr;
  }
  const Transitioned<T> *transitioned() const {
    return m_kind == Kind::kAnim ? &m_extra->anim : nullptr;
  }
  /** The bound Output, shaped or not — so every volatility check, every
   *  "bound ⇒ snap" branch and the reconciler's pointer compare go on
   *  reading one accessor. */
  const choreograph::Output<T> *binding() const {
    return m_kind == Kind::kBound || m_kind == Kind::kBoundMapped ? m_bound
                                                                  : nullptr;
  }
  /** The shaping, if this binding has any. */
  const BoundFloat *boundMap() const {
    return m_kind == Kind::kBoundMapped ? &m_extra->bound : nullptr;
  }

private:
  enum class Kind : uint8_t { kPlain, kAnim, kBound, kBoundMapped };
  /** The out-of-line block for the two FAT forms. They are mutually
   *  exclusive, so one pointer carries both and Animatable stays the
   *  size it was compacted to (ElementNode 1288 B → 688 B). */
  struct Extra {
    Transitioned<T> anim{};
    BoundFloat bound{};
  };
  Extra &extra() {
    if (!m_extra)
      m_extra = std::make_unique<Extra>();
    return *m_extra;
  }

  Kind m_kind = Kind::kPlain;
  T m_plain{};
  const choreograph::Output<T> *m_bound = nullptr;
  std::unique_ptr<Extra> m_extra;
};

} // namespace sigil::motion
