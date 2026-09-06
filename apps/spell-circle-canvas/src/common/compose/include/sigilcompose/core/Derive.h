#pragma once

/** @file
 * SigilCompose derive family — content that asks where a keyed node landed:
 * connector and rail, with Router and RailRouter as their pluggable seam;
 * Anchor, a normalized point on a keyed node's bounds; band, a shape swept
 * out by an authored or borrowed spine; bandPointAt, the one statement of
 * the band's across sign; and the `derive::` namespace that gathers the
 * family under one name.
 *
 * The derive phase itself — resolving text exclusions and routes over the
 * flat edge lists once layout has produced geometry — is a pass in the
 * composer's layout phase list, inside the converging group, so a routed
 * plate settles against the geometry the other converging passes move.
 * That registration is the whole of how the family reaches the kernel's
 * schedule; nothing else in the kernel knows a route from a box.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/Stroke.h>

#include <any>
#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sigil::compose {

/** A relationship as a first-class element: a path routed between two
 *  keyed nodes' resolved bounds, stroked by the connector's foreground
 *  decorations (attach a PathFormat — the routed path arrives as
 *  PaintContext::outline). Straight line by default; supply a router
 *  for anything else. Position it absolute().inset(0) over the nodes
 *  it connects.
 *
 *  `gap` is `Anchor::gap` under another name: it pulls each END of the
 *  routed path back along itself by that many px, clamped so short routes
 *  keep a visible run. Routes run centre to centre, and a node's box can
 *  be much larger than its visible shape — under `sdf::` chrome, for
 *  instance — so the gap is how a wire stops at the glow instead of
 *  piercing it. */
/** A route scheme: `SkPath route(const SkRect& from, const SkRect& to)
 *  const`, plus equality — the same seam-value convention a `Shape`
 *  takes, and for the same reason. A routed node can only prune if the
 *  reconciler can prove the route is the same one, so a router is a
 *  VALUE whose equality is its parameters (the rails it bends at, the
 *  radii, the phases) and never a callable's identity. A scheme's
 *  equality is a contract on the author: equal values must route
 *  identical paths between every pair of rects. */
template <typename R>
concept RouteScheme =
    std::equality_comparable<R> &&
    requires(const R& r, const SkRect& from, const SkRect& to) {
      { r.route(from, to) } -> std::convertible_to<SkPath>;
    };

/** THE ROUTE BETWEEN TWO RECTS, type-erased: what `connector()` holds.
 *
 *  Two constructions, one value:
 *
 *  - a COMPARABLE scheme (any `routers::` value, or your own value with
 *    `route(from, to)` + `==`) — the node prunes while the value and the
 *    rects are unchanged;
 *  - a raw callable (`[](const SkRect&, const SkRect&) -> SkPath`) — the
 *    escape hatch. It never compares equal to a separately-constructed
 *    Router, so the node re-patches on every describe and can never
 *    prune. Copies of ONE Router do compare equal (they share state), so
 *    holding the Router and re-using it — rather than re-minting the
 *    lambda each describe — restores pruning.
 *
 *  Held as one shared immutable pointer, so a node carrying a router
 *  costs a pointer and a copy-on-write node copy is a refcount bump. */
class Router {
 public:
  Router() = default;

