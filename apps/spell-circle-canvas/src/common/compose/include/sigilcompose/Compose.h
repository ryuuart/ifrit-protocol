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
 *  `delay` holds the CURRENT value (the `from`, for withFrom entrances)
 *  before the ramp starts — the stagger primitive: describe a battery of
 *  children whose delays step by 60–90ms and the cascade is data, not
 *  bookkeeping (the §8 stagger law at the Element level). */
struct Transition {
  std::chrono::milliseconds duration{250};
  choreograph::EaseFn ease = &choreograph::easeOutQuad;
  std::chrono::milliseconds delay{0};
};

template <typename T> struct Transitioned {
  T value;
  Transition spec;
  /** withFrom(): where the value ENTERS from when the node first mounts.
   *  Empty for plain with() — no entrance, only change transitions. */
  std::optional<T> from{};
  /** withKeyframes(): the mount-time path as (absolute time, value)
   *  pairs — multi-segment entrances (damped overshoots) that one
   *  from→to ramp can't shape. Mount-only choreography: later changes
   *  retarget to `value` like any with(). */
  std::vector<std::pair<std::chrono::milliseconds, T>> waypoints{};
};

/** Wraps a constant so changes to it transition (see API.md semantics:
 *  one motion per (instance, property), retarget-from-current). */
template <typename T> Transitioned<T> with(T value, Transition spec) {
  return {std::move(value), std::move(spec)};
}

/** The MOUNT transition (CSS animation-on-enter / GSAP from()): when the
 *  node first appears it plays `from → to` over `spec`; afterwards it
 *  behaves exactly like with(to, spec) — later changes retarget from the
 *  current value, and re-describing the same withFrom() prunes clean.
 *  `.opacity(withFrom(0.0f, 1.0f, {400ms}))` is the entrance idiom; it
 *  works on every float slot (opacity/transforms/skew/trim/glyph
 *  progress) and on color fills (a mount-time color sweep). snapshot()
 *  ignores entrances — a bake renders the settled value. */
template <typename T> Transitioned<T> withFrom(T from, T to, Transition spec) {
  return {std::move(to), std::move(spec), std::move(from)};
}

/** The MOUNT keyframe path (GSAP keyframes): absolute (time, value)
 *  waypoints played through on first appearance — the damped-overshoot
 *  entrances one ramp can't shape (P3R's cursor: +40 → −20 → +10 → 0):
 *
 *    .translateX(withKeyframes<float>(
 *        {{0ms, 40}, {200ms, -20}, {300ms, 10}, {400ms, 0}}))
 *
 *  `ease` applies per segment. A leading time > 0 holds the first value
 *  (a built-in delay; Transition::delay and staggerChildren() still add
 *  on top). After the path completes the value behaves like with(last):
 *  changes retarget-from-current; identical re-describes prune;
 *  snapshot()/measure() render the settled end value. */
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

/**
 * Every animatable property accepts one of: a constant (snaps, or ramps
 * under a node-level transition), a constant with its own transition, or
 * a live Choreograph binding stepped by the Ticker (paint-only; the
 * caller owns the Output and composes motions on ticker.timeline()).
 */
template <typename T>
using PropValue =
    std::variant<T, Transitioned<T>, const choreograph::Output<T> *>;

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
  bool animating = false;
  sigil::weave::FontContext *fonts = nullptr;
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
 *  bodies. An optional `bool animated() const` declares per-frame
 *  volatility (the single declared-volatility rule). */
template <typename D>
concept DecorationScheme =
    requires(const D &d, SkCanvas &canvas, const PaintContext &ctx) {
      { d.paint(canvas, ctx) };
    };

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

/** Type-erased decoration: the kernel seam extension primitives
 *  (PathFormat, Slice, ContourWalk — see Decorations.h) plug into. A
 *  bare PaintProgram works too. */
