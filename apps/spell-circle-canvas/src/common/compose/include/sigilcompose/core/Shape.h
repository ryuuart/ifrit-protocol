#pragma once

/** @file
 * SigilCompose shape and decoration seams — Shape, the comparable
 * silhouette value, and the ShapeScheme concept behind it; MotionPath and
 * TextPath, a node or a run of type carried along a curve; Decoration, the
 * type-erased mark, with the concepts that read a scheme's declared
 * volatility, bleed, reach and borrows; and LayerStyle, a bundle of
 * decorations applied together.
 */

#include <include/core/SkPath.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Motion.h>
#include <sigilcompose/core/Paint.h>

#include <any>
#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

class SkCanvas;

namespace sigil::compose {

// ---------------------------------------------------------------------------
// The shape seam — a COMPARABLE silhouette value

/** A shape scheme: `SkPath path(SkSize) const`, plus equality.
 *
 *  This is the seam-value convention the library uses throughout — one
 *  named required member and a comparable value. `Shaper` spells
 *  `shape()`, `CrossingRule` spells `decide()`, a shape value spells
 *  `path()`.
 *
 *  Equality is the point, not decoration. A shaped node can only prune —
 *  skip its dirty marking, keep its recording — if the reconciler can
 *  prove the shape is the same one, and a `std::function` cannot be
 *  compared. Every stock generator in `Shapes.h` is a scheme for that
 *  reason. A scheme's equality is a contract on the author: equal values
 *  must generate identical paths at every size. */
template <typename S>
concept ShapeScheme =
    std::equality_comparable<S> && requires(const S& s, SkSize size) {
      { s.path(size) } -> std::convertible_to<SkPath>;
    };

/** THE NODE'S SILHOUETTE, type-erased: what `Element::shape()`, a
 *  `TextPath` baseline and a `band()` spine hold.
 *
 *  Two constructions, one value:
 *
 *  - a COMPARABLE scheme (any `shapes::` generator, or your own value
 *    with `path(SkSize)` + `==`) — the node prunes while the value and
 *    its size are unchanged;
 *  - a raw callable (`[](SkSize) -> SkPath`, an `OutlineFn`) — the escape
 *    hatch. It never compares equal to a separately-constructed Shape, so
 *    the node re-patches on every describe and can never prune. That is a
 *    real per-frame cost on a node that would otherwise be static; reach
 *    for it only when no value form fits. Copies of ONE Shape do compare
 *    equal (they share state), so holding the Shape and re-using it —
 *    rather than re-minting the lambda each describe — restores pruning.
 *
 *  Held as one shared immutable pointer, so a node carrying a shape costs
 *  a pointer and a copy-on-write node copy is a refcount bump. */
class Shape {
 public:
  Shape() = default;

  template <ShapeScheme S>
    requires(!std::same_as<std::remove_cvref_t<S>, Shape>)
  Shape(S scheme) {  // NOLINT: implicit by design (.shape(shapes::star(5)))
    State state;
    state.held = scheme;
    state.equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const S&>(a) == std::any_cast<const S&>(b);
    };
    state.generate = [s = std::move(scheme)](SkSize size) {
      return s.path(size);
    };
    m_state = std::make_shared<const State>(std::move(state));
  }

  /** The escape hatch: any callable over the laid-out size. Never
   *  compares equal to a separately-constructed Shape. */
  template <typename F>
    requires(!ShapeScheme<std::remove_cvref_t<F>> &&
             !std::same_as<std::remove_cvref_t<F>, Shape> &&
             std::is_invocable_r_v<SkPath, const std::remove_cvref_t<F>&,
                                   SkSize>)
  Shape(F fn) {  // NOLINT: implicit by design (.shape([](SkSize s) {...}))
    State state;
    state.generate = std::move(fn);
    m_state = std::make_shared<const State>(std::move(state));
  }

  explicit operator bool() const { return m_state && (bool)m_state->generate; }
  SkPath operator()(SkSize size) const {
    return m_state && m_state->generate ? m_state->generate(size) : SkPath();
  }
  /** Does this value participate in structural equality? (False for the
   *  callable escape hatch.) */
  bool comparable() const { return m_state && (bool)m_state->equals; }

  /** Shared state (copies of one Shape) is equal; comparable schemes of
   *  one type compare their values; anything else is conservative. */
  bool operator==(const Shape& o) const {
    if (m_state == o.m_state) return true;
    if (!m_state || !o.m_state) return false;
    if (!m_state->equals || !o.m_state->equals) return false;
    return m_state->held.type() == o.m_state->held.type() &&
           m_state->equals(m_state->held, o.m_state->held);
  }