  template <RouteScheme R>
    requires(!std::same_as<std::remove_cvref_t<R>, Router>)
  Router(R scheme) {  // NOLINT: implicit by design (connector(a, b, arc()))
    State state;
    state.held = scheme;
    state.equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const R&>(a) == std::any_cast<const R&>(b);
    };
    state.generate = [s = std::move(scheme)](const SkRect& from,
                                             const SkRect& to) {
      return s.route(from, to);
    };
    m_state = std::make_shared<const State>(std::move(state));
  }

  /** The escape hatch: any callable over the two endpoint rects. Never
   *  compares equal to a separately-constructed Router. */
  template <typename F>
    requires(!RouteScheme<std::remove_cvref_t<F>> &&
             !std::same_as<std::remove_cvref_t<F>, Router> &&
             std::is_invocable_r_v<SkPath, const std::remove_cvref_t<F>&,
                                   const SkRect&, const SkRect&>)
  Router(F fn) {  // NOLINT: implicit by design (connector(a, b, [](…){…}))
    State state;
    state.generate = std::move(fn);
    m_state = std::make_shared<const State>(std::move(state));
  }

  explicit operator bool() const { return m_state && (bool)m_state->generate; }
  SkPath operator()(const SkRect& from, const SkRect& to) const {
    return m_state && m_state->generate ? m_state->generate(from, to)
                                        : SkPath();
  }
  /** A Router is itself a scheme, so it NESTS: a wrapper that asks for a
   *  scheme takes one, and the equality it then uses is the one below —
   *  which refuses a callable, where a compiler-written equality over an
   *  empty closure type would vacuously accept it. */
  SkPath route(const SkRect& from, const SkRect& to) const {
    return (*this)(from, to);
  }
  /** Does this value participate in structural equality? (False for the
   *  callable escape hatch.) */
  bool comparable() const { return m_state && (bool)m_state->equals; }

  /** Shared state (copies of one Router) is equal; comparable schemes of
   *  one type compare their values; anything else is conservative. */
  bool operator==(const Router& o) const {
    if (m_state == o.m_state) return true;
    if (!m_state || !o.m_state) return false;
    if (!m_state->equals || !o.m_state->equals) return false;
    return m_state->held.type() == o.m_state->held.type() &&
           m_state->equals(m_state->held, o.m_state->held);
  }

 private:
  struct State {
    std::function<SkPath(const SkRect&, const SkRect&)> generate;
    std::any held;
    bool (*equals)(const std::any&, const std::any&) = nullptr;
  };
  std::shared_ptr<const State> m_state;
};

Element connector(std::string_view fromKey, std::string_view toKey,
                  Router router = {}, float gap = 0.0f);

/** A rail endpoint/waypoint: a NORMALIZED point on a keyed node's resolved
 *  bounds ((0,0)=top-left, (1,1)=bottom-right — the binding form tldraw and
 *  Excalidraw both converged on; never absolute coordinates, so rails
 *  survive layout, drag, and reflow). `gap` pulls a TERMINAL anchor back
 *  along its segment (breathing room at the ends; ignored on waypoints).
 *
 *  A FREE POINT is the other half: leave `nodeKey` empty and the anchor is
 *  `point`, in the RAIL'S OWN coordinates, bound to nothing. A route
 *  through a place rather than through a thing — the bend that clears a
 *  corner, the fan-out a diagram's own drawing puts at a fixed offset —
 *  is a real waypoint and not a node, and standing invisible boxes up to
 *  carry those coordinates mounts, lays out and reconciles a node per
 *  bend for a number the caller already had.
 *
 *  A rail whose anchors are ALL free points is a polyline the router
 *  draws and nothing binds; one that mixes them is the ordinary case —
 *  a wire that leaves a port, turns in the gutter, and arrives at
 *  another. */
struct Anchor {
  std::string nodeKey;
  SkPoint norm = {0.5f, 0.5f};
  float gap = 0.0f;
  /** Read only when `nodeKey` is empty: the point, in the rail's own
   *  coordinates (the rail is normally `absolute().inset(0)` over the
   *  nodes it threads, so those are the coordinates the nodes are placed
   *  in). Last, so the positional `{key, norm, gap}` spelling stands. */
  SkPoint point = {0.0f, 0.0f};
  bool operator==(const Anchor&) const = default;
};

/** WHERE A BOX HANGS OFF ANOTHER ONE — the positioning value, stated as a
 *  pair of normalized points and a list of places to try.
 *
 *  `on` is the point of the ANCHOR's resolved rect the box hangs from;
 *  `at` is the point of the BOX that lands there; `offset` is how far
 *  from there, in px, in the composition's axes. The pair covers every
 *  arrangement of two boxes there is — `{on = {0.5, 0}, at = {0.5, 1}}`
 *  is "centred above", `{on = {1, 0.5}, at = {0, 0.5}}` is "to the right,
 *  middles level" — and it is normalized rather than absolute for the
 *  same reason an Anchor is: it survives layout, drag and reflow, where
 *  coordinates lifted off one frame do not.
 *
 *  `fallbacks` is what makes it a position rather than an offset. The
 *  stated tether is tried first; if the box it places leaves `within`,
 *  each fallback is tried in the order given, and the first that FITS is
 *  taken. When none fits the stated one stands, so a box that cannot be
 *  placed anywhere is still placed where it was asked for. A fallback's
 *  own `fallbacks` are not read — the list is the list.
 *
 *  `within` empty is the composer's own bounds, which is what "on screen"
 *  means when nothing narrower is stated.
 *
 *  AN UNKNOWN KEY IS SILENT, the family's rule: a tether naming a node
 *  that is not there places nothing and the box stays where layout left
 *  it. So is a key naming this node or one of its descendants, which
 *  would derive the box from itself. */
