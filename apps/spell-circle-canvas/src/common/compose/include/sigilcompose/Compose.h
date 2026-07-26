#pragma once

/** @file
 * SigilCompose kernel — data-driven, cacheable, animated drawable
 * components for any SkCanvas. See DESIGN.md (architecture), API.md
 * (surface rationale), STRESS_TESTS.md (acceptance catalog).
 *
 * The kernel is: Element descriptions built by fluent value builders,
 * component functions over plain data (+ memo), and a Composer that
 * reconciles by key, lays out via Yoga (SigilWeave-measured text leaves),
 * paints with explicit stacking, caches provably-static subtrees as
 * SkPictures automatically, and animates through Choreograph driven by
 * an sigil::motion::Ticker.
 */

#include <sigilmotion/FrameClock.h>
#include <sigilmotion/Ticker.h>

#include <sigilweave/ParagraphLayout.h>
#include <sigilweave/Style.h>

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkPath.h>
#include <include/core/SkShader.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>

#include <any>
#include <chrono>
#include <cmath>
#include <concepts>
#include <ranges>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class SkCanvas;
class SkImageFilter;
class SkPicture;
class SkRuntimeEffect;

namespace sigil::image {
class ImageAsset;
}

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose {

namespace detail {
struct ElementNode;
struct Instance;
} // namespace detail

// The polymorphic paint value (<sigilcompose/Material.h>) — supersedes Fill as
// the authoring value for fill(); a static Material collapses to a Fill.
class Material;

// ---------------------------------------------------------------------------
// Animation values

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
   *  and `withKeyframes<float>({})` build one of these and then fill
   *  nothing, and for a scalar T that left the property reading whatever
   *  was on the stack (§32 review REV-11). An empty keyframe list is a
   *  degenerate ask, but it must be a DETERMINATE one — zero. */
  T value{};
  Transition spec;
  /** animate(from(a).to(b)): where the value ENTERS from when the node
   *  first mounts. Empty for plain with() — no entrance, only change
   *  transitions. */
  std::optional<T> from{};
  /** animate(through({...})): the mount-time path as (absolute time,
   *  value) pairs — multi-segment entrances (damped overshoots) that one
   *  from→to ramp can't shape. Mount-only choreography: later changes
   *  retarget to `value` like any with(). */
  std::vector<std::pair<std::chrono::milliseconds, T>> waypoints{};
};

/** Legacy spelling of `animate(to(v), spec)` — retained until the R3
 *  deletion (ROADMAP §33). Wraps a constant so changes to it transition
 *  (see API.md semantics: one motion per (instance, property),
 *  retarget-from-current). */
template <typename T> Transitioned<T> with(T value, Transition spec) {
  return {std::move(value), std::move(spec)};
}

/** Legacy spelling of animate(from(a).to(b), spec) — retained until the R3
 *  deletion (ROADMAP §33); new code and new signatures spell the intent. */
template <typename T> Transitioned<T> withFrom(T from, T to, Transition spec) {
  return {std::move(to), std::move(spec), std::move(from)};
}

/** Legacy spelling of animate(through({...}), ease) — retained until the R3
 *  deletion (ROADMAP §33); new code and new signatures spell the intent. Needs its
 *  explicit `<float>`, which is the deduction wall through() removes. */
template <typename T>
Transitioned<T>
withKeyframes(std::vector<std::pair<std::chrono::milliseconds, T>> frames,
              choreograph::EaseFn ease = &choreograph::easeOutQuad) {
  Transitioned<T> t;
  t.spec.ease = std::move(ease);
  if (!frames.empty()) {
    t.from = frames.front().second;
    t.value = frames.back().second;
    t.spec.duration = frames.back().first;
  }
  t.waypoints = std::move(frames);
  return t;
}

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
 *  form below (and withKeyframes before it) has to be told `<float>`.
 *  Every waypoint path in the corpus is float. */
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
 *  and afterwards the property behaves exactly like with(to, spec) —
 *  later changes retarget from the CURRENT value, and re-describing the
 *  same animate() prunes clean (an entrance is a mount thing, not a
 *  per-render restart). staggerChildren() adds order·each to every
 *  entrance in a child subtree, on top of Transition::delay.
 *  snapshot()/measure() ignore entrances — a bake renders the settled
 *  value.
 *
 *  from→to works on every float slot (opacity/transforms/skew/trim/glyph
 *  progress) and on color fills (a mount-time color sweep). `spec`
 *  defaults to the house Transition (250 ms, easeOutQuad) — an
 *  affordance withFrom never had; name a spec when the beat matters. */
template <typename T>
Transitioned<T> animate(FromTo<T> ft, Transition spec = {}) {
  return withFrom(std::move(ft.from), std::move(ft.to), std::move(spec));
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
 *  does not queue a second). Identical to `with(v, spec)`, which is the
 *  legacy spelling this replaces. */
template <typename T> Transitioned<T> animate(To<T> t, Transition spec = {}) {
  return with(std::move(t.value), std::move(spec));
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
 *  on top). After the path completes the value behaves like with(last). */