 private:
  struct State {
    std::function<SkPath(SkSize)> generate;
    std::any held;
    bool (*equals)(const std::any&, const std::any&) = nullptr;
  };
  std::shared_ptr<const State> m_state;
};

/** A SPATIAL PATH for a node to ride — After Effects' motion model.
 *
 *  `translateX`/`translateY` are two independent lanes. Two lanes can
 *  describe a POINT; they cannot describe a TRAJECTORY, and hand-driving a
 *  curve through them means the author computing two numbers a frame,
 *  which is imperative animation wearing declarative clothes.
 *
 *  The animatable lane here is a single float, @ref t — WHERE ALONG the
 *  curve — so the whole `bind()` chain still applies, to the SCHEDULE
 *  rather than to the geometry:
 *  `.map(&choreograph::easeInOutQuad)` eases the move in and out,
 *  `.target(0, 2)` runs two laps of a closed curve, `.window(...)` makes
 *  the move a slice of a larger phase. The curve supplies the SHAPE, the
 *  lane supplies the SCHEDULE. That separation is the whole design.
 *
 *      .travel({.path = shapes::circle(),
 *               .t = bind(&phase).map(&choreograph::easeInOutQuad).target(0,
 * 1)})
 *
 *  The rules:
 *
 *  - **The curve is resolved against the PARENT's box, not the node's.**
 *    A `Shape` is a function of a size, and the size that makes a motion
 *    path mean anything is the FRAME the node moves in — `shapes::circle()`
 *    on a 40 px dot inside a 400 px card is a 400 px orbit, not a 40 px
 *    twitch. (A root node with no parent resolves against its own box,
 *    which is the canvas.)
 *  - **The transform ORIGIN is what rides the curve.** AE moves the
 *    layer's anchor point; `transformOrigin()` is Compose's anchor point,
 *    and it is already the pivot rotate/scale/skew turn about, so the
 *    point on the curve is fixed under all of them. The default (0.5,
 *    0.5) rides the node's centre.
 *  - **PRECEDENCE: whatever the path drives, it drives outright.** It
 *    always drives position, so `translateX`/`translateY` are IGNORED
 *    while a path is engaged — not blended, not treated as an offset (a
 *    lane that half-contradicts a curve can only place the node off it).
 *    Dropping the path hands the very same lanes back, live.
 *  - **Auto-orient ADDS to `rotate()`, it does not replace it.** The
 *    tangent angle sets the base orientation and the authored rotation
 *    composes on top, as it does in AE — so `rotate(&spin)` on a
 *    travelling node spins it AS it banks.
 *  - **WRAP on a closed curve, CLAMP on an open one.** On a closed
 *    outline 0 and 1 are the same point, so `t` past 1 comes round (and
 *    negative `t` runs backwards) — that is what makes `.target(0, 2)`
 *    read as two laps with no extra API. An open curve parks at its ends.
 *    A path is closed when EVERY contour it resolved to is closed.
 *  - **ARC LENGTH is the only parameterisation.** `t` is a fraction of
 *    the path's TOTAL arc length across every contour — the same
 *    coordinate `bandPointAt`, `spans::` and `SkTrimPathEffect` speak, so
 *    a motion path and a span reveal driven by the same numbers describe
 *    the same run. There is no flag to switch it off because there is no
 *    alternative: an `SkPath` has no native parameter, only length.
 *  - **A path that resolves to no length is not engaged.**
 *
 *  Paint-only, like the lanes it outranks: a travelling node never
 *  relayouts, and its content picture replays under the new transform.
 *
 *  PRUNING follows the shape seam it is built from: a comparable scheme
 *  (any `shapes::` generator) prunes, and the raw-callable escape hatch
 *  never compares equal, so the node re-patches every describe — the same
 *  contract and the same cost `Element::shape()` documents. */
struct MotionPath {
  /** The curve, resolved against the PARENT's laid-out box. */
  Shape path;
  /** WHERE along it, as a fraction of total arc length. One float, so
   *  every `bind()`/`animate()` verb still applies. */
  Animatable<float> t = 0.0f;
  /** Auto-orient: how far ahead the node looks, in the same units as
   *  @ref t. Non-zero adds `atan2` of the chord `position(t + lookAhead)
   *  - position(t)` to `rotate()`, so a negative value faces BACK down
   *  the curve. Exactly 0 (the default, matching AE's unchecked box)
   *  leaves orientation alone. At the end of an OPEN curve, where the
   *  forward chord collapses, the last good chord is held rather than
   *  yielding a NaN angle. */
  float lookAhead = 0.0f;
};