class Decoration {
public:
  template <DecorationScheme D>
  Decoration(D scheme) // NOLINT: implicit by design
      : m_animated([&] {
          if constexpr (AnimatedDecoration<D>)
            return scheme.animated();
          else
            return false;
        }()),
        m_bleed([&] {
          if constexpr (BleedingDecoration<D>)
            return (float)scheme.bleed();
          else
            return 0.0f;
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
  bool animated() const { return m_animated; }
  float bleed() const { return m_bleed; }

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
 *  wasteful for sparse regions); None opts a node out (per-frame paint
 *  programs that read the clock MUST declare this — declared
 *  volatility). */
enum class Cache : uint8_t { Auto, Picture, Texture, None };

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

  // ---- shape (defines PaintContext::outline and clipping) ----
  Element &corners(Corners c);
  /** Trim the node's painted outline to the [start, end] fraction of its
   *  arc length (the Lottie/sksg Trim Path — SkTrimPathEffect underneath).
   *  Applies to the fill surface and every outline-following decoration
   *  (PathFormat strokes, ContourWalk), so a stroked border with
   *  `.trim(0, with(1.0f, {600ms}))` DRAWS ON, and a connector's wire can
   *  reveal along its route. Both ends take the full PropValue treatment —
   *  plain, with() transitions, or ch::Output bindings (bound/animating trim
   *  is content volatility: the node paints live while moving). `offset`
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
  /** Custom outline: a path generator over the node's laid-out size,
   *  in local coordinates. Overrides corners() as the node's shape —
   *  the fill surface, clip(), and every outline-following decoration
   *  (PathFormat strokes, ContourWalk) trace it. Spiky dialogs,
   *  scalloped frames, any non-rectangular chrome. Like custom(), the
   *  generator is an incomparable callable — memo() such a node (or keep it
   *  pointer-stable) to prune it while its size and inputs are unchanged. */
  Element &outline(std::function<SkPath(SkSize)> shape);
  /** Clip fill, content, and children to the node's shape. Decorations
   *  are NOT clipped — they dress the outline (outer strokes, shadows,
   *  glows keep their reach); hit-testing still bounds the subtree. */
  Element &clip(bool on = true);

  // ---- paint ----
  Element &fill(PropValue<Fill> f);
  /** Fill with a Material (gradient ramp, blend stack, sprite, SkSL) — the
   *  richer authoring value. A static Material collapses to a Fill, so it
   *  caches and prunes on the same path. See <sigilcompose/Material.h>. */
  Element &fill(Material m);
  /** Solid-color sugar: fill({r,g,b,a}) without the Fill:: ceremony. */
  Element &fill(SkColor4f color) { return fill(PropValue<Fill>{Fill::color(color)}); }
  /** Decoration layers: backgrounds paint below content/children (in
   *  declaration order), foregrounds above; repeated calls APPEND (the
   *  Photoshop stacked-strokes model — two stroke() calls are two rings).
   *  fill() is the transitionable first background; custom() is a box
   *  with one background program. Decorations dress the OUTLINE: clip()
   *  does not clip them (it bounds fill/content/children only), so outer
   *  strokes and shadows survive on clipped nodes. */
  Element &background(Decoration d);
  Element &foreground(Decoration d);
  /** fill's peer (the Photoshop/Illustrator mental model): dress the
   *  node's OUTLINE with a brush — a PathFormat, a layered brush stack,
   *  any decoration that strokes. Pure sugar for foreground(), named for
   *  what it means at the call site. */
  Element &stroke(Decoration brush);
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
   *  to self or a descendant is ignored (cycle guard). */
  /** Text-only: flow this paragraph around the keyed element's resolved
   *  bounds (derive phase). Call repeatedly to weave around several
   *  elements; `margin` applies to all of them. */
  Element &flowAround(std::string_view key, float margin = 0.0f);

  // ---- content ----
  /** Image leaves only: draw this sub-rect of the asset (atlas / sprite
   *  regions, in source pixels) instead of the whole image. Strictly
   *  constrained — neighboring atlas cells never bleed in. */
  Element &region(SkRect sourceRect);

  /** Kinetic typography on a text() element: a per-glyph effect staggered
   *  across the glyphs and driven by a master progress (see GlyphFx). */
  Element &glyphFx(GlyphFx fx);

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

  // ---- identity, caching, transitions ----
  Element &key(std::string_view k);
  Element &cache(Cache c);
  /** Texture-bake resolution multiplier (Cache::Texture only; 0.1–1).
   *  The bake rasterizes at `factor` times the device scale and the blit
   *  scales it back up with linear sampling — Chrome's reduced
   *  raster-scale trade, for planes whose content is soft anyway
   *  (blurred glass, watercolor shader walls): bakeScale(0.5f) quarters
   *  the pixels each re-bake evaluates. Sharp text or 1px hairlines do
   *  NOT belong under a reduced bake. */
  Element &bakeScale(float factor);
  Element &transition(Transition t); // node default for plain constants
  /** GSAP-style container stagger: child i's subtree enters with an EXTRA
   *  order·each delay on all its withFrom() mount transitions (compounding
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
/** Overlap container: children share the box (absolute by default),
 *  painted in (zIndex, declaration order). */
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
 *  Shadow) do prune automatically — prefer them for static chrome. */
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
 *  across slot updates (independent data domains). */
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

/** One-shot element render: reconciles, lays out, and records the
 *  paint into a picture. With an empty @p maxSize the tree takes its
 *  intrinsic (content) size; a non-empty one bounds it (root max
 *  dims). Bindings are sampled at their current values; transitions
 *  don't run — there is no live timeline. This is the bake primitive
 *  behind ContourWalk element stamps, and generally "an element tree
 *  as a brush". */
sk_sp<SkPicture> snapshot(Element root, sigil::weave::FontContext &fonts,
                          SkSize maxSize = SkSize::MakeEmpty());

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
   *  are ignored, like the CSS root) — size content via children. */
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
    size_t picturesRecorded = 0;///< recordings performed last draw()
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