struct Tether {
  std::string key;
  SkPoint on = {0.5f, 0.5f};
  SkPoint at = {0.5f, 0.5f};
  SkVector offset = {0.0f, 0.0f};
  SkRect within = SkRect::MakeEmpty();
  std::vector<Tether> fallbacks;
  bool operator==(const Tether&) const = default;

  /** Where a box of @p size lands when this tether ties it to @p anchor.
   *  Both rects are in ONE space and the answer is in that space; which
   *  space that is belongs to the caller. */
  SkRect place(const SkRect& anchor, SkSize size) const {
    return SkRect::MakeXYWH(
        anchor.left() + anchor.width() * on.x() + offset.x() -
            size.width() * at.x(),
        anchor.top() + anchor.height() * on.y() + offset.y() -
            size.height() * at.y(),
        size.width(), size.height());
  }
};

/** A rail-route scheme: `SkPath route(std::span<const SkPoint>) const`,
 *  plus equality — the pointwise seam's half of the same convention
 *  `RouteScheme` states. Equal values must route identical paths through
 *  every anchor run. */
template <typename R>
concept RailScheme =
    std::equality_comparable<R> &&
    requires(const R& r, std::span<const SkPoint> anchors) {
      { r.route(anchors) } -> std::convertible_to<SkPath>;
    };

/** THE PATH THROUGH AN ORDERED RUN OF ANCHORS, type-erased: what
 *  `rail()` holds. Stock values in <sigilcompose/kit/Routers.h>
 *  (polyline, octilinear, orbit, manhattan); write your own for anything
 *  else, and a straight polyline is what an empty one draws.
 *
 *  Comparable exactly as `Router` is: a `routers::` value or your own
 *  value with `route(anchors)` + `==` prunes; a raw callable
 *  (`[](std::span<const SkPoint>) -> SkPath`) is the escape hatch that
 *  compares equal to nothing but its own copies. */
class RailRouter {
 public:
  RailRouter() = default;

  template <RailScheme R>
    requires(!std::same_as<std::remove_cvref_t<R>, RailRouter>)
  RailRouter(R scheme) {  // NOLINT: implicit by design (rail(a, polyline()))
    State state;
    state.held = scheme;
    state.equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const R&>(a) == std::any_cast<const R&>(b);
    };
    state.generate = [s = std::move(scheme)](std::span<const SkPoint> anchors) {
      return s.route(anchors);
    };
    m_state = std::make_shared<const State>(std::move(state));
  }

  /** The escape hatch: any callable over the resolved anchor run. Never
   *  compares equal to a separately-constructed RailRouter. */
  template <typename F>
    requires(!RailScheme<std::remove_cvref_t<F>> &&
             !std::same_as<std::remove_cvref_t<F>, RailRouter> &&
             std::is_invocable_r_v<SkPath, const std::remove_cvref_t<F>&,
                                   std::span<const SkPoint>>)
  RailRouter(F fn) {  // NOLINT: implicit by design (rail(a, [](auto p){…}))
    State state;
    state.generate = std::move(fn);
    m_state = std::make_shared<const State>(std::move(state));
  }

  explicit operator bool() const { return m_state && (bool)m_state->generate; }
  SkPath operator()(std::span<const SkPoint> anchors) const {
    return m_state && m_state->generate ? m_state->generate(anchors) : SkPath();
  }
  /** A RailRouter is itself a scheme, so it NESTS — same reason a Router
   *  and a Shape do. */
  SkPath route(std::span<const SkPoint> anchors) const {
    return (*this)(anchors);
  }
  /** Does this value participate in structural equality? (False for the
   *  callable escape hatch.) */
  bool comparable() const { return m_state && (bool)m_state->equals; }

  /** Shared state (copies of one RailRouter) is equal; comparable schemes
   *  of one type compare their values; anything else is conservative. */
  bool operator==(const RailRouter& o) const {
    if (m_state == o.m_state) return true;
    if (!m_state || !o.m_state) return false;
    if (!m_state->equals || !o.m_state->equals) return false;
    return m_state->held.type() == o.m_state->held.type() &&
           m_state->equals(m_state->held, o.m_state->held);
  }

 private:
  struct State {
    std::function<SkPath(std::span<const SkPoint>)> generate;
    std::any held;
    bool (*equals)(const std::any&, const std::any&) = nullptr;
  };
  std::shared_ptr<const State> m_state;
};