/** Text whose BASELINE is a path (`Element::onPath`).
 *
 *  The run is shaped once — real kerning, real ligatures, real advances —
 *  and then every glyph is placed by arc length along the resolved path
 *  and rotated to its tangent, through the same batched RSXform draw
 *  kinetic text uses (one draw per font+colour, never one per glyph).
 *
 *  The alternative, placing curved lettering by hand, costs one Element
 *  and one layout PER GLYPH and loses kerning, because each glyph is laid
 *  out alone. Ring labels, dial faces, seals, compass roses, mottoes and
 *  map lettering all want this instead. */
struct TextPath {
  /** The baseline, resolved against the node's laid-out box — any
   *  `shapes::` generator, or your own. EVERY contour is walked, in order,
   *  as one continuous arc-length coordinate, so a trajectory that the
   *  frame cut into several contours still carries its whole run.
   *
   *  "The node's box" means the TEXT NODE'S OWN box, not a parent's. The
   *  tempting `disc(c, R).child(text(...).onPath(...))` resolves the ring
   *  against the text's intrinsic size and silently collapses every label
   *  into a blob. Give the TEXT node the disc's width and height instead
   *  — the text leaf is the disc. */
  Shape path;
  /** WHERE ALONG the path the run sits, as a fraction of its length. With
   *  Align::Center this is the run's midpoint.
   *
   *  One float, so every `bind()`/`animate()` verb applies — and on a
   *  CLOSED baseline the fraction WRAPS, which is the infinite marquee: a
   *  phase output running 0→1 forever walks the whole run round the loop
   *  and back to where it started, with no seam and no relayout. On an
   *  open one the run simply slides, and glyphs pushed off either end are
   *  dropped rather than piled on the last point.
   *
   *  Moving it is PAINT-ONLY. The run is shaped and broken across the
   *  path's contours once; the phase re-places the glyphs it already
   *  placed, so a marquee costs a repaint and never a reflow. It is
   *  content volatility all the same — the glyphs move inside the node's
   *  own box — so the node's recording is refused while the phase runs and
   *  taken again once it provably holds still. */
  Animatable<float> at = 0.0f;
  enum class Align { Start, Center, End };
  Align align = Align::Start;
  /** Perpendicular offset in px, positive to the LEFT of travel — which on
   *  a clockwise circle is outward. The path is the baseline, so this is
   *  how far off it the type rides. */
  float offset = 0.0f;
  /** Flip glyphs that would come out upside down, so lettering on the
   *  lower half of a ring reads right way up.
   *
   *  Default OFF, which is the engraver's convention: glyph-up points
   *  radially outward everywhere, so the bottom of a ring genuinely reads
   *  upside down. Modern signage flips; historical plates do not. */
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
   *  Element, which is the same per-element cost onPath exists to avoid.
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
  /** Turn every glyph to its EXACT tangent instead of snapping the angle.
   *
   *  Snapping is the default because each distinct rotation is a distinct
   *  glyph-atlas strike: a curve whose glyphs turn continuously would
   *  re-rasterize every letter on every frame. The steps are far under a
   *  pixel of lean at label sizes on a ring whose letters sit further apart
   *  than that. Set it for STATIC artwork set large, where the steps show
   *  and nothing is paying per frame. */
  bool exactTangent = false;
};

/** Anything with paint(canvas, PaintContext) — decorations, effect
 *  bodies. An optional `bool isAnimated() const` declares per-frame
 *  volatility; see AnimatedDecoration below. */
template <typename D>
concept DecorationScheme =
    requires(const D& d, SkCanvas& canvas, const PaintContext& ctx) {
      { d.paint(canvas, ctx) };
    };

/** THE VOLATILITY DECLARATION, and the author obligation behind every
 *  automatic cache in this library.
 *
 *  A scheme that repaints differently from one frame to the next — a
 *  bound dash phase, a live material, a walk keyed to elapsed time — must
 *  say so with `bool isAnimated() const`. The library then stops caching
 *  its node's picture. Say nothing and the node is treated as static: its
 *  first frame is recorded and replayed forever, and the mark freezes with
 *  no error and no warning.
 *
 *  Nothing introspects on your behalf. By the time the composer holds it a
 *  Decoration is a type-erased value with one paint() entry point; there
 *  is no way to look inside a lambda and see that it read the clock. The
 *  value declares, or the value freezes.
 *
 *  `isAnimated()` is the one word for this question across the whole
 *  library — Material, Effect and Decoration all spell it the same way,
 *  and it is always a query derived from how the value was constructed,
 *  never a setter. */
