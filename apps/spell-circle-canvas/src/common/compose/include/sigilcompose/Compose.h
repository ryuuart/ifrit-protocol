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
 * an sigil::tick::Ticker.
 */

#include <sigiltick/FrameClock.h>
#include <sigiltick/Ticker.h>

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
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class SkCanvas;
class SkImageFilter;
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

// ---------------------------------------------------------------------------
// Animation values

/** How a reconciled property change animates instead of snapping. */
struct Transition {
  std::chrono::milliseconds duration{250};
  choreograph::EaseFn ease = &choreograph::easeOutQuad;
};

template <typename T> struct Transitioned {
  T value;
  Transition spec;
};

/** Wraps a constant so changes to it transition (see API.md semantics:
 *  one motion per (instance, property), retarget-from-current). */
template <typename T> Transitioned<T> with(T value, Transition spec) {
  return {std::move(value), std::move(spec)};
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

struct Corners {
  float radius = 0.0f;
  bool operator==(const Corners &) const = default;
};

/** The one paint-program context: custom leaves (and, in extensions,
 *  decorations and contour walks) all receive this. `elapsedSeconds` is
 *  the Ticker's FrameClock time — pause/time-scale affect it. */
struct PaintContext {
  SkSize size = SkSize::MakeEmpty();
  SkPath outline;
  double elapsedSeconds = 0.0;
  float contentScale = 1.0f;
  bool animating = false;
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

  const sk_sp<SkImageFilter> &imageFilter() const { return m_filter; }

private:
  sk_sp<SkImageFilter> m_filter;
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
        m_paint([s = std::move(scheme)](SkCanvas &c, const PaintContext &ctx) {
          s.paint(c, ctx);
        }) {}
  Decoration(PaintProgram program) // NOLINT: implicit by design
      : m_paint(std::move(program)) {}

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if (m_paint)
      m_paint(canvas, ctx);
  }
  bool animated() const { return m_animated; }

private:
  bool m_animated = false;
  PaintProgram m_paint;
};

// ---------------------------------------------------------------------------
// Layout values (Yoga semantics, 1:1)

struct Dim {
  enum class Unit : uint8_t { Px, Pct, Auto };
  Unit unit = Unit::Auto;
  float value = 0.0f;

  Dim() = default;
  Dim(float px) : unit(Unit::Px), value(px) {} // NOLINT: implicit by design
  bool operator==(const Dim &) const = default;
};
inline Dim pct(float v) {
  Dim d;
  d.unit = Dim::Unit::Pct;
  d.value = v;
  return d;
}

enum class Align : uint8_t { Auto, Start, Center, End, Stretch, Baseline };
enum class Justify : uint8_t {
  Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly
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

/** What a custom layout sees: the container's resolved size and each
 *  child's measured size (text children measured by SigilWeave). */
struct LayoutInput {
  SkSize container = SkSize::MakeEmpty();
  std::vector<SkSize> childSizes;
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
  Element &gap(float px);
  Element &padding(float all);
  Element &padding(float horizontal, float vertical);
  Element &margin(float all);
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

  // ---- shape (defines PaintContext::outline and clipping) ----
  Element &corners(Corners c);
  /** Custom outline: a path generator over the node's laid-out size,
   *  in local coordinates. Overrides corners() as the node's shape —
   *  the fill surface, clip(), and every outline-following decoration
   *  (PathFormat strokes, ContourWalk) trace it. Spiky dialogs,
   *  scalloped frames, any non-rectangular chrome. */
  Element &outline(std::function<SkPath(SkSize)> shape);
  Element &clip(bool on = true);

  // ---- paint ----
  Element &fill(PropValue<Fill> f);
  /** Decoration layers: backgrounds paint below content/children (in
   *  declaration order), foregrounds above. fill() is the transitionable
   *  first background; custom() is a box with one background program. */
  Element &background(Decoration d);
  Element &foreground(Decoration d);
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
  Element &transformOrigin(float fx, float fy);
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

  // ---- identity, caching, transitions ----
  Element &key(std::string_view k);
  Element &cache(Cache c);
  Element &transition(Transition t); // node default for plain constants

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
  const std::shared_ptr<detail::ElementNode> &node() const { return m_node; }
  explicit Element(std::shared_ptr<detail::ElementNode> n)
      : m_node(std::move(n)) {}

private:
  std::shared_ptr<detail::ElementNode> m_node;
};

// ---- factories -----------------------------------------------------------

Element box();
/** Overlap container: children share the box (absolute by default),
 *  painted in (zIndex, declaration order). */
Element stack();
Element text(std::u8string utf8, sigil::weave::TextStyle style);
Element image(std::shared_ptr<const sigil::image::ImageAsset> asset);
/** A box whose content is one paint program (≡ box().background(p)).
 *  Cached like any static subtree — programs that read the clock or
 *  otherwise change per frame must declare .cache(Cache::None). */
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
  Composer(tick::Ticker &ticker, sigil::weave::FontContext &fontContext);
  ~Composer();

  Composer(const Composer &) = delete;
  Composer &operator=(const Composer &) = delete;

  /** Layout viewport in canvas-space px; percent dims resolve here. */
  void setSize(SkSize size);

  /** Feeds PaintContext::elapsedSeconds (one clock everywhere). Null
   *  freezes paint time at 0 — fine for static content and goldens. */
  void setClock(const tick::FrameClock *clock);

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

  // ---- queries (resolved side only) ----
  /** Layout rect of a keyed node, in the composer's coordinate space
   *  (valid after draw()/layout). */
  std::optional<SkRect> bounds(std::string_view key) const;
  /** Live SigilWeave layout of a keyed text node (valid until the next
   *  layout; for glyph choreography and queries). */
  const sigil::weave::ParagraphLayout *
  paragraphLayout(std::string_view key) const;

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
  };
  const Stats &stats() const;

  /** @private */
  struct Impl;

private:
  friend struct detail::Instance;
  std::unique_ptr<Impl> m_impl;
};

} // namespace sigil::compose