/** The component that IS a line: a path threaded through an ordered span of
 *  anchors (a transit line through its stations, a wire through ports),
 *  resolved in the derive phase and re-routed whenever an anchored node
 *  moves. The routed path becomes PaintContext::outline, so PathFormat
 *  strokes, ContourWalk stamps and span masks all dress it — a rail with
 *  `.mask(by::spans(spans::upTo(with(1.0f, {800ms}))))` DRAWS ITSELF.
 *  Position it absolute().inset(0) over the nodes it threads, as with
 *  connector(). */
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
 *  IT DOES NOT HIT-TEST AS ITS SHAPE. Hit testing consults the node's own
 *  shape value, and a band's silhouette is derived rather than set there,
 *  so a band hits as its LAYOUT BOX — a wider region than the mark you can
 *  see.
 *
 *  An authored spine is a `Shape`, exactly like shape()'s value: a
 *  comparable generator (any `shapes::` value) prunes; a raw callable is
 *  the escape hatch that never compares equal — memo() such a node, or
 *  hold the Shape value stable, to prune it. A borrowed spine
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
Element band(Shape spine, Across width);
Element band(Around spine, Across width);

/** The band's own (along, across) space, addressable: `along` is a
 *  fraction of the spine's total arc length, `across` is px on the normal.
 *
 *  **Positive `across` is to the LEFT of travel**, which in screen space
 *  (y down) is OUTSIDE a clockwise path — SkPath's own direction for rects
 *  and circles, so `.outward()` exits the shape.
 *
 *  THIS IS THE ONE STATEMENT OF THAT CONVENTION for the whole library.
 *  `Profile::across`, `strand::offset`, `geometry::parallel`,
 *  `lines::Rail::across`, `geometry::shapes::offset` and
 *  `TextPath::offset` all mean this same side. Anything placing content on
 *  a band reads it here, so the placement and the band's own geometry
 *  cannot disagree. */
SkPoint bandPointAt(const SkPath& spine, float along, float acrossPx);

// ---------------------------------------------------------------------------
// The DERIVE family, gathered under one word

/** Everything that asks "where did that keyed node land, and give me more
 *  content because of it" — the DERIVE phase.
 *
 *  Its members are `flowAround`, `connector` and `rail` (with `routers::`
 *  as their pluggable seam), `band(around(key))`, `spans::fit(key)` and a
 *  decoration's `strand::from(key)`. Six spellings, one mechanism; the
 *  aliases below exist so it can be found under one name.
 *
 *  THE RULES THEY SHARE — one flat edge store, walked once per render:
 *
 *   1. **AN UNKNOWN KEY IS SILENT, across the whole family.**
 *      `flowAround("typo")`, `spans::fit("typo")`, `around("typo")`, a
 *      connector naming a node that is not in the tree — each resolves to
 *      nothing and draws nothing, with no diagnostic. A misspelled key
 *      looks exactly like a feature you did not write.
 *   2. **ONE SECOND PASS, cycle-guarded.** Backward influence inside a
 *      frame is this declared exception and nothing else: derive answers
 *      are computed from the FIRST layout and fed to at most one more
 *      pass. A borrow that would close a cycle is dropped, not chased.
 *   3. **The answer can lag by a frame** when the borrowed node's own
 *      geometry only settles during that layout, so a borrow taken on the
 *      very first frame may resolve against a not-yet-final rect.
 *   4. **Flat, not recursive.** Routed nodes and flowing text are flat
 *      lists in tree order plus a back-index from anchor key to routes, so
 *      a tree with no derived content pays nothing and `routesAt(key)`
 *      answers in time proportional to the routes at that node.
 */
namespace derive {
/** A relationship as an element — see connector() above. */
using sigil::compose::connector;
/** A path threaded through anchors — see rail() above. */
using sigil::compose::rail;
/** A spine borrowed from a keyed element — `band(derive::around("dial"),
 *  across(14))`. */
using sigil::compose::around;
/** The family's text member as a free verb: `derive::flowAround(el,
 *  "fig", 8)` == `el.flowAround("fig", 8)`. The method is the ergonomic
 *  form, since it chains; this exists so the whole family can be found
 *  under one name. */
inline Element flowAround(Element el, std::string_view key,
                          float margin = 0.0f) {
  el.flowAround(key, margin);
  return el;
}
}  // namespace derive

}  // namespace sigil::compose
