#pragma once

/** @file
 * IfritCompose kernel — data-driven, cacheable, animated drawable
 * components for any SkCanvas. See DESIGN.md (architecture), API.md
 * (surface rationale), STRESS_TESTS.md (acceptance catalog).
 *
 * The kernel is: Element descriptions built by fluent value builders,
 * component functions over plain data (+ memo), and a Composer that
 * reconciles by key, lays out via Yoga (TextFlow-measured text leaves),
 * paints with explicit stacking, caches provably-static subtrees as
 * SkPictures automatically, and animates through Choreograph driven by
 * an ifrit::tick::Ticker.
 */

#include <ifrittick/FrameClock.h>
#include <ifrittick/Ticker.h>

#include <textflow/ParagraphLayout.h>
#include <textflow/Style.h>

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

namespace ifrit::image {
class ImageAsset;
}

namespace textflow {
class FontContext;
}

namespace ifrit::compose {

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
 *  subtrees; None opts a node out (per-frame paint programs that read
 *  the clock MUST declare this — declared volatility). */
enum class Cache : uint8_t { Auto, Picture, None };

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
  Element &clip(bool on = true);

  // ---- paint ----
  Element &fill(PropValue<Fill> f);
  Element &opacity(PropValue<float> o);
  Element &blend(SkBlendMode mode);
  Element &translateX(PropValue<float> v);
  Element &translateY(PropValue<float> v);
  Element &rotate(PropValue<float> degrees);
  Element &scale(PropValue<float> factor);
  Element &transformOrigin(float fx, float fy);
  Element &zIndex(int z);

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
Element text(std::u8string utf8, textflow::TextStyle style);
Element image(std::shared_ptr<const ifrit::image::ImageAsset> asset);
/** A box whose content is one paint program (≡ box().background(p)).
 *  Cached like any static subtree — programs that read the clock or
 *  otherwise change per frame must declare .cache(Cache::None). */
Element custom(PaintProgram program);

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
  Composer(tick::Ticker &ticker, textflow::FontContext &fontContext);
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
  /** Live TextFlow layout of a keyed text node (valid until the next
   *  layout; for glyph choreography and queries). */
  const textflow::ParagraphLayout *
  paragraphLayout(std::string_view key) const;

  // ---- introspection (perf verification; see compose_bench) ----
  struct Stats {
    size_t instances = 0;       ///< live retained nodes
    size_t describedNodes = 0;  ///< element nodes visited last render()
    size_t memoHits = 0;        ///< memo props equal → describe skipped
    size_t patchedNodes = 0;    ///< instances whose props changed
    size_t picturesLive = 0;    ///< auto-cached subtree pictures held
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

} // namespace ifrit::compose
