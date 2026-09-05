#pragma once

/** @file
 * SigilCompose shape and decoration seams — Shape, the comparable
 * silhouette value, and the ShapeScheme concept behind it; MotionPath, a
 * node carried along a curve; Decoration, the type-erased mark, with the
 * concepts that read a scheme's declared volatility, bleed, reach and
 * borrows; and LayerStyle, a bundle of decorations applied together. A
 * run of type carried along a curve is `TextPath`, in
 * <sigilcompose/typography/TextPath.h>.
 */

#include <include/core/SkPath.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/values/Animated.h>

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
  /** A Shape is itself a scheme, so it NESTS: a wrapper that asks for a
   *  scheme takes one, and the equality it then uses is the one below —
   *  which refuses a callable, where a compiler-written equality over an
   *  empty closure type would vacuously accept it. */
  SkPath path(SkSize size) const { return (*this)(size); }
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

/** A PATH COOKED ONCE, held as a comparable Shape.
 *
 *  The commonest escape hatch in the tree is `.shape([p](SkSize) { return
 *  p; })` — a path already built in the author's own coordinates, handed
 *  to the node through a lambda. That lambda compares equal to nothing,
 *  so the node re-patches and re-records on every describe however static
 *  the drawing is. This is the same handover as a value: the path is the
 *  identity.
 *
 *  Equality is the path's own generation, which a COPY carries and a
 *  rebuild does not. So `heldPath(m_ring)` re-minted every describe off a
 *  path the caller holds prunes, and a path rebuilt from its parts each
 *  describe does not — cook it once, hold it, hand it here.
 *
 *  The path is used AS IT WAS COOKED, in the node's local coordinates. It
 *  is not fitted to the box (`shapes::svg()` is the fitting one), so a
 *  node whose box is not the path's own bounds shows the path where the
 *  path is. `pathFigure()` is the factory that gives a node exactly those
 *  bounds. */
class HeldPath {
 public:
  HeldPath() = default;
  explicit HeldPath(SkPath cooked) : m_cooked(std::move(cooked)) {}
  SkPath path(SkSize) const { return m_cooked; }
  const SkPath& cooked() const { return m_cooked; }
  /** The generation id changes on every edit and rides every copy, so
   *  this is exact for a held path and conservative for a rebuilt one.
   *  Fill type joins it because the id does not answer for it. */
  bool operator==(const HeldPath& o) const {
    return m_cooked.getGenerationID() == o.m_cooked.getGenerationID() &&
           m_cooked.getFillType() == o.m_cooked.getFillType();
  }

 private:
  SkPath m_cooked;
};

inline HeldPath heldPath(SkPath cooked) { return HeldPath(std::move(cooked)); }

/** A CALLABLE MADE COMPARABLE BY THE VALUE IT CLOSES OVER.
 *
 *  A generator that is genuinely a function of a few numbers — a radius,
 *  a corner mask, a pair of angles — is a value wearing a lambda's
 *  clothes, and the numbers are what tells one from another. Hand them
 *  over as @p key and the node settles: equal keys mean equal drawings,
 *  which is the same author contract `shapes::parametric(key, …)` and
 *  `custom(key, …)` take.
 *
 *      .shape(keyedShape(std::tuple(radius, cut, mask),
 *                        [=](SkSize s) { return panel(radius, cut, mask)(s); }))
 *
 *  ONE KEY MUST NAME ONE DRAWING. Anything the callable reads that is not
 *  in the key is invisible to the prune, and a node that prunes replays
 *  the picture it recorded — so a number left out of the key freezes at
 *  whatever it was on the frame that recorded. Fold everything the body
 *  reads into the key, or use the keyless form and pay per describe.
 *
 *  The key's type is part of the identity, as it is for every scheme: two
 *  keyed shapes compare only when both the key type and the callable's
 *  own type match, which is what makes a `std::tuple` of the closed-over
 *  numbers the natural spelling. */
template <std::equality_comparable K, typename F>
  requires std::is_invocable_r_v<SkPath, const F&, SkSize>
class KeyedShape {
 public:
  KeyedShape(K key, F fn) : m_key(std::move(key)), m_fn(std::move(fn)) {}
  SkPath path(SkSize size) const { return m_fn(size); }
  const K& key() const { return m_key; }
  bool operator==(const KeyedShape& o) const { return m_key == o.m_key; }