template <typename T>
Transitioned<T> animate(Waypoints<T> w,
                        choreograph::EaseFn ease = &choreograph::easeOutQuad) {
  return withKeyframes<T>(std::move(w.frames), std::move(ease));
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
 *  `from`/`to` are the legacy spellings of stages 1 and 3 (ROADMAP §33
 *  ruling 3: they collided with the authored `from(a).to(b)` ramp, which
 *  means something else); they compile until R3.
 *
 *  Costs nothing a bare binding does not: still paint-only, still read
 *  through the pointer each frame, still no relayout. sizeof(PropValue)
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
 *  any `PropValue<float>` property. */
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
  /** Legacy spelling of source(lo, hi) — dies in the R3 deletion. */
  Bound &from(float lo, float hi) { return source(lo, hi); }
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
  /** Legacy spelling of target(lo, hi) — dies in the R3 deletion. */
  Bound &to(float lo, float hi) { return target(lo, hi); }
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
 * (`PropValue` is the legacy spelling, kept as an alias below — the
 *  grammar rule is DESIGN.md's: names say intent, not mechanism.)
 *
 * Stored compactly (this used to be a std::variant): Transitioned<T> is
 * the fat form — from/waypoints/spec, ~100 B for a float — and most
 * properties on most nodes are plain constants, so the transitioned
 * payload lives out-of-line. Eight Animatable<float>s ride every node's
 * PaintProps; this is the ElementNode block-split rule applied to the
 * property type itself (856 B -> ~250 B of PaintProps).
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
   *  nothing to sizeof(PropValue) and nothing to a node that never uses
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

  /** Which form holds (0 plain, 1 transitioned, 2 bound, 3 shaped
   *  binding — the old variant's index order, for the reconciler's
   *  compare, which is why a shaped binding sorts after a bare one rather
   *  than replacing it). */
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

/** Legacy spelling of Animatable — retained until the R3 deletion (ROADMAP §33); new code and
 *  new signatures spell the intent. */
template <typename T> using PropValue = Animatable<T>;

// ---------------------------------------------------------------------------
// Paint values

/** A paint slot: nothing, a color, or anything Skia can shade (gradient
 *  helpers live in util, SkSL via SkRuntimeEffect works here). */
struct Fill {
  enum class Kind : uint8_t { None, Color, Shader };

  static Fill color(SkColor4f c) { return {Kind::Color, c, nullptr}; }
  static Fill shader(sk_sp<SkShader> s);
  static Fill none() { return {}; }

  Kind kind = Kind::None;
  SkColor4f colorValue = {0, 0, 0, 0};
  sk_sp<SkShader> shaderValue;

  bool operator==(const Fill &o) const {
    return kind == o.kind && colorValue == o.colorValue &&
           shaderValue == o.shaderValue;
  }
};

/** Corner radii, clockwise from top-left. `{r}` rounds all four; the
 *  four-value form dresses each corner independently. For shapes whose
 *  corners aren't box corners (stars, polygons, custom outlines), use
 *  shapes::rounded() around the outline generator instead. */
struct Corners {
  float topLeft = 0.0f, topRight = 0.0f, bottomRight = 0.0f,
        bottomLeft = 0.0f;

  Corners() = default;
  Corners(float all) // NOLINT: implicit by design (.corners({8}))
      : topLeft(all), topRight(all), bottomRight(all), bottomLeft(all) {}
  Corners(float tl, float tr, float br, float bl)
      : topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {}

  bool any() const {
    return topLeft > 0 || topRight > 0 || bottomRight > 0 || bottomLeft > 0;
  }
  bool operator==(const Corners &) const = default;
};

/** The one paint-program context: custom leaves (and, in extensions,
 *  decorations and contour walks) all receive this. `elapsedSeconds` is
 *  the Ticker's FrameClock time — pause/time-scale affect it. `fonts`
 *  is the owning composer's FontContext (null only when a decoration
 *  is painted outside a composer) — what element stamps and ad-hoc
 *  SigilWeave drawing inside paint programs lay text out with. */
struct PaintContext {
  SkSize size = SkSize::MakeEmpty();
  SkPath outline;
  double elapsedSeconds = 0.0;
  float contentScale = 1.0f;
  /** Is the composer's Ticker running anything at all this frame, as
   *  read by a node that REPAINTS this frame (a cached node replays its
   *  recording and keeps its last-read value) — the
   *  WHOLE tree's answer, not this node's. A program that wants cheap
   *  chrome while something moves reads it; nothing in the library does.
   *  False outside a composer (a decoration painted standalone), which is
   *  the honest answer there: there is no ticker to be active. */
  bool animating = false;
  sigil::weave::FontContext *fonts = nullptr;
  /** Paths this node BORROWED from keyed elements in the derive phase, in
   *  its own local space — what `strand::from(key)` reads. Null outside a
   *  composer, or when the node borrowed nothing. Non-owning: valid for
   *  the duration of the paint call only.
   *
   *  A decoration declares what it borrows (see BorrowingDecoration
   *  below) so the element can register the keys without introspecting a
   *  type-erased value; the derive pass then resolves them on the same
   *  flat edge-store walk connectors and flowAround ride. */
  const std::vector<std::pair<std::string, SkPath>> *borrowed = nullptr;

  /** The borrowed path for `key`, or an empty path. */
  SkPath borrowedPath(const std::string &key) const {
    if (borrowed)
      for (const auto &[k, p] : *borrowed)
        if (k == key)
          return p;
    return SkPath();
  }
};

using PaintProgram = std::function<void(SkCanvas &, const PaintContext &)>;

/**
 * Post-processing at stacking-context boundaries. `filter` wraps any
 * SkImageFilter (blur, displacement, lighting, compose chains);
 * `shader` wraps an SkSL runtime effect whose child shader is the
 * rendered layer. Attach with Element::effect() (the node's own layer)
 * or Element::backdrop() (what's already painted beneath the node's
 * bounds). Under Cache::Texture an effect() is baked into the snapshot
 * — expensive filters on static content are paid once.
 */
class Effect {
public:
  static Effect filter(sk_sp<SkImageFilter> f);
  /** @p uniforms are float uniforms set by name on the SkSL effect;
   *  the layer arrives as the child shader named "content". */
  static Effect shader(sk_sp<SkRuntimeEffect> effect,
                       std::vector<std::pair<std::string, float>> uniforms = {});
  /** Chain: apply `next` AFTER this effect (SkImageFilters::Compose) —
   *  e.g. the DWM glass formula: Effect::filter(Blur(3,3)).then(
   *  Effect::shader(colorize)). */
  Effect then(const Effect &next) const;

  const sk_sp<SkImageFilter> &imageFilter() const { return m_filter; }

private:
  sk_sp<SkImageFilter> m_filter;
};

// ---------------------------------------------------------------------------
// Kinetic typography (the per-glyph seam; presets in <sigilcompose/Kinetic.h>)

/** What a glyph effect sees for one glyph. Enumeration order is stable
 *  across relayouts while the text is unchanged (SigilWeave contract). */
struct GlyphInfo {
  size_t index = 0;   ///< glyph position in the paragraph
  size_t count = 1;   ///< total glyphs
  SkPoint rest;       ///< the glyph's laid-out origin (pen position)
  float advance = 0;  ///< the glyph's advance width
  float fontSize = 0; ///< the glyph's font size (em-relative effects)
};

/** One glyph's deviation from rest — what an effect returns for local
 *  progress t ∈ [0,1]. alpha 0 skips the glyph entirely. */
struct GlyphMod {
  float dx = 0, dy = 0;
  float scale = 1;
  float rotateDeg = 0;
  float alpha = 1;
};

/** A pure value: (glyph, local progress) → deviation. Compose freely. */
using GlyphEffectFn = std::function<GlyphMod(const GlyphInfo &, float)>;

/** The per-glyph time remap (the GSAP stagger model): the element's master
 *  progress [0,1] spans `durationMs + eachMs·(N−1)` of virtual time; glyph i
 *  starts after its delay and runs for durationMs. */
struct Stagger {
  float eachMs = 30;
  /** GSAP amount-mode (XOR with eachMs; wins when > 0): the TOTAL spread
   *  divided across all glyphs — the §8 budget law ("entrances ≤ 1.2s")
   *  as a constant that survives copy changes. */
  float amountMs = 0;
  float durationMs = 450;
  enum class From : uint8_t { Start, Center, End } from = From::Start;
  bool operator==(const Stagger &) const = default;
};

/** Text whose BASELINE is a path (`Element::onPath`).
 *
 *  The run is shaped once — real kerning, real ligatures, real advances —
 *  and then every glyph is placed by arc length along the resolved path
 *  and rotated to its tangent, through the same batched RSXform draw
 *  kinetic text uses (one draw per font+colour, never one per glyph).
 *
 *  Written because placing curved lettering by hand costs one Element and
 *  one layout PER GLYPH: the Nightingale coxcomb study spent ~230 leaves,
 *  ~230 measure() calls and sixty lines of arc-length trigonometry on its
 *  ring labels, and got no kerning for the trouble. Ring labels, dial
 *  faces, seals, compass roses, mottoes and map lettering all want this. */
struct TextPath {
  /** The baseline, resolved against the node's laid-out box — any
   *  `shapes::` generator, or your own. EVERY contour is walked, in
   *  order, as one arc-length coordinate — a trajectory clipped to the
   *  frame is several contours and used to lose its label silently. */
  std::function<SkPath(SkSize)> path;
  /** Where the run sits along the path, as a fraction of its length.
   *  With Align::Center this is the run's midpoint. */
  float at = 0.0f;
  enum class Align { Start, Center, End };
  Align align = Align::Start;
  /** Perpendicular offset in px, positive to the LEFT of travel — which on
   *  a clockwise circle is outward. The path is the baseline, so this is
   *  how far off it the type rides. */
  float offset = 0.0f;
  /** Flip glyphs that would come out upside down, so lettering on the
   *  lower half of a ring reads right way up.
   *
   *  Default OFF, and that is a considered default: engravers used one
   *  convention — glyph-up points radially outward everywhere — so on
   *  Nightingale's 1858 plate DECEMBER, JANUARY and FEBRUARY are all
   *  genuinely upside down. Modern signage flips; historical plates do
   *  not. */
  bool autoFlip = false;
  /** Which way a glyph faces.
   *
   *  `Tangent` is running lettering: the baseline lies ALONG the path,
   *  which is what a ring inscription or a motto wants. Note this already
   *  gives you "up points outward" on a circle — that is why a clock
   *  face's 6 comes out upside down, and why `autoFlip` exists.
   *
   *  `Radial` runs the baseline along the RADIUS instead, so the type
   *  radiates like a spoke — which is how an astrolabe limb, a compass
   *  rose and a radial axis label their divisions: you turn the
   *  instrument to read them. Without it each numeral costs one rotated
   *  Element, which is exactly the per-glyph cost onPath exists to
   *  abolish, resurfacing for the other half of the problem.
   *
   *  `Upright` leaves every glyph level regardless of where it sits —
   *  the convention a calendar ring or a modern gauge uses, and the one
   *  case neither of the others can reach.
   *
   *  The centre `Radial` radiates from is the resolved baseline's
   *  BOUNDING-BOX centre. That is the true centre for a full ring and
   *  silently wrong for an arc that does not span one — a quarter-arc's
   *  bbox centre is not its circle's centre — so give a partial arc a
   *  full-circle baseline and place the run on it with `at`. */
  enum class Orient { Tangent, Radial, Upright } orient = Orient::Tangent;
  // No operator==: `path` is a std::function, so a defaulted one is
  // implicitly DELETED and compiles quietly while comparing nothing. The
  // reconciler treats a run with a baseline as never-prunable instead
  // (Reconcile.cpp, textEqual) — the same rule the derive callables get.
};

/** Kinetic text: attach to a text() element with Element::glyphFx(). The
 *  master `progress` takes the full PropValue treatment — plain, with()
 *  transitions (retarget-safe), or a ch::Output binding (loops: bind a
 *  wrapping phase). While progress moves the node paints live; settled
 *  kinetic text caches like any static leaf. All glyphs render through
 *  batched RSXform draws — one draw per (font, color), never per glyph. */
struct GlyphFx {
  GlyphEffectFn effect;
  Stagger stagger;
  PropValue<float> progress = 1.0f;
};

/** Anything with paint(canvas, PaintContext) — decorations, effects
 *  bodies. An optional `bool animates() const` declares per-frame
 *  volatility (the single declared-volatility rule). */
template <typename D>
concept DecorationScheme =
    requires(const D &d, SkCanvas &canvas, const PaintContext &ctx) {
      { d.paint(canvas, ctx) };
    };

/** THE VOLATILITY DECLARATION. A scheme that repaints every frame — a
 *  bound dash phase, a live material, a walk keyed to elapsed time — says
 *  so with `bool animates() const`, and the library stops caching its
 *  node's picture. Nothing introspects: a Decoration is type-erased by
 *  the time the composer holds it, so the value must declare.
 *
 *  ONE WORD, five spellings before it (ROADMAP §33 ruling 2): schemes
 *  said `animated()`, Material said `isLive()`, PaintContext said
 *  `animating`, the node-level checks said "volatile". `animates()` is
 *  the primary — it shares a root with `animate()`, and a scheme
 *  DECLARES an activity rather than reporting a state. `animated()` is
 *  accepted for as long as the transition runs (the concept below
 *  duck-types both) and dies in R3; a scheme that spells both is read
 *  through `animates()`. */
template <typename D>
concept AnimatingDecoration = requires(const D &d) {
  { d.animates() } -> std::convertible_to<bool>;
};

/** The legacy half of the volatility concept — see AnimatingDecoration. */
template <typename D>
concept AnimatedDecoration = requires(const D &d) {
  { d.animated() } -> std::convertible_to<bool>;
};

/** Optional on a DecorationScheme: how far it paints BEYOND the node's
 *  bounds (soft shadows, glows). The recording cull grows by the node's
 *  max bleed so cached pictures never truncate overflowing chrome. */
template <typename D>
concept BleedingDecoration = requires(const D &d) {
  { d.bleed() } -> std::convertible_to<float>;
};

/** Optional on a DecorationScheme: the FULL width of the MARK it paints,
 *  across the outline it dresses.
 *
 *  Distinct from `bleed()`, which is the CULL's number — how far the paint
 *  escapes the node's box. An `Align::Inner` stroke bleeds ZERO (it never
 *  leaves the shape) while painting a mark `width` px wide, so a repair
 *  region derived from bleed() was measurably too small. Anything that
 *  needs to know where a mark IS, rather than how far it escapes, asks
 *  this. Over-reporting is safe; under-reporting is not. */
template <typename D>
concept ReachingDecoration = requires(const D &d) {
  { d.reach() } -> std::convertible_to<float>;
};

/** Optional on a DecorationScheme: element keys whose resolved PATHS this
 *  decoration needs (a weave's `strand::from(key)`). The element collects
 *  them at build time and the derive pass answers them into
 *  PaintContext::borrowed — the third borrow on one flat walk, beside
 *  flowAround and connector/rail, rather than a fourth mechanism.
 *
 *  Declared rather than introspected because a Decoration is type-erased
 *  by then: the element cannot look inside the value, so the value says
 *  so. Composites forward their children's keys. */
template <typename D>
concept BorrowingDecoration = requires(const D &d) {
  { d.borrows() } -> std::convertible_to<std::vector<std::string>>;
};

/** Type-erased decoration: the kernel seam extension primitives
 *  (PathFormat, Slice, ContourWalk — see Decorations.h) plug into. A
 *  bare PaintProgram works too. */
class Decoration {
public:
  template <DecorationScheme D>
  Decoration(D scheme) // NOLINT: implicit by design
      : m_animated([&] {
          // animates() wins where a scheme spells both — the library's own
          // schemes forward animated() to it, so the two always agree.
          if constexpr (AnimatingDecoration<D>)
            return scheme.animates();
          else if constexpr (AnimatedDecoration<D>)
            return scheme.animated();
          else
            return false;
        }()),
        m_bleed([&] {
          if constexpr (BleedingDecoration<D>)
            return (float)scheme.bleed();
          else
            return 0.0f;
        }()),
        m_reach([&] {
          if constexpr (ReachingDecoration<D>)
            return (float)scheme.reach();
          else if constexpr (BleedingDecoration<D>)
            return (float)scheme.bleed(); // the best available answer
          else
            return 0.0f;
        }()),
        m_borrows([&]() -> std::vector<std::string> {
          if constexpr (BorrowingDecoration<D>)
            return scheme.borrows();
          else
            return {};
        }()) {
    // Value-comparable schemes (PathFormat, Slice, Shadow…) retain a
    // comparator so the reconciler can prune a static decorated node with no
    // memo (see propsEqual). A non-comparable scheme — or a bare
    // PaintProgram — keeps none and stays conservatively unequal.
    if constexpr (std::equality_comparable<D>) {
      m_scheme = scheme; // retained copy, compared structurally
      m_equals = [](const std::any &a, const std::any &b) {
        return std::any_cast<const D &>(a) == std::any_cast<const D &>(b);
      };
    }
    m_paint = [s = std::move(scheme)](SkCanvas &c, const PaintContext &ctx) {
      s.paint(c, ctx);
    };
  }
  Decoration(PaintProgram program) // NOLINT: implicit by design
      : m_paint(std::move(program)) {}

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if (m_paint)
      m_paint(canvas, ctx);
  }
  /** Declared volatility, read off whichever word the scheme spelled. */
  bool animates() const { return m_animated; }
  /** Legacy spelling of animates() — dies in the R3 deletion. */
  bool animated() const { return animates(); }
  float bleed() const { return m_bleed; }
  /** FULL width of the mark this decoration paints, across the outline it
   *  dresses (see ReachingDecoration). Falls back to bleed(), then to 0. */
  float reach() const { return m_reach; }
  /** Keyed elements whose resolved paths this decoration reads (see
   *  BorrowingDecoration). Empty for everything that borrows nothing. */
  const std::vector<std::string> &borrows() const { return m_borrows; }

  /** Structural equality for the no-memo prune: true only when both wrap the
   *  same value-comparable scheme type and those values compare equal. A bare
   *  PaintProgram or an incomparable scheme always compares unequal —
   *  conservative, matching the rest of the reconciler's equality. */
  bool operator==(const Decoration &o) const {
    return m_equals && o.m_equals && m_scheme.type() == o.m_scheme.type() &&
           m_equals(m_scheme, o.m_scheme);
  }

private:
  bool m_animated = false;
  float m_bleed = 0.0f;
  float m_reach = 0.0f;
  std::vector<std::string> m_borrows;
  PaintProgram m_paint;
  std::any m_scheme;
  std::function<bool(const std::any &, const std::any &)> m_equals;
};

/** A named bundle of decorations applied together — the Photoshop "layer
 *  style" as a value. Presets (styles::aquaGel(), styles::y2kChrome())
 *  return one; Element::style() splices it in: `under` layers paint below
 *  the fill/content (drop shadows, body ramps), `over` layers above
 *  (gloss lenses, bevels, keylines). One call dresses the node. */
struct LayerStyle {
  std::vector<Decoration> under;
  std::vector<Decoration> over;
};

// ---------------------------------------------------------------------------
// The stroke grammar — WHERE a stroke goes (ROADMAP §33, stage one)
//
// The words: SHAPE is the region an element occupies (Element::shape);
// LINE is an element whose shape is an open path; BAND is a derived shape
// around a spine (band(), below); STROKE is the slot that dresses a
// boundary, and BRUSH is what paints. "Frame" and "border" are not
// concepts — they are strokes of a boundary; "bounding box" is
// query-side vocabulary (Composer::bounds), never a shape.

/** One claimed run of a boundary, as fractions of its TOTAL arc length —
 *  every contour end to end, which is the coordinate SkTrimPathEffect
 *  uses and therefore the one `trim()` always spoke. */
struct Span {
  float begin = 0.0f, end = 1.0f;
  bool operator==(const Span &) const = default;
};

/** What a Spans value is resolved against. `fitRects` are the derive
 *  pass's answers for spans::fit(), keyed; `values` holds the resolved
 *  animatable endpoints in declaration order (two per term), which is how
 *  a reveal can be a transition or a binding without Spans knowing about
 *  either. */
struct SpanInput {
  const SkPath *outline = nullptr;
  const std::vector<std::pair<std::string, SkRect>> *fitRects = nullptr;
  const std::vector<float> *values = nullptr;
};

/** WHERE a stroke pass goes: a comparable value built by the `spans::`
 *  factories and combined with `|` (union).
 *
 *  Deliberately a CLOSED vocabulary rather than an open seam. The seam
 *  convention (one named required member, §33) governs shapers, profiles
 *  and crossing rules — things whose whole point is that a user writes
 *  new ones. A span is an interval set: kit values (kit::spans::brackets)
 *  are COMPOSITIONS of these terms, not new kinds, so nothing is lost and
 *  the value stays trivially prunable. Widening it later is additive. */
class Spans {
public:
  enum class Rule : uint8_t {
    Range,   ///< [begin, end] outright — and upTo(t) is range(0, t)
    Wrap,    ///< [begin, end] on the boundary read as a CYCLE (spans::wrap)
    Corners, ///< a window of `arm` px either side of every tangent break
    Edges,   ///< everything EXCEPT within `arm` px of a break
    Every,   ///< `count` equal slots, each claiming its leading `duty`
    At,      ///< one slot (`index`) of `count`
    Fit,     ///< the run the keyed element covers, grown by `margin` px
    Rest,    ///< the complement (see Element::stroke)
  };
  /** One term of the union. Only the members its Rule reads are
   *  meaningful; the rest keep their defaults so the value compares. */
  struct Term {
    Rule rule = Rule::Range;
    Animatable<float> begin = 0.0f, end = 1.0f;
    float arm = 0.0f;         ///< Corners/Edges: px of arc length
    float angleDeg = 30.0f;   ///< Corners/Edges: the tangent break that counts
    float duty = 1.0f;        ///< Every: fraction of each slot claimed
    float margin = 0.0f;      ///< Fit: px grown around the keyed content
    int count = 1, index = 0; ///< Every/At
    std::string key;          ///< Fit: the content key; Rest: the pass name
  };
  std::vector<Term> terms;

  /** Structural equality, defined beside the reconciler's own property
   *  comparator so an animated endpoint compares the way every other
   *  animated property does (declared here, defined in Reconcile.cpp). */
  bool operator==(const Spans &other) const;

  /** Resolve to intervals. Rest terms return nothing — the complement
   *  needs the element's OTHER passes and is computed by the painter. */
  std::vector<Span> resolve(const SpanInput &in) const;
  /** How many floats `SpanInput::values` must carry (two per term). */
  size_t valueCount() const { return terms.size() * 2; }
  bool hasRest() const {
    for (const Term &t : terms)
      if (t.rule == Rule::Rest)
        return true;
    return false;
  }
};

/** Union. `spans::corners(18) | spans::at(0, 4)` is one pass. */
inline Spans operator|(Spans a, const Spans &b) {
  a.terms.insert(a.terms.end(), b.terms.begin(), b.terms.end());
  return a;
}

/** The span factories — the WHERE half of `.stroke(where, what)`. */
namespace spans {
/** `[begin, end]` of the boundary's arc length. Both ends take the full
 *  Animatable treatment (constant, `animate(...)`, or a bound Output). */
Spans range(Animatable<float> begin, Animatable<float> end);
/** THE SEAM-CROSSING RANGE: the boundary read as a CYCLE, so a window
 *  whose `begin` is past its `end` claims [begin,1] AND [0,end] — the
 *  marching-ants and orbiting-comet idiom, and the one thing `trim()`
 *  did that spans could not (`TrimMode::Wrap`).
 *
 *      .stroke(spans::wrap(bind(&phase), bind(&phase).offset(0.25f)), ants)
 *
 *  Both ends take the full Animatable treatment, so the window marches by
 *  driving them; two shaped bindings on ONE Output are how a fixed-length
 *  window is spelled (that is trim's `offset` argument, as arithmetic on
 *  the endpoints rather than a third parameter).
 *
 *  A DEDICATED TERM, not `range()` learning to wrap, for two reasons.
 *  (1) `range(0.9, 0.1)` compiles today and means the empty/reversed
 *  window that `normalizeSpans` swaps — teaching it to wrap would change
 *  what existing descriptions DRAW, which §27 forbids and R1 is not the
 *  phase for. (2) The no-overlap law reads over RESOLVED runs, and this
 *  is the only term that yields two runs from one pair of endpoints; a
 *  reader auditing a claim conflict needs the call site to say that the
 *  term is cyclic. `wrap` names the intent; `range` stays the clamped
 *  interval it has always been.
 *
 *  Semantics match `TrimMode::Wrap` exactly, INCLUDING the degenerate
 *  ends: `end - begin <= 0` claims nothing and `>= 1` claims the whole
 *  boundary (both read from the RAW endpoints, before the fractional
 *  wrap, which is why a window driven past 1.0 keeps its length). The
 *  seam itself (fraction 0) is the outline's own start point, and a
 *  seam-crossing claim stitches into ONE contour so caps and additive
 *  brushes never double-hit there. */
Spans wrap(Animatable<float> begin, Animatable<float> end);
/** THE REVEAL: `range(0, end)`. `spans::upTo(animate(from(0.f).to(1.f),
 *  {600ms}))` is a stroke that DRAWS ON, and a bound Output scrubs it —
 *  uniform across every brush, which is what `trim()` never was (it was a
 *  node-level property that happened to reveal). */
Spans upTo(Animatable<float> end);
/** A window of `arm` px of arc length either side of every tangent break
 *  — the four corner L's, and the reticle bracket vocabulary. Follows any
 *  silhouette: chamfer the shape and the marks move to the chamfers.
 *  `angleDeg` is what counts as a break; a regular n-gon turns 360/n per
 *  vertex, so nothing above 12 sides clears the 30° default (Border's
 *  cornerAngleDeg doctrine — the scan warns rather than adapting). */
Spans corners(float arm, float angleDeg = 30.0f);
/** The complement of corners(): the runs BETWEEN the breaks, stopping
 *  `arm` px short of each — the rule with open corners. */
Spans edges(float arm, float angleDeg = 30.0f);
/** `count` equal slots around the boundary, each claiming the leading
 *  `duty` of its own slot. `duty == 1` tiles the boundary completely. */
Spans every(int count, float duty = 1.0f);
/** One slot of `count` — `every()`'s singular. Positional, so it moves
 *  when the geometry changes; use it on settled compositions. */
Spans at(int index, int count);
/** The run the KEYED element covers, grown by `margin` px: a gap sized
 *  from content, resolved in the derive phase against that element's
 *  resolved box (the flowAround pattern, applied to a boundary). */
Spans fit(std::string_view key, float margin = 0.0f);
/** Everything this element's other CLAIMING passes left over. */
Spans rest();
/** The complement of ONE named pass — and, unlike bare rest(), allowed
 *  to overlay other passes on purpose. */
Spans rest(std::string_view passName);
} // namespace spans

// ---------------------------------------------------------------------------
// The profile seam — how far a mark sits ACROSS its spine

/** A profile value: `float across(float along) const`, `float max()
 *  const`, and EQUALITY.
 *
 *  `max()` is REQUIRED, and that is the point of the seam: a varying width
 *  whose reach is unknown can only be clipped silently (the Ribbon widthFn
 *  trap, ROADMAP §25). Equality is required for the other half of the same
 *  argument — a profile is read live, and §33's comparable-values law says
 *  anything an author hands the library participates in reconciler
 *  equality or a pruned node reads it stale forever. An incomparable
 *  callable is not a profile; write a struct.
 *
 *  `along` is a fraction of the spine's arc length; `across` is px on its
 *  normal, positive to the LEFT of travel — see bandPointAt for the sign
 *  and for why it is the negation of lines::offsetAlong's. */
template <typename P>
concept ProfileScheme = std::equality_comparable<P> &&
    requires(const P &p, float along) {
      { p.across(along) } -> std::convertible_to<float>;
      { p.max() } -> std::convertible_to<float>;
    };

/** Type-erased comparable profile — Decoration's pattern on the width
 *  seam. SHARED vocabulary: a band's taper, a weave strand's path and the
 *  future ribbon width are all one value. */
class Profile {
public:
  template <ProfileScheme P>
  Profile(P scheme) // NOLINT: implicit by design (across(myTaper))
      : m_max((float)scheme.max()) {
    // The concept requires equality, so every profile keeps a comparator —
    // there is no conservatively-unequal fallback here, unlike Decoration.
    m_held = scheme;
    m_equals = [](const std::any &a, const std::any &b) {
      return std::any_cast<const P &>(a) == std::any_cast<const P &>(b);
    };
    m_across = [s = std::move(scheme)](float along) { return s.across(along); };
  }
  Profile() = default;

  float across(float along) const { return m_across ? m_across(along) : 0.0f; }
  /** The widest this profile ever reaches — what bleed and cull are
   *  computed from, so nothing it draws is silently truncated. */
  float max() const { return m_max; }
  bool operator==(const Profile &o) const {
    // Reflexive on the DEFAULT-CONSTRUCTED value too: two empty profiles
    // are the same nothing, and a value that does not compare equal to
    // itself makes every containing description patch forever.
    if (!m_equals || !o.m_equals)
      return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

private:
  float m_max = 0.0f;
  std::function<float(float)> m_across;
  std::any m_held;
  std::function<bool(const std::any &, const std::any &)> m_equals;
};

/** The CORE profile presets. The oscillating family (`wave`, and `braid`
 *  built on it) is kit, per the tier rule — core holds the seam and the
 *  two profiles every other one is measured against. */
namespace strand {
/** across ≡ 0: the boundary itself. */
struct Self {
  float across(float) const { return 0.0f; }
  float max() const { return 0.0f; }
  bool operator==(const Self &) const = default;
};
/** across ≡ px: a parallel. Parallels are rails — they never cross.
 *
 *  **Positive is LEFT of travel** (outside a clockwise path) — the band's
 *  frame, which this shares by ruling. Note that
 *  `kit::brush::shapers::offset` is the OPPOSITE sign: it wraps
 *  `lines::offsetAlong`, whose convention is right-of-travel and which is
 *  not being flipped (§27). The split predates both (see bandPointAt) and
 *  its reconciliation is open — RULED 2026-07-27: **left wins, and the
 *  lines:: sign dies in R3**; the flip rides the R2 corpus port because
 *  it changes every existing caller's picture. */
struct Offset {
  float px = 0.0f;
  float across(float) const { return px; }
  float max() const { return std::abs(px); }
  bool operator==(const Offset &) const = default;
};
inline Profile self() { return Profile(Self{}); }
inline Profile offset(float px) { return Profile(Offset{px}); }
} // namespace strand

/** The band's width, named at the call site: `band(spine, across(22))`.
 *  Takes a constant or any Profile (a taper, a kit oscillation). */
struct Across {
  Profile profile;
  bool operator==(const Across &) const = default;
};
inline Across across(float px) { return Across{strand::offset(px)}; }
inline Across across(Profile p) { return Across{std::move(p)}; }

/** Which side of the spine the band occupies. Explicit because the
 *  offset-path lineage has no defensible default beyond "both". */
enum class Formation : uint8_t { Centered, Outward, Inward };

/** A band spine borrowed from another element's resolved shape, through
 *  the derive phase: `band(around("dial"), across(14))`. */
struct Around {
  std::string key;
  bool operator==(const Around &) const = default;
};
inline Around around(std::string_view key) { return Around{std::string(key)}; }

// ---------------------------------------------------------------------------
// The shaper seam — the ONE way geometry deviates

/** A shaper value: `SkPath shape(const SkPath &) const`, plus equality.
 *
 *  It bends the ONE CONTINUOUS MARK — a wave, a zigzag, a jitter, an
 *  offset. That is the whole of the geometry-deviation vocabulary: the
 *  other mechanism (a PATTERN, which builds the mark out of cells) is a
 *  brush kind, not a shaper, and the two were worth naming apart.
 *
 *  SkPath in, SkPath out, because dash and width are path operations —
 *  proved by the fact that every op the corpus wanted was expressible
 *  that way. `bleed()` is optional and declares how far the deviation
 *  reaches (a wave's amplitude), so a cull can hold it.
 *
 *  There are no sugar methods over this seam. Stock shapers are kit
 *  values (`kit::brush::shapers::`), peers of anything you write — which
 *  is the point of a seam and the reason `jittered()`-style convenience
 *  was refused. */
template <typename S>
concept ShaperScheme = std::equality_comparable<S> &&
    requires(const S &s, const SkPath &p) {
      { s.shape(p) } -> std::convertible_to<SkPath>;
    };

/** Type-erased comparable shaper. Also accepts the legacy `apply()`
 *  spelling (Brushes.h's GeometryOp family) so §27 holds. */
class Shaper {
public:
  template <ShaperScheme S>
  Shaper(S scheme) // NOLINT: implicit by design (.shaped(myWave))
      : m_bleed([&] {
          if constexpr (requires { { scheme.bleed() } -> std::convertible_to<float>; })
            return (float)scheme.bleed();
          else
            return 0.0f;
        }()) {
    m_held = scheme;
    m_equals = [](const std::any &a, const std::any &b) {
      return std::any_cast<const S &>(a) == std::any_cast<const S &>(b);
    };
    m_shape = [s = std::move(scheme)](const SkPath &p) { return s.shape(p); };
  }
  Shaper() = default;

  SkPath shape(const SkPath &p) const { return m_shape ? m_shape(p) : p; }
  float bleed() const { return m_bleed; }
  bool operator==(const Shaper &o) const {
    if (!m_equals || !o.m_equals)
      return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

private:
  float m_bleed = 0.0f;
  std::function<SkPath(const SkPath &)> m_shape;
  std::any m_held;
  std::function<bool(const std::any &, const std::any &)> m_equals;
};

// ---------------------------------------------------------------------------
// Strands — WHERE a composite's marks run

/** Where one strand of a composite runs. Two families:
 *
 *  RELATIVE — a displacement of the stroked boundary in its (along,
 *  across) frame, **the same frame a band owns** (across positive to the
 *  LEFT of travel; see bandPointAt). Any Profile is one: `strand::self()`
 *  rides the boundary, `strand::offset(px)` runs parallel, and a custom
 *  profile value is accepted here directly.
 *
 *  ABSOLUTE — `strand::from(key)` borrows a keyed element's resolved path
 *  through the derive phase, and `strand::path(p)` is authored geometry
 *  (SkPath is comparable, so it prunes). **With only absolute strands the
 *  boundary is an unpainted host** — nothing runs on it, which is a real
 *  and useful shape of composite.
 *
 *  Paths are DATA: a path participates as an element's shape, as borrowed
 *  geometry, or as pure guide data in no tree. This is the third case. */
class StrandPath {
public:
  enum class Source : uint8_t { Relative, Borrowed, Authored };

  StrandPath() = default;
  StrandPath(Profile p) // NOLINT: implicit by design (.path = strand::self())
      : m_source(Source::Relative), m_profile(std::move(p)) {}
  static StrandPath borrowed(std::string key) {
    StrandPath s;
    s.m_source = Source::Borrowed;
    s.m_key = std::move(key);
    return s;
  }
  static StrandPath authored(SkPath path) {
    StrandPath s;
    s.m_source = Source::Authored;
    s.m_path = std::move(path);
    return s;
  }

  Source source() const { return m_source; }
  const Profile &profile() const { return m_profile; }
  const std::string &key() const { return m_key; }
  const SkPath &path() const { return m_path; }
  /** How far off the boundary this strand can run — 0 for the absolute
   *  family, whose geometry is its own. */
  float reach() const {
    return m_source == Source::Relative ? m_profile.max() : 0.0f;
  }
  bool operator==(const StrandPath &o) const {
    return m_source == o.m_source && m_profile == o.m_profile &&
           m_key == o.m_key && m_path == o.m_path;
  }

private:
  Source m_source = Source::Relative;
  Profile m_profile;
  std::string m_key;
  SkPath m_path;
};

namespace strand {
/** Borrow a keyed element's resolved path (derive phase, cycle-guarded
 *  like every other borrow). */
inline StrandPath from(std::string_view key) {
  return StrandPath::borrowed(std::string(key));
}
/** Authored geometry, in the host element's local space. */
inline StrandPath path(SkPath p) { return StrandPath::authored(std::move(p)); }
} // namespace strand

/** Displace a path in its own (along, across) frame — the primitive
 *  behind a relative strand, and the band's frame exactly: `along` is a
 *  fraction of total arc length, positive `across` is LEFT of travel
 *  (outside a clockwise path). NOT `lines::offsetAlong`, which offsets
 *  RIGHT of travel and stays as it is FOR NOW (§27); the two conventions
 *  are documented at bandPointAt, and §33 ruling 5 settles it — LEFT wins
 *  everywhere and the lines:: sign dies in R3, flipped by the R2 port. */
SkPath profileOffset(const SkPath &spine, const Profile &profile);

/** THE REGION a band occupies: the spine walked at both profile rails,
 *  per contour, through `profileOffset` — so corners get
 *  `lines::offsetAlong`'s real-vertex repair (arc outside a turn, miter
 *  inside) instead of the sample-and-displace spur that a naive walk
 *  leaves on the inside of every rectangle.
 *
 *  Public because a varying-width MARK along a spine IS this region: the
 *  ruling that a milled groove is band + fill (§8b) applies to a ribbon
 *  too, and `brushes::Ribbon`'s profile path fills exactly this. One
 *  geometry, so the corner repair is not reimplemented per consumer. */
SkPath bandRegion(const SkPath &spine, const Across &width,
                  Formation formation = Formation::Centered);

// ---------------------------------------------------------------------------
// Crossings — WHICH mark is on top where two strands meet

/** Who is on top. Read against a Crossing's `a`, which is always the
 *  LOWER strand index, so the question is well-posed: Over means strand
 *  `a` passes over strand `b`. */
enum class Order : uint8_t { Over, Under };

/** One discovered crossing. **Crossings are never authored** — they are
 *  found by path intersection and numbered along the boundary. */
struct Crossing {
  /** ORDINAL in the discovered list, 0-based: the crossings are sorted by
   *  `alongA` (position on the lower-indexed strand) and then numbered.
   *  It is NOT a coordinate in any parameterisation and NOT stable under
   *  a change of geometry — add a strand or move one and the same knot may
   *  take a different ordinal. This is the number `CrossingRule::except()`
   *  pins, which is exactly why pins are documented as positional. */
  size_t index = 0;
  /** Strand indices, always `a < b` — `b` is the one list order paints
   *  later, i.e. on top when nothing says otherwise. */
  size_t a = 0, b = 0;
  SkPoint at{0, 0};
  /** Where the crossing falls along each strand, as fractions of that
   *  strand's arc length. */
  float alongA = 0.0f, alongB = 0.0f;
  bool operator==(const Crossing &) const = default;
};

/** A crossing rule value: `Order decide(const Crossing &) const`, plus
 *  equality — the seam convention's one named required member. Never a
 *  bare lambda: a rule is read live, so it must prune. */
template <typename D>
concept CrossingScheme = std::equality_comparable<D> &&
    requires(const D &d, const Crossing &c) {
      { d.decide(c) } -> std::convertible_to<Order>;
    };

/** The rule ladder, as ONE comparable value. Climb only as far as the
 *  composition needs:
 *
 *      crossing::alternate()                    // == sequence({Over, Under})
 *      crossing::sequence({Over, Over, Under})  // any repeating pattern
 *      crossing::pairs({{0,1},{1,2},{2,0}})     // dominance, cyclic allowed
 *      MyRule{}                                 // your own decide() value
 *
 *  and pin exceptions onto whatever you chose with `.except(i, order)`.
 *
 *  The default is LIST ORDER: later strands pass over earlier ones. That
 *  is what makes `layers` and `weave` formally one machine. */
class CrossingRule {
public:
  CrossingRule() = default;
  template <CrossingScheme D>
  CrossingRule(D scheme) // NOLINT: implicit by design (.crossing = MyRule{})
      : m_kind(Kind::Custom) {
    m_held = scheme;
    m_equals = [](const std::any &x, const std::any &y) {
      return std::any_cast<const D &>(x) == std::any_cast<const D &>(y);
    };
    m_decide = [s = std::move(scheme)](const Crossing &c) {
      return s.decide(c);
    };
  }

  static CrossingRule sequence(std::vector<Order> pattern) {
    CrossingRule r;
    r.m_kind = Kind::Sequence;
    r.m_pattern = std::move(pattern);
    return r;
  }
  static CrossingRule pairs(std::vector<std::pair<int, int>> dominance) {
    CrossingRule r;
    r.m_kind = Kind::Pairs;
    r.m_dominance = std::move(dominance);
    return r;
  }

  /** Pin ONE crossing, layered over whatever rule this already is.
   *
   *  **Pins are POSITIONAL**: the index is a position in the discovered
   *  order, so a stable RULE survives a geometry change and a pin does
   *  not — move a strand and pin 3 lands on a different meeting. Use
   *  rules while a composition is still moving, and pins only once it is
   *  settled and you are correcting one knot by eye.
   *
   *  Pins compose onto the base rule and never stack as separate
   *  entries: there is one `.crossing` field, and this is how it takes
   *  exceptions. */
  CrossingRule &except(size_t index, Order order) {
    for (auto &pin : m_pins)
      if (pin.first == index) {
        pin.second = order;
        return *this;
      }
    m_pins.emplace_back(index, order);
    return *this;
  }

  Order decide(const Crossing &c) const {
    for (const auto &pin : m_pins)
      if (pin.first == c.index)
        return pin.second;
    switch (m_kind) {
    case Kind::Sequence:
      if (!m_pattern.empty())
        return m_pattern[c.index % m_pattern.size()];
      break;
    case Kind::Pairs:
      for (const auto &[over, under] : m_dominance) {
        if (over == (int)c.a && under == (int)c.b)
          return Order::Over;
        if (over == (int)c.b && under == (int)c.a)
          return Order::Under;
      }
      break;
    case Kind::Custom:
      if (m_decide)
        return m_decide(c);
      break;
    case Kind::ListOrder:
      break;
    }
    // List order: `b` is later in the list, so `a` is underneath.
    return Order::Under;
  }

  bool operator==(const CrossingRule &o) const {
    if (m_kind != o.m_kind || m_pattern != o.m_pattern ||
        m_dominance != o.m_dominance || m_pins != o.m_pins)
      return false;
    if (m_kind != Kind::Custom)
      return true;
    if (!m_equals || !o.m_equals)
      return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

private:
  enum class Kind : uint8_t { ListOrder, Sequence, Pairs, Custom };
  Kind m_kind = Kind::ListOrder;
  std::vector<Order> m_pattern;
  std::vector<std::pair<int, int>> m_dominance;
  std::vector<std::pair<size_t, Order>> m_pins;
  std::function<Order(const Crossing &)> m_decide;
  std::any m_held;
  std::function<bool(const std::any &, const std::any &)> m_equals;
};

namespace crossing {
/** Over, under, over, under — the plain-weave rule, and formally just
 *  `sequence({Over, Under})`. Both words are kept because they name two
 *  author intents over one machine (the layers/weave precedent). */
inline CrossingRule alternate() {
  return CrossingRule::sequence({Order::Over, Order::Under});
}
inline CrossingRule sequence(std::vector<Order> pattern) {
  return CrossingRule::sequence(std::move(pattern));
}
/** Strand DOMINANCE: `{{over, under}, …}`. Cycles are legal and are the
 *  point — `{{0,1},{1,2},{2,0}}` is the impossible-braid rule Penrose
 *  tilings and heraldic knots are full of. */
inline CrossingRule pairs(std::vector<std::pair<int, int>> dominance) {
  return CrossingRule::pairs(std::move(dominance));
}
} // namespace crossing

/** Every crossing among a set of strand paths, numbered along the
 *  boundary (ascending by position on the lowest-indexed strand
 *  involved). Only PROPER crossings count: coincident strands and
 *  endpoint touches (a shared polygon vertex) are meetings, not
 *  crossings, and reporting them would put a knot at every corner.
 *
 *  Public because the rule VALUES are shared with the pinned
 *  element-level crossover pass — its API is undecided, this vocabulary
 *  is not. */
std::vector<Crossing> discoverCrossings(const std::vector<SkPath> &strands);

/** The region where two strands' MARKS actually overlap at one crossing:
 *  the intersection of the two paths stroked to their own reach, reduced to
 *  the component containing `at` and bounded by @p maxRadius px around it.
 *
 *  Exact at any angle, which a disc is not — two marks meeting at 12° overlap
 *  in a long lens whose extent along each strand goes as reach/sin(theta),
 *  and a disc sized for the perpendicular case leaves the under-strand
 *  showing straight across the over-strand's mark.
 *
 *  `maxRadius` is not a safety margin, it is REQUIRED for correctness on any
 *  ordinary braid. Once reach/sin(theta) approaches the spacing between
 *  knots, the neighbouring lenses touch and pathops merges them into ONE
 *  contour — at which point the first crossing's patch owns the whole run
 *  and the weave degenerates to "one strand on top" (measured: a 3px/40px
 *  braid in 6px ink got 25 of 50 knots wrong). Pass half the arc distance to
 *  the adjacent crossing so each knot can only ever claim its own half.
 *
 *  Falls back to a disc when the intersection is empty (degenerate or
 *  non-overlapping input). */
SkPath crossingPatch(const SkPath &a, float reachA, const SkPath &b,
                     float reachB, SkPoint at, float maxRadius);

// ---------------------------------------------------------------------------
// Layout values (Yoga semantics, 1:1)

struct Dim {
  enum class Unit : uint8_t { Px, Pct, Auto };
  Unit unit = Unit::Auto;
  float value = 0.0f;

  constexpr Dim() = default;
  constexpr Dim(float px) // NOLINT: implicit by design
      : unit(Unit::Px), value(px) {}
  bool operator==(const Dim &) const = default;
};
constexpr Dim pct(float v) {
  Dim d;
  d.unit = Dim::Unit::Pct;
  d.value = v;
  return d;
}
constexpr Dim autoDim() { return {}; }

/** `width(50_pct)`, `basis(120_px)` — for the Dim-valued setters;
 *  exposed by `using namespace sigil::compose` (or `using namespace
 *  sigil::compose::literals`). */
inline namespace literals {
constexpr Dim operator""_px(long double v) { return Dim((float)v); }
constexpr Dim operator""_px(unsigned long long v) { return Dim((float)v); }
constexpr Dim operator""_pct(long double v) { return pct((float)v); }
constexpr Dim operator""_pct(unsigned long long v) { return pct((float)v); }
} // namespace literals

enum class Align : uint8_t { Auto, Start, Center, End, Stretch, Baseline };
enum class Justify : uint8_t {
  Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly
};

/** How trim() treats fractions outside [0,1]. Clamp (the default) pins
 *  them — a reveal saturates at the ends. Wrap treats the outline as a
 *  CYCLE: fractions wrap mod 1, and a window that crosses the seam draws
 *  both pieces — the marching-ants / orbiting-comet idiom. Pair with an
 *  animated `offset` to march a fixed-length window around a closed
 *  outline forever. */
enum class TrimMode : uint8_t { Clamp, Wrap };

/** One misprint pass: the node's own fill shape and text re-stamped at
 *  `offset` in a flat color, UNDER the real content. Repeated echoes
 *  stack in declaration order (bottom first). The registration-error
 *  language: P3R's red text echo (3,−6), P5's zero-blur sticker stacks,
 *  §5's ink under-copies — one call each, no duplicate sibling nodes. */
struct Echo {
  SkVector offset = {3, 3};
  SkColor4f color = {0, 0, 0, 1};
  bool operator==(const Echo &) const = default;
};

/** Cache override. Auto (the default) picture-caches provably-static
 *  subtrees; Texture rasterizes the subtree once into an image (the
 *  raster-surface pixel win — best for dense or effect-heavy content,
 *  wasteful for sparse regions); Group is Texture for a subtree whose
 *  children ANIMATE (below); None opts a node out (per-frame paint
 *  programs that read the clock MUST declare this — declared
 *  volatility).
 *
 *  **Group** — "many small rotated/blended pieces forming one static
 *  assembly". `Cache::Texture` bakes a node's OWN paint and refuses the
 *  moment anything below it is volatile, so a fill-less container of 523
 *  animated strips gets zero bakes and every strip replays its shaders
 *  every frame. `Cache::Group` bakes the container AND its children into
 *  one unrotated device-space layer — the children's rotations, bevels and
 *  mutual compositing all resolve INSIDE the bake at full precision, which
 *  is why it is pixel-safe where a per-child `Cache::Texture` is not (that
 *  isolates each piece and moved 34% of kumiko's pixels).
 *
 *  It is held by a SUBTREE VALUE MEMO, not by a volatility verdict: the
 *  bake is taken only while every bound transform, opacity and content
 *  scalar below the node is holding the value it held last frame, and is
 *  dropped on the frame any of them ticks. So an entrance animation plays
 *  live and the settled assembly costs one blit — pixel-verified on
 *  `kumiko_asanoha` (byte-identical at seven phases across its 6.4 s
 *  loop), where the lattice reads `[group]` at ~0.49 ms of RECORDING time
 *  on a node that was 523 live pictures, with 0 cache writes in steady
 *  state. **No GPU work-ms number exists for Group yet**: the
 *  before/after pair has not been taken (ROADMAP §30).
 *
 *  It REFUSES, permanently and loudly (one line to stderr), any subtree
 *  carrying volatility a float comparison cannot see: a live material
 *  (`uTime` or a bound uniform), an animated decoration, an animated image,
 *  a bound `fill()`, a variable-font drive, a `Cache::None` descendant, or
 *  a non-srcOver blend / backdrop filter below the root (which would
 *  resolve against the bake's transparent black). It also declines, per
 *  frame, while its own transform animates or its device rect is moving —
 *  a device-pinned bake remade every frame costs more than the paint it
 *  replaces. `--bench` shows a held group as `[group]`; a Group node that
 *  never reaches that state has one of the above in it. */
enum class Cache : uint8_t { Auto, Picture, Texture, Group, None };

// ---------------------------------------------------------------------------
// Custom layout (the SwiftUI Layout-protocol shape, C++20-ified)

/** What a custom layout sees: the container's resolved size, each child's
 *  measured size (text children measured by SigilWeave), and each child's
 *  first-baseline offset from its own top (NaN for children without one) —
 *  what baseline-rhythm schemes (layouts::BaselineGrid) snap by. */
struct LayoutInput {
  SkSize container = SkSize::MakeEmpty();
  std::vector<SkSize> childSizes;
  std::vector<float> childBaselines; // NaN = no baseline (non-text)
};

/** A custom layout places children: one rect per child (position and
 *  size, container-relative). Runs as a bounded second layout pass. */
template <typename L>
concept LayoutScheme = requires(const L &l, const LayoutInput &in) {
  { l.place(in) } -> std::convertible_to<std::vector<SkRect>>;
};

// ---------------------------------------------------------------------------
// Concepts (readable errors at the generic entry points)

template <typename P>
concept ComponentProps = std::equality_comparable<P> && std::copyable<P>;

class Element;

template <typename F, typename P>
concept ComponentFn = std::invocable<F, const P &> &&
    std::convertible_to<std::invoke_result_t<F, const P &>, Element>;

// ---------------------------------------------------------------------------
// Element — a cheap value description

class Element {
public:
  Element(); // empty box

  // ---- layout ----
  Element &row();
  Element &column();
  /** Flex-wrap: children flow onto new lines/columns when they
   *  overflow the main axis. */
  Element &wrapLines(bool on = true);
  Element &gap(float px);
  Element &padding(float all);
  Element &padding(float horizontal, float vertical);
  Element &padding(float left, float top, float right, float bottom);
  Element &margin(float all);
  Element &margin(float horizontal, float vertical);
  Element &margin(float left, float top, float right, float bottom);
  Element &width(Dim d);
  Element &height(Dim d);
  Element &minWidth(Dim d);
  Element &maxWidth(Dim d);
  Element &minHeight(Dim d);
  Element &maxHeight(Dim d);
  Element &aspect(float ratio);
  Element &grow(float factor = 1.0f);
  Element &shrink(float factor);
  Element &basis(Dim d);
  Element &alignItems(Align a);
  Element &alignSelf(Align a);
  Element &justify(Justify j);
  Element &absolute();
  Element &inset(float all);
  Element &inset(float left, float top, float right, float bottom);
  /** Dim-valued insets: px, pct(), or autoDim() per side — autoDim()
   *  leaves that side unpinned (the CSS `auto`), so width/height (or the
   *  opposite inset) size the node instead of stretching it. */
  Element &inset(Dim left, Dim top, Dim right, Dim bottom);
  /** Pin ONE edge of an absolute node (implies absolute()): the
   *  corner-badge idiom — `.top(12).right(12)` pins a date block to the
   *  top-right without stretching it across the box. Unpinned sides stay
   *  auto. */
  Element &left(Dim d);
  Element &top(Dim d);
  Element &right(Dim d);
  Element &bottom(Dim d);
  /** Center this absolute node ON a parent-space point — the dominant
   *  placement in node-graph scenes (sockets on orbit positions, badges
   *  on markers). Resolved after measurement, so intrinsic-size nodes
   *  center correctly; implies absolute(). */
  Element &centerAt(SkPoint p);
  /** Place an absolute node on a parent-space RECT — the peer of
   *  centerAt(), for when you already know the box.
   *
   *  Exactly `left(r.fLeft).top(r.fTop).width(r.width()).height(r.height())`
   *  — it calls those four setters, so it writes the same four LayoutProps
   *  fields, prunes identically, and cannot drift from the longhand. Right
   *  and bottom stay unpinned.
   *
   *  **This is a primitive for MEASURED reconstruction, not a general
   *  ergonomics fix**, and the two populations disagree about it sharply.
   *  A study that rebuilds an artefact measures coordinates off a
   *  reference plate, so its positions arrive as numbers: `.left(` beats
   *  `.inset(` almost 2:1 there (626 : 328) and nine studies independently
   *  wrote this helper under four names. The gallery is flex-native and
   *  the ratio inverts (84 : 222). Measured on its two most house-style
   *  scenes: `ScenesInventory.h` collapses at most 17 chains of 791 lines
   *  (~4%), and its coordinates are `cellX(col)`/`cellY(row)`/`spanW(w)`,
   *  so this respells the call rather than removing arithmetic;
   *  `ScenesPersona.h` saves **zero** — `plainRow()` (`:438-447`) pins
   *  `.left(kBaseX + r.dx - 14).top(r.y - 14)` and carries no width or
   *  height at all, so there is no rect to pass. If your position is a
   *  *relationship* — "inside its parent", "next to that one", "as wide as
   *  the column" — flex and inset() are the right tools and this is the
   *  wrong one.
   *
   *      g.child(box().rect(panelBox).fill(…));
   *      g.child(text(u8"…", st).at({panelBox.fLeft + 16, panelBox.fTop}));
   *
   *  Does not cover right()/bottom() pinning, percentage insets, or
   *  autoDim() sides — those are different intents and keep the longhand.
   *  `util::centred()` (Util.h) builds the rect for the centre-and-size
   *  case. */
  Element &rect(const SkRect &r);
  /** Pin an absolute node's top-left to a parent-space POINT, leaving the
   *  node to size itself from its content — `left(p.fX).top(p.fY)`, the
   *  187-site half of the placement longhand that carries no box.
   *  Same qualification as rect() above. */
  Element &at(SkPoint topLeft);

  // ---- shape (defines PaintContext::outline and clipping) ----
  Element &corners(Corners c);
  /** Legacy spelling of the span reveal — retained until the R3 deletion (ROADMAP §33) (§27);
   *  new code writes `.stroke(spans::upTo(t), brush)`, which reveals a
   *  named PASS instead of the whole node and works for every brush kind.
   *  Trim the node's painted outline to the [start, end] fraction of its
   *  arc length (the Lottie/sksg Trim Path — SkTrimPathEffect underneath).
   *  Applies to the fill surface and every outline-following decoration
   *  (PathFormat strokes, ContourWalk), so a stroked border with
   *  `.trim(0, animate(from(0.f).to(1.f), {600ms}))` DRAWS ON, and a
   *  connector's wire can reveal along its route. Both ends take the full
   *  Animatable treatment — plain, animate() transitions, or ch::Output
   *  bindings (bound/animating trim is content volatility: the node paints
   *  live while moving). `offset`
   *  shifts both ends and takes the full PropValue treatment too — under
   *  TrimMode::Wrap, bind it to a wrapping phase Output and a fixed
   *  window marches around a closed outline forever (marching ants, the
   *  orbiting comet); under Clamp (default) fractions pin to [0,1].
   *  The wrap SEAM (fraction 0) is the outline's own start point — SkPath
   *  convention (a corner box starts at its top-left; addCircle at
   *  3 o'clock, clockwise); seam-crossing windows stitch into ONE contour
   *  so caps and additive brushes never double-hit there. Clipping and
   *  hit-testing keep the UNtrimmed shape — trim is a paint-phase reveal,
   *  not a layout change. */
  Element &trim(PropValue<float> start, PropValue<float> end,
                PropValue<float> offset = 0.0f,
                TrimMode mode = TrimMode::Clamp);
  /** THE NODE'S SHAPE: a path generator over its laid-out size, in local
   *  coordinates. Overrides corners() — the fill surface, clip(), every
   *  stroke pass and every outline-following decoration (PathFormat,
   *  ContourWalk) trace it. Spiky dialogs, scalloped frames, any
   *  non-rectangular chrome.
   *
   *  Renamed from `outline()` (which still compiles, below): the old name
   *  read as a DRAWN LINE — the thing `stroke()` does — and call sites
   *  showed it, `.outline(chevron()).fill(ramp)` filling an "outline" and
   *  `.outline(shape).stroke(brush)` putting two halves of one idea under
   *  one word. A shape is a region; a stroke is a mark on its boundary.
   *
   *  Like custom(), the generator is an incomparable callable — memo()
   *  such a node (or keep it pointer-stable) to prune it while its size
   *  and inputs are unchanged. */
  Element &shape(std::function<SkPath(SkSize)> path);
  /** Legacy spelling of shape() — retained until the R3 deletion (ROADMAP §33) (§27). */
  Element &outline(std::function<SkPath(SkSize)> path) {
    return shape(std::move(path));
  }
  /** BAND FORMATION: which side of the spine the band occupies.
   *  `.centered()` is the default and straddles it; `.outward()` and
   *  `.inward()` take one side (the offset-path lineage). No effect on a
   *  node that is not a band(). */
  Element &centered();
  Element &outward();
  Element &inward();
  /** Clip fill, content, and children to the node's shape. Decorations
   *  are NOT clipped — they dress the outline (outer strokes, shadows,
   *  glows keep their reach); hit-testing still bounds the subtree. */
  Element &clip(bool on = true);

  // ---- paint ----
  /** A colour, a shader, a transition between colours, or a LIVE binding.
   *
   *  The binding form is `fill(&output)` where the Output holds a `Fill`,
   *  and it is the answer to "this widget's colour IS its value" — a
   *  level meter whose hue is the level, a temperature readout, a health
   *  bar that reddens. Write the Fill Output from the same steppable that
   *  computes the number:
   *
   *      ch::Output<float> level; ch::Output<Fill> bar;
   *      ticker.add([&](double){ level = v; bar = Fill::color(ramp(v)); … });
   *      box().scaleX(bind(&level)).fill(&bar)
   *
   *  Spelled out because a study concluded there was no bound Fill at all
   *  and rebuilt its most period-authentic widget on renderSlot() instead.
   *  What genuinely does NOT exist is deriving one from the other at the
   *  binding site — `fill(bind(&level).map(ramp))` — see ROADMAP.md. */
  Element &fill(PropValue<Fill> f);
  /** Fill with a Material (gradient ramp, blend stack, sprite, SkSL) — the
   *  richer authoring value. A static Material collapses to a Fill, so it
   *  caches and prunes on the same path. See <sigilcompose/Material.h>. */
  Element &fill(Material m);
  /** Solid-color sugar: fill({r,g,b,a}) without the Fill:: ceremony. */
  Element &fill(SkColor4f color) { return fill(PropValue<Fill>{Fill::color(color)}); }
  /** How an image() leaf samples its source. Defaults to linear, which is
   *  right for photographs and wrong for every pixel grid: art, tilemaps,
   *  fonts baked as sprites, simulation buffers.
   *
   *      image(tileset).sampling(SkSamplingOptions(SkFilterMode::kNearest))
   *
   *  `Material::image()` has always taken sampling; the element factory
   *  did not, so the fix was discoverable only by diffing two signatures.
   *  No effect on non-image leaves. */
  Element &sampling(SkSamplingOptions options);
  // ---- decoration layers ----
  // Backgrounds paint below content/children (in declaration order),
  // foregrounds above; fill() is the transitionable first background,
  // custom() a box with one background program.
  // Repeated calls APPEND (the Photoshop stacked-strokes model — two
  // stroke() calls are two rings).
  // Decorations dress the OUTLINE: clip() does not clip them (it bounds
  // fill/content/children only), so outer strokes and shadows survive on
  // clipped nodes.
  /** A directional REVEAL: shows the fraction of the node lying before a
   *  moving edge travelling at @p angleDeg (0 = left-to-right, 90 = top
   *  to bottom, and any angle between).
   *
   *  `trim()` walks the PERIMETER, so on a filled shape 0→1 sweeps a
   *  wedge round the outline instead of extending the surface, and
   *  `scaleX`/`scaleY` SQUASH rather than reveal — a striped or textured
   *  fill visibly compresses. Three studies met this and the last one's
   *  workaround was the worst in the program: it left the retained tree
   *  entirely, snapshotting each node at setup and replaying it under a
   *  hand-written clipRect in a `custom(Cache::None)` leaf, forfeiting
   *  decorations, hit-testing and pruning on twelve nodes at once.
   *
   *  Paint-only and bindable, like the transforms — animating a wipe
   *  never relayouts, and it covers the node's decorations too, because a
   *  reveal reveals. */
  Element &wipe(float angleDeg, PropValue<float> fraction);
  /** Takes this node OUT of hit testing — CSS `pointer-events: none`.
   *
   *  `hitTest` returns any keyed node whose box contains the point,
   *  whether or not it paints anything. That is correct and it means a
   *  keyed full-bleed layout SHELL with no fill swallows every hit in
   *  the frame: a study's four stat-bar groups were transparent
   *  containers carrying their bars' keys, and every point on screen
   *  came back as the last of them. The failure is silent and total.
   *
   *  Children are still tested — this excludes the node's own box, not
   *  its subtree. */
  Element &hitTestable(bool enabled);
  /** A decoration painted OVER the fill and UNDER the content and
   *  children — the slot between the two that did not exist.
   *
   *  `background()` hides beneath the FILL (an opaque fill covers it —
   *  the trap that sent a first attempt at bevelled chrome back as flat
   *  slabs, 31 bevels all drawing underneath their own surfaces), and
   *  `foreground()` paints above the children, so a textured button
   *  greys out its own label. This slot is what hazard stripes over a
   *  surface but under the digit, scanlines over a panel but under its
   *  readout, and 100% of bevelled chrome actually want. The workaround
   *  was a sibling stack, which costs a node and loses the outline. */
  Element &overlay(Decoration d);
  /** A decoration painted BENEATH the fill (the CSS box-shadow
   *  ordering) — shadows, ground textures, anything the surface sits on
   *  top of. If you want it over the surface but under the children, that
   *  is `overlay()` above. */
  Element &background(Decoration d);
  Element &foreground(Decoration d);
  /** fill's peer (the Photoshop/Illustrator mental model): dress the
   *  node's whole BOUNDARY with a brush — a PathFormat, a layered brush
   *  stack, any decoration that strokes.
   *
   *  This form does not CLAIM: it overlays the whole boundary, so
   *  repeated calls stack the way the decoration law says (two strokes
   *  are two rings) and never collide. Naming a `where` (below) is what
   *  turns a pass into a claim on part of the boundary. */
  Element &stroke(Decoration brush);
  /** THE STROKE SLOT: `where` on the boundary, painted by `what`.
   *
   *      .stroke(spans::corners(18), stroke(2, ink))          // reticle
   *      .stroke(spans::edges(14), stroke(1, ink))            // open corners
   *      .stroke(spans::upTo(animate(from(0.f).to(1.f), {600ms})), wire)
   *
   *  Repeated calls APPEND, in declaration order — the decoration law.
   *  ORDERING, precisely: the unqualified strokes paint FIRST (they are
   *  foregrounds and share that list), then the span passes in their own
   *  declaration order. Within each group declaration order holds;
   *  between the groups the unqualified ones are underneath. Interleaving
   *  the two by call order is not expressible today — if a span pass has
   *  to sit UNDER a whole-boundary one, make the whole-boundary one a
   *  span pass too (`spans::every(1)`) so both are in one list.
   *
   *  Span-qualified passes CLAIM the runs they resolve to, and two claims
   *  that overlap are a mistake the library says out loud (naming both
   *  passes and the overlapping run): one boundary, one mark. Layering
   *  two marks on one run is a composite BRUSH, not two passes — today
   *  `Brush{}.leg(a).leg(b)` or a LayeredBrush.
   *
   *  Two exceptions, both deliberate: bare `spans::rest()` claims
   *  whatever the other passes left over (so a rule and its bracket
   *  corners are two calls and no arithmetic), and `spans::rest("name")`
   *  is the complement of ONE named pass and may overlay the others.
   *
   *  `name` is LOCAL to this element — for inspection and for the
   *  `rest("name")` reference. It is never a query key: a second identity
   *  system is exactly what the query side refuses (DESIGN §Queries). */
  Element &stroke(Spans where, Decoration what, std::string name = {});
  /** Apply a whole LayerStyle (preset or hand-built): its `under` layers
   *  append as backgrounds, `over` as foregrounds — one call dresses the
   *  node in aqua gel / y2k chrome / any bundled treatment. Composable
   *  with fill() and further background()/foreground() calls. */
  Element &style(LayerStyle s);
  /** Append a misprint echo (see Echo): the node's fill shape and text
   *  re-stamped offset+flat-colored beneath the real pass. Not applied to
   *  glyphFx text (kinetic draws its own buckets) or image/custom content. */
  Element &echo(SkVector offset, SkColor4f color);
  /** Post-processes this node's rendered layer (forces a stacking
   *  context). Baked once under Cache::Texture. */
  Element &effect(Effect e);
  /** Filters what is already painted beneath this node's bounds before
   *  the node paints (CSS backdrop-filter). Incompatible with
   *  Cache::Texture (the backdrop depends on the live destination);
   *  such nodes fall back to picture caching. */
  Element &backdrop(Effect e);
  Element &opacity(PropValue<float> o);
  Element &blend(SkBlendMode mode);
  Element &translateX(PropValue<float> v);
  Element &translateY(PropValue<float> v);
  Element &rotate(PropValue<float> degrees);
  Element &scale(PropValue<float> factor);
  /** Per-axis scale about the transform origin, multiplied INTO scale().
   *  Paint-only like scale(): animating one never relayouts, and the
   *  content picture replays under the new transform.
   *
   *  Bars, wipes, meters, cooldown sweeps, drain rings and "slide this
   *  piece into its slot" are the most common animated primitive a UI
   *  has, and not one of them is uniform. Without these the idiom was a
   *  full-width fill inside a clip translated by -(1 - fraction) * width,
   *  which only survives while the fill happens to be a gradient along
   *  the OTHER axis. Set transformOrigin() to pin the growing edge —
   *  `transformOrigin(0, 0.5f).scaleX(&fraction)` grows a bar rightward
   *  from its left edge. */
  Element &scaleX(PropValue<float> factor);
  Element &scaleY(PropValue<float> factor);
  /** Shear, in degrees, about the transform origin — the diagonal-slash
   *  language (P3R cards ≈ −12°, P5R ≈ −20°; REFERENCES.md §1). Paint-only
   *  like rotate/scale: animating skews never relayouts, and content
   *  pictures replay under the new transform. skewX slants verticals
   *  (positive leans the top to the right at negative... use negative
   *  values for the ATLUS lean); skewY slants horizontals. */
  Element &skewX(PropValue<float> degrees);
  Element &skewY(PropValue<float> degrees);
  // Integer-literal sugar (rotate(-8) etc. — int doesn't convert into the
  // PropValue variant on its own, and the resulting error is unreadable).
  // std::integral-constrained so FLOAT calls can never land here (a plain
  // int overload would capture them via the standard float→int conversion
  // and recurse); PropValue is constructed explicitly for the same reason.
  template <std::integral T> Element &opacity(T v) {
    return opacity(PropValue<float>((float)v));
  }
  template <std::integral T> Element &translateX(T v) {
    return translateX(PropValue<float>((float)v));
  }
  template <std::integral T> Element &translateY(T v) {
    return translateY(PropValue<float>((float)v));
  }
  template <std::integral T> Element &rotate(T deg) {
    return rotate(PropValue<float>((float)deg));
  }
  template <std::integral T> Element &scale(T f) {
    return scale(PropValue<float>((float)f));
  }
  template <std::integral T> Element &scaleX(T f) {
    return scaleX(PropValue<float>((float)f));
  }
  template <std::integral T> Element &scaleY(T f) {
    return scaleY(PropValue<float>((float)f));
  }
  template <std::integral T> Element &skewX(T deg) {
    return skewX(PropValue<float>((float)deg));
  }
  template <std::integral T> Element &skewY(T deg) {
    return skewY(PropValue<float>((float)deg));
  }
  Element &transformOrigin(float fx, float fy);
  /** Pixel-valued transform origin (node-local px) — for pivots that
   *  aren't a fraction of THIS node's box, e.g. zooming a window that
   *  lives inside a full-canvas overlay around its own center. */
  Element &transformOriginPx(SkPoint p);
  Element &zIndex(int z);

  // ---- derive phase (inputs are resolved geometry) ----
  /** Text leaves only: flow this paragraph around the keyed node's
   *  resolved bounds (SigilWeave ExclusionFlow), with @p margin px of
   *  standoff. Resolved as a bounded second layout pass; a reference
   *  to self or a descendant is ignored (cycle guard). Call repeatedly
   *  to weave around several elements. */
  Element &flowAround(std::string_view key, float margin = 0.0f);

  // ---- content ----
  /** Image leaves only: draw this sub-rect of the asset (atlas / sprite
   *  regions, in source pixels) instead of the whole image. Strictly
   *  constrained — neighboring atlas cells never bleed in. */
  Element &region(SkRect sourceRect);

  /** Kinetic typography on a text() element: a per-glyph effect staggered
   *  across the glyphs and driven by a master progress (see GlyphFx). */
  Element &glyphFx(GlyphFx fx);
  /** VariationDrive (text leaves): drive a variable-font axis from a
   *  bound Output at DRAW time — paint-only volatility, no reshape, no
   *  relayout. The paint phase probes the node's fonts once per content:
   *  an advance-variant axis (wght on most fonts) is REFUSED with a debug
   *  warning and the text draws at its shaped coordinates — drive GRAD
   *  (the advance-invariant weight) or re-render discretely instead. */
  Element &variationDrive(const char (&tag)[5],
                          const choreograph::Output<float> *value);

  /** Text leaves only: how lines sit inside the node's width (SigilWeave
   *  TextAlignment — kStart/kCenter/kEnd/kJustify). Meaningful when the
   *  node is WIDER than its text (explicit width, grow, stack stretch);
   *  intrinsic-width text has nothing to align within. */
  Element &textAlign(sigil::weave::TextAlignment a);

  /** Text leaves only: paint the GLYPHS with this material, mapped to
   *  TEXT-METRIC space — the material's unit square lands with x across
   *  the widest line and y from the first line's CAP TOP (real cap height
   *  from the face's metrics) to the last line's baseline. The chrome-type
   *  primitive (REFERENCES.md §2): author the sunset ramp once in [0,1]
   *  and its horizon crosses the capitals at any font size — no
   *  hand-positioned gradients. Supersedes the style's foreground paint;
   *  a live material re-resolves per frame (animated chrome); glyphFx
   *  wins when both are set (kinetic text draws its own buckets). */
  Element &textFill(Material m);

  /** Strokes the GLYPHS, under the fill — engraved display type, an
   *  outlined label, a caption that has to survive over an image.
   *
   *  `Element::stroke()` dresses the node's BOX outline, which is a
   *  different thing entirely, so thickening a face meant dropping to
   *  `sigil::weave::PaintStyle::addUnderlay` with a hand-built stroke
   *  paint. Three studies did it; one spelled "1 px outline plus offset
   *  shadow" as 117 full re-draws of a paragraph through `echo()`.
   *
   *  Composes with `textFill()` — the stroke is a pass beneath whatever
   *  fills the letterforms — and with the style's own underlays and
   *  overlays, which it joins rather than replaces. */
  Element &textStroke(float width, Fill fill);
  /** Text leaves only: lay the run out along a PATH instead of a line.
   *  See TextPath. Single-line runs; the node's own box still sizes the
   *  path, so give it the box the curve should be inscribed in
   *  (`util::disc`-style: width(2r).height(2r).centerAt(centre)).
   *
   *  Interacts with the rest of the text surface the way you would hope:
   *  the style's underlays, overlays and decorations all still draw, and
   *  glyphFx() wins if both are set (kinetic text draws its own buckets). */
  Element &onPath(TextPath spec);

  // ---- identity, caching, transitions ----
  /** The author-owned identity: what the reconciler matches a child by
   *  across describes, and what `connector`/`rail`/`spans::fit` borrow
   *  geometry by.
   *
   *  ON A `slot()` IT RENAMES THE MOUNT. A slot's name IS its key —
   *  there is no second field — so `slot("hud").key("panel")` produces a
   *  slot called "panel", and `renderSlot("hud")` then finds nothing and
   *  no-ops. Doing it warns once (Release too), because the symptom is a
   *  W × 0 layout, not an error (ROADMAP §26b's other half). */
  Element &key(std::string_view k);
  Element &cache(Cache c);
  /** Texture-bake resolution multiplier (Cache::Texture only; 0.1–1).
   *  The bake rasterizes at `factor` times the device scale and the blit
   *  scales it back up with linear sampling.
   *
   *  ALMOST ALWAYS THE WRONG LEVER — it cheapens the BAKE and taxes
   *  every BLIT (an upscaling resample, paid forever), which is
   *  backwards for the bake-once/blit-every-frame node Cache::Texture
   *  exists for. One study removed it from six nodes and went mean
   *  11.07 → 4.31 ms. Reach for it only when something forces FREQUENT
   *  re-bakes (a live material stepping at its own rate, a resizing
   *  node) AND the content is soft enough to survive the resample.
   *  Sharp text or 1px hairlines never belong under a reduced bake. */
  Element &bakeScale(float factor);
  Element &transition(Transition t); // node default for plain constants
  /** GSAP-style container stagger: child i's subtree enters with an EXTRA
   *  order·each delay on all its animate() mount transitions (compounding
   *  through nested staggered containers). `from` picks the origin —
   *  Start (declaration order), End (last child first — the P3R bottom-up
   *  cascade without reordering paint), Center (ripple outward). One call,
   *  no per-child delay arithmetic:
   *  `column().staggerChildren(33ms, Stagger::From::End).children(rows)`. */
  Element &staggerChildren(std::chrono::milliseconds each,
                           Stagger::From from = Stagger::From::Start);

  // ---- composition ----
  Element &child(Element e);
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, Element>
  Element &children(R &&range) {
    for (auto &&e : range)
      child(std::move(e));
    return *this;
  }

  /** @private reconciler access */
  const std::shared_ptr<detail::ElementNode> &node() const {
    return m_node.value;
  }
  explicit Element(std::shared_ptr<detail::ElementNode> n)
      : m_node(std::move(n)) {}

private:
  /** Register a decoration's declared derive borrows (see
   *  BorrowingDecoration). Called by EVERY slot that takes a Decoration —
   *  a borrow honoured by only some of them would be the sibling-path
   *  failure family again. */
  void claimBorrows(const Decoration &d);

  /** Copy-on-write handle: Element stays a cheap value, but fluent mutation
   *  can never alter another copy or a description retained by Composer. */
  struct NodeHandle {
    explicit NodeHandle(std::shared_ptr<detail::ElementNode> node)
        : value(std::move(node)) {}

    detail::ElementNode *operator->();
    const detail::ElementNode *operator->() const;

    std::shared_ptr<detail::ElementNode> value;
  };

  NodeHandle m_node;
};

// ---- factories -----------------------------------------------------------

Element box();
/** Overlap container: children share the box, painted in (zIndex,
 *  declaration order). EVERY child is absolute — the container sets it
 *  after the child's own layout props, so a child cannot rejoin the flex
 *  flow from inside a stack (it keeps its insets, which is what absolute
 *  is for: `.top(12).right(12)` pins a corner). Mixed flow wants a box
 *  with a stack inside it. */
Element stack();
Element text(std::u8string utf8, sigil::weave::TextStyle style);
/** Full-control text: a prebuilt Paragraph (spans, mixed styles) plus
 *  ParagraphLayoutOptions (justification, hyphenation, Knuth–Plass,
 *  overflow…). The paragraph is shared by reference: reuse one
 *  shared_ptr across renders to keep shaping caches warm; a fresh
 *  pointer means "content changed" and re-shapes. */
Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
             sigil::weave::ParagraphLayoutOptions options = {});
Element image(std::shared_ptr<const sigil::image::ImageAsset> asset);
/** A box whose content is one paint program (≡ box().background(p)).
 *  Cached like any static subtree — programs that read the clock or
 *  otherwise change per frame must declare .cache(Cache::None). The program
 *  is an incomparable callable, so the structural prune cannot prove a
 *  custom() node unchanged: it re-records on every render(). Wrap it in
 *  memo() (or keep its Element pointer-stable across renders) to prune it
 *  while its inputs are unchanged. Value decorations (PathFormat/Slice/
 *  Shadow) do prune automatically — prefer them for static chrome.
 *
 *  IT SIZES LIKE AN EMPTY BOX, and the failure is silent. Being literally
 *  `box().background(p)`, a custom() leaf has no intrinsic size: dropped
 *  into an `absolute().inset(0)` parent it stretches on the cross axis and
 *  measures ZERO on the main one, so the program runs against a
 *  zero-height context and draws nothing at all. Give it dims, or make it
 *  `absolute().inset(0)` itself — which is exactly what
 *  `instancing::instances()` returns, for exactly this reason. */
Element custom(PaintProgram program);

/** A container whose children are placed by @p scheme instead of
 *  flexbox (nests freely inside flex and vice versa). The container
 *  itself is sized by its own dims/flex; children are measured by
 *  Yoga/SigilWeave, then positioned and sized by scheme.place() in a
 *  bounded second layout pass. */
template <LayoutScheme L> Element layout(L scheme);

namespace detail {
Element makeLayout(
    std::function<std::vector<SkRect>(const LayoutInput &)> place);
} // namespace detail

template <LayoutScheme L> Element layout(L scheme) {
  return detail::makeLayout(
      [s = std::move(scheme)](const LayoutInput &in) { return s.place(in); });
}

/** A named mount point whose content is supplied independently via
 *  Composer::renderSlot() — the surrounding tree's caches stay valid
 *  across slot updates (independent data domains).
 *
 *  THE NAME IS STORED AS THE ELEMENT'S `key`, which is the same field
 *  `.key()` writes: `slot("hud").key("panel")` is a slot named "panel"
 *  and `renderSlot("hud")` silently finds nothing. Name the slot here,
 *  never twice; `.key()` warns once if it is called on one anyway. */
Element slot(std::string_view name);

/** A relationship as a first-class element: a path routed between two
 *  keyed nodes' resolved bounds, stroked by the connector's foreground
 *  decorations (attach a PathFormat — the routed path arrives as
 *  PaintContext::outline). Straight line by default; supply a router
 *  for anything else. Position it absolute().inset(0) over the nodes
 *  it connects. */
using Router = std::function<SkPath(const SkRect &from, const SkRect &to)>;
Element connector(std::string_view fromKey, std::string_view toKey,
                  Router router = {});

/** A rail endpoint/waypoint: a NORMALIZED point on a keyed node's resolved
 *  bounds ((0,0)=top-left, (1,1)=bottom-right — the binding form tldraw and
 *  Excalidraw both converged on; never absolute coordinates, so rails
 *  survive layout, drag, and reflow). `gap` pulls a TERMINAL anchor back
 *  along its segment (breathing room at the ends; ignored on waypoints). */
struct Anchor {
  std::string nodeKey;
  SkPoint norm = {0.5f, 0.5f};
  float gap = 0.0f;
  bool operator==(const Anchor &) const = default;
};

/** Routes an ordered run of resolved anchor points into the rail's path —
 *  stock ones in <sigilcompose/Routers.h> (polyline, octilinear); write your
 *  own for anything else. Straight polyline when omitted. */
using RailRouter = std::function<SkPath(std::span<const SkPoint>)>;

/** The component that IS a line: a path threaded through an ordered span of
 *  anchors (a transit line through its stations, a wire through ports),
 *  resolved in the derive phase and re-routed whenever an anchored node
 *  moves. The routed path becomes PaintContext::outline, so PathFormat
 *  strokes, ContourWalk stamps, and trim() all dress it — a rail with
 *  `.trim(0, with(1.0f, {800ms}))` DRAWS ITSELF. Position it
 *  absolute().inset(0) over the nodes it threads (like connector()). */
Element rail(std::vector<Anchor> anchors, RailRouter router = {});

/** A BAND: the shape a spine sweeps out at a given width across it.
 *
 *      band(shapes::circle(), across(22)).inward().fill(brass)
 *      band(around("dial"), across(14)).stroke(spans::edges(6), rule)
 *
 *  It is an ordinary element in every way that matters — it lays out,
 *  hosts children, fills, clips and takes stroke passes like any other
 *  shape. What it adds is that its shape is DERIVED: it owns an
 *  (along, across) space over its spine, `along` a fraction of arc length
 *  and `across` px on the normal (see bandPointAt for the sign), and
 *  `across(...)` takes a Profile, so a taper is the same value a strand
 *  or a ribbon width uses.
 *
 *  It does NOT hit-test as its shape: hitTest consults shapeFn, and a
 *  band's silhouette is derived instead, so a band hits as its LAYOUT
 *  BOX. That is the pinned organic-shape hit-testing pass (§33), not a
 *  band defect to work around here.
 *
 *  An authored spine is an incomparable callable, exactly like shape()'s
 *  generator — memo() such a node (or keep the generator pointer-stable)
 *  to prune it while its size and inputs are unchanged. A borrowed spine
 *  (`around(key)`) is a comparable value and prunes on its own.
 *
 *  Formation is explicit: `.centered()` (the default) straddles the
 *  spine, `.outward()` and `.inward()` take one side. The spine is guide
 *  DATA, never an element — a path participates as an element's shape, as
 *  borrowed geometry (`around(key)`, resolved in the derive phase), or as
 *  pure guide data in no tree, and this is the third case.
 *
 *  The profile's `max()` is what the paint cull grows by, so a band whose
 *  width varies is never silently clipped. */
Element band(std::function<SkPath(SkSize)> spine, Across width);
Element band(Around spine, Across width);

/** The band's own (along, across) space, addressable: `along` is a
 *  fraction of the spine's total arc length, `across` is px on the normal.
 *
 *  **Positive `across` is to the LEFT of travel**, which in screen space
 *  (y down) is OUTSIDE a clockwise path — SkPath's own direction for
 *  rects and circles, so `.outward()` exits the shape.
 *
 *  That is the NEGATION of `lines::offsetAlong` and `lines::Rail::offset`,
 *  both of which offset RIGHT of travel. The split is not new and the band
 *  did not invent it: `TextPath::offset` (this header, above) has always
 *  been left-of-travel-is-outward, so the KERNEL says left and the LINES
 *  extension says right. The band follows the kernel, and §33 ruling 5
 *  makes that the answer: **LEFT wins everywhere and the lines:: members'
 *  right-of-travel sign dies in R3.** Until the R2 port flips them (a
 *  sign flip moves pixels, so it does not ride this additive phase), do
 *  not assume a profile means the same side in both.
 *
 *  The one place the two coordinates are spelled out, so a caller placing
 *  content on a band and the band's own geometry cannot disagree. */
SkPoint bandPointAt(const SkPath &spine, float along, float acrossPx);

// ---------------------------------------------------------------------------
// The DERIVE family, gathered under one word

/** Everything that asks "where did that keyed node land, and give me more
 *  content because of it" — the DERIVE phase, which is already the
 *  pipeline's name for it (DESIGN.md §The pipeline) and therefore canon
 *  vocabulary rather than a coinage.
 *
 *  The members shipped separately over a year and shared no name, which
 *  is the audit's item 8: `flowAround`, `connector`/`rail` (with
 *  `routers::` as their pluggable seam, not a seventh member),
 *  `band(around(key))`, `spans::fit(key)` and a weave's
 *  `strand::from(key)` are ONE mechanism with six spellings and ~55 total
 *  uses — the low churn IS the symptom. Aliases only; nothing moves.
 *
 *  THE LAWS THEY SHARE — one flat edge store, walked once per render:
 *
 *   1. **An unknown key is SILENT.** `flowAround("typo")`,
 *      `spans::fit("typo")`, `around("typo")`, a connector to a node that
 *      is not in the tree — every one resolves to nothing and draws
 *      nothing. There is no diagnostic, by precedent and consistency; a
 *      warning would have to be the whole family's at once.
 *   2. **ONE SECOND PASS, cycle-guarded.** Backward influence inside a
 *      frame is this declared exception and nothing else: derive answers
 *      are computed from the FIRST layout and fed to at most one more
 *      pass. A borrow that would close a cycle is dropped, not chased.
 *   3. **The answer can lag by a frame** where the borrowed node's own
 *      geometry only settles during that layout — the second `frame()`
 *      in the fit/flow tests is that, not a test artefact.
 *   4. **Flat, not recursive.** Routed nodes and flowing text are flat
 *      lists in tree order plus a back-index from anchor key to routes,
 *      so a tree with no derived content pays nothing and `routesAt(key)`
 *      answers in O(routes at that node).
 */
namespace derive {
/** A relationship as an element — see connector() above. */
using sigil::compose::connector;
/** A path threaded through anchors — see rail() above. */
using sigil::compose::rail;
/** A spine borrowed from a keyed element — `band(derive::around("dial"),
 *  across(14))`. */
using sigil::compose::around;
/** The family's text member as a free verb, so the whole family has one
 *  spelling: `derive::flowAround(el, "fig", 8)` == `el.flowAround("fig",
 *  8)`. The METHOD stays the ergonomic form (it chains); this exists so
 *  the family can be named and found in one place. */
inline Element flowAround(Element el, std::string_view key,
                          float margin = 0.0f) {
  el.flowAround(key, margin);
  return el;
}
} // namespace derive

/** One-shot element render: reconciles, lays out, and records the
 *  paint into a picture. With an empty @p maxSize the tree takes its
 *  intrinsic (content) size; a non-empty one bounds it (root max
 *  dims). Bindings are sampled at their current values; transitions
 *  don't run — there is no live timeline. This is the bake primitive
 *  behind ContourWalk element stamps, and generally "an element tree
 *  as a brush". */
sk_sp<SkPicture> snapshot(Element root, sigil::weave::FontContext &fonts,
                          SkSize maxSize = SkSize::MakeEmpty());

/** A face's vertical metrics at a given size, without laying anything out.
 *
 *  The most-used missing primitive in the study program, and the reason
 *  is a mismatch nobody documents: a compose text node's top is the LINE
 *  BOX top, while almost every artefact worth reconstructing positions
 *  type by its CAP TOP. Aligning a rebuild to a reference therefore needs
 *  the slack between the two, and `measure()` returns only an `SkSize` —
 *  so the Fallout 2 sheet inferred it as an empirical
 *  `0.20 × measure("H").height()` across ~134 runs and three faces, which
 *  happened to work and was a guess.
 *
 *  `capHeight` and `xHeight` are what a face reports; both fall back to a
 *  fraction of the ascent when it reports zero, which some faces do.
 *  Values are positive distances in px, `ascent` above the baseline. */
struct TextMetrics {
  float ascent = 0;     ///< baseline to the top of the em box (positive)
  float descent = 0;    ///< baseline to the bottom (positive)
  float leading = 0;    ///< the face's recommended extra line gap
  float capHeight = 0;  ///< baseline to the top of a flat capital
  float xHeight = 0;    ///< baseline to the top of a lowercase x
  float lineHeight = 0; ///< ascent + descent + leading
  /** How far the line box's top sits above the cap top — the number that
   *  turns "place this at the reference's y" into a coordinate. */
  float capSlack() const { return ascent - capHeight; }
};

TextMetrics metrics(const sigil::weave::TextStyle &style,
                    sigil::weave::FontContext &fonts);

/** One-shot intrinsic measurement: what size would this element take?
 *  Runs the same reconcile+layout as snapshot() and returns the root's
 *  resolved size without painting. The sizing primitive behind
 *  content-fit chrome (marquees, tooltips, badges): measure the content,
 *  then describe the real tree with the answer. Same sampling rules as
 *  snapshot() — bindings at current values, no transitions. */
SkSize measure(Element root, sigil::weave::FontContext &fonts,
               SkSize maxSize = SkSize::MakeEmpty());

namespace detail {
Element makeMemo(std::any props,
                 std::function<bool(const std::any &, const std::any &)> equal,
                 std::function<Element(const std::any &)> invoke);
} // namespace detail

/** Deferred description: `fn` runs only when `props` changed (by
 *  operator==) since the last render on this position/key. */
template <ComponentProps P, ComponentFn<P> F> Element memo(P props, F fn) {
  return detail::makeMemo(
      std::any(std::move(props)),
      [](const std::any &a, const std::any &b) {
        return std::any_cast<const P &>(a) == std::any_cast<const P &>(b);
      },
      [fn = std::move(fn)](const std::any &p) -> Element {
        return fn(std::any_cast<const P &>(p));
      });
}

// ---------------------------------------------------------------------------
// Composer — the retained side; a guest in the host's canvas

class Composer {
public:
  /** @p fontContext outlives the composer; @p ticker drives transitions
   *  and (via its FrameClock, when attached) PaintContext time. */
  Composer(motion::Ticker &ticker, sigil::weave::FontContext &fontContext);
  ~Composer();

  Composer(const Composer &) = delete;
  Composer &operator=(const Composer &) = delete;

  /** Layout viewport in canvas-space px; percent dims resolve here.
   *  The root element always fills the viewport (its own width/height
   *  are ignored, like the CSS root) — size content via children.
   *  An EMPTY size means INTRINSIC instead: the root sizes to its
   *  content and its own dims ARE respected (Layout.cpp) — the rule the
   *  snapshot()/measure() path runs under. */
  void setSize(SkSize size);

  /** Feeds PaintContext::elapsedSeconds (one clock everywhere). Null
   *  freezes paint time at 0 — fine for static content and goldens. */
  void setClock(const motion::FrameClock *clock);

  /** Output view transform (color management): applied to the composer's
   *  whole output as the final stage — one saveLayer while set, zero cost
   *  when cleared (a default Effect{}). The intended source is an OCIO
   *  display/view baked to a 3D LUT (<sigilcompose/Ocio.h>), but any Effect
   *  works. Per-node caches are unaffected (this is post-cache, at
   *  composite). */
  void setView(Effect view);

  /** Reconciles against the retained tree (keys match instances; memo
   *  and payload identity prune). Call whenever data changed. */
  void render(Element root);

  /** Updates only the named slot() mount point (layout and stacking
   *  still integrate normally; ancestors re-record their caches, the
   *  rest of the tree is untouched). No-op if the slot doesn't exist. */
  void renderSlot(std::string_view name, Element content);

  /** Content or layout changed since the last draw(). Redraw when
   *  dirty() || ticker.active(). */
  bool dirty() const;

  /** Lays out if needed and paints at the canvas's current matrix/clip.
   *  Provably-static subtrees replay their auto-recorded pictures. */
  void draw(SkCanvas &canvas);

  /** Drops every per-node cache (auto pictures, Cache::Texture bakes,
   *  held live-material shaders) and marks the tree for a full repaint.
   *  GPU hosts call this on device loss or a backend switch: cached
   *  images minted by a dead context must not replay onto the next
   *  canvas. The retained tree, layout, and animations are untouched. */
  void purgeCaches();

  // ---- queries (resolved side only) ----
  /** Layout rect of a keyed node, in the composer's coordinate space
   *  (valid after draw()/layout). */
  std::optional<SkRect> bounds(std::string_view key) const;
  /** Live SigilWeave layout of a keyed text node (valid until the next
   *  layout; for glyph choreography and queries). */
  const sigil::weave::ParagraphLayout *
  paragraphLayout(std::string_view key) const;
  /** Topmost key at a canvas-space point (valid after draw()/layout).
   *  Paint-order aware (zIndex, declaration order, topmost first),
   *  transform-aware (rotated/scaled/translated nodes hit in their
   *  visual place), and shape-aware (custom outlines and corner radii
   *  bound the hit region — the gap between a star's arms misses).
   *  A keyless node hit resolves to its nearest keyed ancestor;
   *  clipped subtrees don't hit outside their clip. */
  std::optional<std::string> hitTest(SkPoint canvasPoint) const;
  /** The edge store's back-index: keys of route elements (connector()/
   *  rail()) anchored on @p nodeKey, in tree order — the graph query
   *  ("which edges touch this node") for hover highlights and pruned
   *  updates. Keyless routes are anchored but unaddressable, so they are
   *  omitted; give routes keys to see them here. Valid after render(). */
  std::vector<std::string> routesAt(std::string_view nodeKey) const;

  // ---- introspection (perf verification; see compose_bench) ----
  struct Stats {
    size_t instances = 0;       ///< live retained nodes
    size_t describedNodes = 0;  ///< element nodes visited last render()
    size_t memoHits = 0;        ///< memo props equal → describe skipped
    size_t patchedNodes = 0;    ///< instances whose props changed
    size_t picturesLive = 0;    ///< auto-cached subtree pictures held
    size_t texturesLive = 0;    ///< Cache::Texture images held
    /** CACHE WRITES last draw() — every recording AND every pixel bake.
     *
     *  The name is narrower than the number and has misled a reader: a
     *  `Cache::Texture` bake and a promoted bake both count here too, so
     *  this is "how much cache work did that frame do", which is what
     *  every caller actually wants to know. `texturesBaked` breaks out
     *  the pixel-bake subset. */
    size_t picturesRecorded = 0;
    size_t texturesBaked = 0;   ///< of those, bakes rather than recordings
    size_t nodesPainted = 0;    ///< instances painted live last draw()
    // Per-phase wall time, so a slow frame localizes at a glance. The paint
    // number is where per-pixel cost lives (live materials, re-records);
    // reconcile/layout/volatile are the retained machinery.
    double reconcileMs = 0;     ///< render()/renderSlot() since previous draw()
    double layoutMs = 0;        ///< ensureLayout() inside last draw()
    double volatileMs = 0;      ///< computeVolatile() walk inside last draw()
    double paintMs = 0;         ///< paint traversal inside last draw()
  };
  const Stats &stats() const;

  /** PER-NODE PAINT COST — the instrument that did not exist.
   *
   *  `stats().paintMs` says the frame spent 1192 ms painting and says
   *  nothing about WHERE, and nine studies were authored with no way to
   *  find out. `debug::coverage` is a geometric path-tiling check and has
   *  never had anything to do with cost, so the advice to reach for it was
   *  wrong. This is the replacement.
   *
   *      composer.setProfiling(true);
   *      composer.draw(canvas);
   *      for (const auto &row : composer.profile())   // worst first
   *        printf("%7.2f ms  %s\n", row.selfMs, row.label.c_str());
   *
   *  `selfMs` EXCLUDES children, so the number lands on the node that
   *  actually costs, not on its ancestors. `cached` reports whether the
   *  node replayed a cached picture — and note that a cached node can
   *  still be the most expensive thing on the sheet, because a picture
   *  records the DRAW CALLS: replaying it re-runs every shader over every
   *  pixel. That is the distinction the corpus got wrong, and it is
   *  visible here as a node with `cached == true` and a large `selfMs`.
   *
   *  Off by default: the timing calls are cheap but not free, and a
   *  profiler that is always on is a profiler nobody trusts. */
  /** How a node produced its pixels this frame. The distinction between
   *  Picture and Texture is the one the corpus got wrong, so the profiler
   *  names it rather than saying "cached". */
  enum class CacheState : uint8_t {
    Live,     ///< painted from scratch
    Picture,  ///< replayed a recording — RE-RUNS every shader, every pixel
    Texture,  ///< blitted a raster bake — the author asked for it
    Promoted, ///< blitted a raster bake the LIBRARY decided to make
    /** Blitted the node's OWN paint and drew its live children over it.
     *  Volatility is declared per node, so a static ground plane carrying
     *  one moving child shares the child's verdict and loses; this state
     *  says the two were separated. */
    SplitOwn,
    /** Blitted a whole-subtree bake held by `Cache::Group`'s value memo:
     *  the node AND its animated children, composited once into one
     *  unrotated device layer while every bound scalar below holds still.
     *  Distinct from Texture because the thing being asserted is
     *  different — a Texture node is provably static, a Group node is
     *  provably NOT CHANGING RIGHT NOW, and the difference is one frame. */
    Group,
  };
  /** WHY a node is, or is not, a pixel bake.
   *
   *  A node that reads `live paint` and costs 600 ms is the corpus's whole
   *  problem, and "the library declined" is not an answer an author can act
   *  on: the refusals are individually correct and individually invisible.
   *  So every profiled node carries the reason, and `--bench` prints it.
   *  Each Refused* value names a condition that would make a bake produce
   *  DIFFERENT PIXELS, which is the one thing promotion may never do. */
  enum class Promotion : uint8_t {
    Cheap,      ///< under the cost threshold — promoting it would not pay
    Warming,    ///< expensive, counting the consecutive frames before a bake
    Promoted,   ///< baked by the library
    AskedFor,   ///< Cache::Texture — the author's own bake, not a decision
    OptedOut,   ///< Cache::Picture / Cache::None, or promotion switched off
    Volatile,   ///< its content genuinely changes every frame
    Composited, ///< opacity < 1 or a non-srcOver blend: a bake would round twice
    Transformed,///< rotated, skewed or mirrored — a bake would resample
    Filtered,   ///< layer/backdrop effect or clip on the node itself
    /** Something in the subtree composites against what is already on the
     *  canvas — a non-srcOver blend or a backdrop filter, on this node or
     *  any descendant. A bake would resolve it against transparent black.
     *  Separated from Filtered because the remedy is different: a clip is
     *  the author's own node to change, whereas this can be a blend three
     *  levels down that they will not find without being told. */
    ReadsBackdrop,
    TooBig,     ///< beyond the per-bake area cap or the composer's bake budget
    /** The node's OWN paint is baked and its volatile children are painted
     *  live over the blit. Not a refusal — the outcome for a node whose
     *  static self was being re-rasterized every frame to redraw a moving
     *  child on top of it. */
    SplitBaked,
  };
  struct NodeCost {
    std::string label;   ///< key() if set, else kind + size — actionable
    double selfMs = 0;   ///< this node's own paint, EXCLUDING children
    double totalMs = 0;  ///< including children
    int depth = 0;
    CacheState cacheState = CacheState::Live;
    Promotion promotion = Promotion::Cheap;
    /** EVERY condition that refused a bake, not just the first one.
     *
     *  `promotion` is a first-match verdict, so a node that is both
     *  volatile and clipped reports only `Volatile` — and an author who
     *  fixes the volatility then meets a second refusal nobody mentioned.
     *  That is honest and it costs an iteration each time. The mask carries
     *  all of them at once; `promotion` stays the primary outcome so every
     *  existing assertion on it keeps its meaning.
     *
     *  The bit index IS the Promotion ordinal, so there is no second table
     *  to drift out of sync with the enum. */
    uint16_t refusals = 0;
    bool refused(Promotion p) const {
      return (refusals & (uint16_t)(1u << (unsigned)p)) != 0;
    }
    bool cached() const { return cacheState != CacheState::Live; }
  };
  /** One short phrase for a Promotion, for printing next to a cost. */
  static const char *promotionReason(Promotion p);
  void setProfiling(bool on);
  bool profiling() const;
  /** Rows from the last draw(), sorted by `selfMs` descending. Empty when
   *  profiling is off. */
  const std::vector<NodeCost> &profile() const;

  /** AUTOMATIC TEXTURE PROMOTION (on by default on CPU raster; OFF by
   *  default on a Graphite/GPU surface — the cost model that drives it
   *  measures op-RECORDING time, which describes raster and not GPU,
   *  and promotion measured inert there; see ComposeRuntime.h and
   *  ROADMAP §29. This setter overrides in both directions).
   *
   *  A `Cache::Auto` subtree that is provably static already caches as an
   *  SkPicture — and a picture records the DRAW CALLS, so replaying it
   *  re-runs every shader over every pixel forever. Nine studies in the
   *  sketch corpus were authored believing that was a cache; every one of
   *  them reported `picturesRecorded == 0` while missing 60 FPS by an
   *  order of magnitude.
   *
   *  So the composer now watches how long each static node's paint
   *  actually costs, and once a node has been expensive for several
   *  consecutive frames it re-bakes that subtree ONCE into a raster image
   *  and blits it thereafter.
   *
   *  THREE KINDS OF NODE ARE ELIGIBLE:
   *
   *  1. A cached subtree whose picture replay is expensive.
   *  2. A LEAF that never records a picture at all. Bare boxes are excluded
   *     from picture recording on purpose (one drawRect beats a nested
   *     recording), and the promoter used to watch only the replay path —
   *     so a full-canvas box carrying one grain shader was structurally
   *     invisible to it. That node is the corpus's single most expensive
   *     object: 663 ms of a 697 ms frame in chladni_tab1, 108 ms in
   *     penrose_paving, 34 ms in genesis_fire. Leaves are measured now.
   *  3. A node whose only volatility is a LIVE MATERIAL that has not
   *     actually moved since the bake — `Material::quantizeTime(10)` steps
   *     its uniforms ten times a second, so at 60 FPS five frames in six
   *     resolve to the SAME shader and the previous bake is still exact.
   *     A material bound to a continuous Output resolves to a new shader
   *     every frame, never reaches the stability rate, and stays live: the
   *     library measures that rather than assuming it.
   *
   *  Re-baking is not free, so a node only holds its promotion while it is
   *  actually stable — a bake per frame would cost more than the replay it
   *  replaced.
   *
   *  IT MUST NOT CHANGE A PIXEL, and that is enforced structurally rather
   *  than hoped for: promotion is refused unless the node maps to device
   *  space with no rotation, mirroring or skew, and the bake is then taken
   *  in DEVICE space at an integer-snapped rect and blitted back with the
   *  matrix reset and no resampling. An integer device-space translation
   *  cannot alter rasterisation, so the blit is a literal copy of the
   *  pixels the live paint would have produced. Anything outside that
   *  envelope keeps painting as it did.
   *
   *  The refusals that look most like missed wins are the honest ones. A
   *  leaf at `opacity(0.13).blend(kSoftLight)` — the paper-grain idiom,
   *  and the most expensive node in three studies — cannot be promoted,
   *  because compositing a bake applies the alpha to an already-rounded
   *  8-bit colour and the direct draw applies it to the shader's float
   *  output; the two agree to within 1 LSB, which is not agreement. Ask
   *  for that one yourself with `.cache(Cache::Texture)`: an author who
   *  types it has accepted the rounding, and the library has not.
   *
   *  Why a given node was or was not promoted is reported per node as
   *  NodeCost::promotion, and `ComposeSketch --bench` prints it.
   *
   *  Opting out: globally here, or per node with `.cache(Cache::Picture)`,
   *  which means "record, and never promote". `Cache::Texture` is the
   *  opposite opt-in and is unaffected. */
  void setAutoTexturePromotion(bool on);
  bool autoTexturePromotion() const;

  /** @private */
  struct Impl;

private:
  friend struct detail::Instance;
  friend sk_sp<SkPicture> snapshot(Element, sigil::weave::FontContext &,
                                   SkSize);
  friend SkSize measure(Element, sigil::weave::FontContext &, SkSize);
  std::unique_ptr<Impl> m_impl;
};

} // namespace sigil::compose