template <typename D>
concept AnimatedDecoration = requires(const D& d) {
  { d.isAnimated() } -> std::convertible_to<bool>;
};

/** Optional on a DecorationScheme: how far it paints BEYOND the node's
 *  bounds (soft shadows, glows). The recording cull grows by the node's
 *  max bleed so cached pictures never truncate overflowing chrome. */
template <typename D>
concept BleedingDecoration = requires(const D& d) {
  { d.bleed() } -> std::convertible_to<float>;
};

/** Optional on a DecorationScheme: the FULL width of the MARK it paints,
 *  across the outline it dresses.
 *
 *  A DIFFERENT NUMBER FROM `bleed()`, and confusing them truncates
 *  pictures silently. `bleed()` is the CULL's number — how far paint
 *  escapes the node's box. This is how wide the mark is. An
 *  `Align::Inner` stroke bleeds ZERO, because it never leaves the shape,
 *  while painting a mark `width` px across; anything sized from its bleed
 *  would be far too small. Consumers that need to know where a mark IS,
 *  rather than how far it escapes, ask this one. Over-reporting either
 *  number is safe; under-reporting either one silently clips cached
 *  output. */
template <typename D>
concept ReachingDecoration = requires(const D& d) {
  { d.reach() } -> std::convertible_to<float>;
};

/** Optional on a DecorationScheme: element keys whose resolved PATHS this
 *  decoration needs (a weave's `strand::from(key)`). The element collects
 *  them at build time and the derive pass answers them into
 *  `PaintContext::borrowed`, on the same flat walk that resolves
 *  flowAround and connector/rail borrows.
 *
 *  Declared rather than introspected, for the same reason isAnimated() is:
 *  the element cannot look inside a type-erased value. A composite
 *  decoration must forward its children's keys, or their borrows resolve
 *  to nothing and they draw nothing. */
template <typename D>
concept BorrowingDecoration = requires(const D& d) {
  { d.borrows() } -> std::convertible_to<std::vector<std::string>>;
};

/** Type-erased decoration: the kernel seam the extension primitives
 *  (PathFormat, Slice, ContourWalk — see Decorations.h) plug into. A bare
 *  PaintProgram works too, at the cost of comparability — see
 *  operator== below. */
class Decoration {
 public:
  template <DecorationScheme D>
  Decoration(D scheme)  // NOLINT: implicit by design
      : m_animated([&] {
          if constexpr (AnimatedDecoration<D>)
            return scheme.isAnimated();
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
            return (float)scheme.bleed();  // the best available answer
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
      m_scheme = scheme;  // retained copy, compared structurally
      m_equals = [](const std::any& a, const std::any& b) {
        return std::any_cast<const D&>(a) == std::any_cast<const D&>(b);
      };
    }
    m_paint = [s = std::move(scheme)](SkCanvas& c, const PaintContext& ctx) {
      s.paint(c, ctx);
    };
  }
  Decoration(PaintProgram program)  // NOLINT: implicit by design
      : m_paint(std::move(program)) {}

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    if (m_paint) m_paint(canvas, ctx);
  }
  /** Declared volatility, read off whichever word the scheme spelled. */
  bool isAnimated() const { return m_animated; }
  float bleed() const { return m_bleed; }
  /** FULL width of the mark this decoration paints, across the outline it
   *  dresses (see ReachingDecoration). Falls back to bleed(), then to 0. */
  float reach() const { return m_reach; }
  /** Keyed elements whose resolved paths this decoration reads (see
   *  BorrowingDecoration). Empty for everything that borrows nothing. */
  const std::vector<std::string>& borrows() const { return m_borrows; }

  /** Structural equality, which is what lets a decorated node prune with
   *  no memo around it: true only when both wrap the same value-comparable
   *  scheme type and those values compare equal.
   *
   *  A bare PaintProgram, or a scheme without operator==, ALWAYS compares
   *  unequal. That is conservative and correct — the library cannot prove
   *  a callable is the same drawing — but it has a cost: such a node is
   *  re-patched and re-recorded on every describe, forever. Prefer a value
   *  scheme for static chrome, or wrap the node in memo(). */
  bool operator==(const Decoration& o) const {
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
  std::function<bool(const std::any&, const std::any&)> m_equals;
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

}  // namespace sigil::compose