 private:
  K m_key;
  F m_fn;
};

template <std::equality_comparable K, typename F>
  requires std::is_invocable_r_v<SkPath, const F&, SkSize>
KeyedShape<K, F> keyedShape(K key, F fn) {
  return KeyedShape<K, F>(std::move(key), std::move(fn));
}

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
  motion::Animatable<float> t = 0.0f;
  /** Auto-orient: how far ahead the node looks, in the same units as
   *  @ref t. Non-zero adds `atan2` of the chord `position(t + lookAhead)
   *  - position(t)` to `rotate()`, so a negative value faces BACK down
   *  the curve. Exactly 0 (the default, matching AE's unchecked box)
   *  leaves orientation alone. At the end of an OPEN curve, where the
   *  forward chord collapses, the last good chord is held rather than
   *  yielding a NaN angle. */
  float lookAhead = 0.0f;
};

/** WHAT A NODE'S DECORATIONS DRESS.
 *
 *  Every decoration — a bevel, an inner shadow, a glow, a gloss, a
 *  keyline, a whole layer style — is drawn ACROSS AN OUTLINE, and this
 *  says which outline it is handed. The three answers are three different
 *  MECHANISMS, not three shapes:
 *
 *  `Outline` is THE NODE'S SHAPE — its rounded box, its `shape()`
 *  generator, a routed connector's path, a band's swept region. On a text
 *  leaf that shape is a rectangle, which is why a chrome style on a word
 *  bevels a slab behind it rather than the word.
 *
 *  `Glyphs` is THE PLACEMENT'S CONTOURS: the union of a text leaf's glyph
 *  outlines exactly where its layout put them. Every decoration already
 *  written then works on letters with no new preset, because a decoration
 *  was never about a box — it was about whatever outline it was handed.
 *  On a node that is not text it means the node's shape.
 *
 *  `Coverage` is WHAT THE NODE ACTUALLY DREW: its rendered layer's alpha,
 *  traced back into a path. Neither of the other two looks at a pixel, so
 *  neither can answer for an image with an alpha cut-out, a clipped or
 *  masked subtree, or anything else whose visible silhouette is not its
 *  shape and not a glyph run. This one can, and it is the only one that
 *  can. Three things follow from tracing a raster, and all three are
 *  visible in the result:
 *
 *  - THE BOUNDARY IS A STAIRCASE. It is built from whole pixels of a
 *    bounded raster, so its edges are axis-aligned steps, and a
 *    decoration that dresses it dresses that staircase. A keyline around
 *    a cut-out reads as a keyline around a stepped cut-out.
 *  - THE STEP SIZE IS THE NODE'S OWN. The trace rasterises the node's box
 *    at a fixed number of pixels on its longer side however large the box
 *    is, so the cost of a coverage boundary does not grow with the node,
 *    and the steps of a big node are bigger than the steps of a small one.
 *  - PAINT BELOW HALF COVERAGE IS NOT A SILHOUETTE. A pixel joins the
 *    boundary when the node's paint covers at least half of it, so a 30%
 *    wash over the whole box traces to nothing at all and its decorations
 *    have nothing to dress.
 *
 *  The node's OWN decorations are not in the trace — they are what dresses
 *  it, and a mark that dressed itself would have no fixed point. Its fill,
 *  its content, its children and their marks are.
 *
 *  `Auto` is what a node that says nothing gets, and it means the node's
 *  own shape. A text leaf does NOT default to its glyphs and nothing
 *  defaults to its coverage: a caption with a drop shadow means the
 *  caption's box, and changing that under every existing passage would
 *  repaint pages nobody asked to repaint. */
enum class Boundary : uint8_t { Auto, Outline, Glyphs, Coverage };

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
 *  style" as a value. Presets (kit::aquaGel(), kit::y2kChrome())
 *  return one; Element::style() splices it in: `under` layers paint below
 *  the fill/content (drop shadows, body ramps), `over` layers above
 *  (gloss lenses, bevels, keylines). One call dresses the node. */
struct LayerStyle {
  std::vector<Decoration> under;
  std::vector<Decoration> over;
};

}  // namespace sigil::compose
