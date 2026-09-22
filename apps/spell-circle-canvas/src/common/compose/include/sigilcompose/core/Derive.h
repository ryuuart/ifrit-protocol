#pragma once

/** @file
 * @ingroup compose-core
 *
 * SigilCompose derive vocabulary — what asks where a keyed node landed:
 * Router and RailRouter, the two route seams, with routeBetween and
 * routeAlong as the one statement of where a route runs and where it
 * ends; Anchor, a normalized point on a keyed node's bounds or a point
 * bound to nothing; Tether, where a box hangs off another; band, a shape
 * swept out by a spine; and bandPointAt, the one statement of the band's
 * across sign.
 *
 * The derive phase itself — resolving text exclusions and borrowed
 * geometry over the flat lists once layout has produced geometry — is a
 * pass in the composer's layout phase list, inside the converging group,
 * so borrowed geometry settles against what the other converging passes
 * move. That registration is the whole of how the family reaches the
 * kernel's schedule.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilcore/comparable/Erased.h>

#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sigil::compose {

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

namespace detail {

/** WHAT A ROUTE SEAM DOES: answer the path between two endpoint rects.
 *  The operations behind `Router`, so the erasure itself is SigilCore's
 *  one mechanism and this header holds only the vocabulary. */
struct RouteOperations {
  virtual ~RouteOperations() = default;
  virtual SkPath route(const SkRect& from, const SkRect& to) const = 0;
};

/** A comparable scheme as those operations. Its equality is the scheme's,
 *  which is what makes two separately-built routers of one kind prune. */
template <RouteScheme R>
struct RouteModel : RouteOperations {
  R scheme;
  explicit RouteModel(R s) : scheme(std::move(s)) {}
  bool operator==(const RouteModel& o) const { return scheme == o.scheme; }
  SkPath route(const SkRect& from, const SkRect& to) const override {
    return scheme.route(from, to);
  }
};

/** The callable escape hatch, which carries no equality at all. */
struct RouteFunction : RouteOperations {
  std::function<SkPath(const SkRect&, const SkRect&)> fn;
  explicit RouteFunction(std::function<SkPath(const SkRect&, const SkRect&)> f)
      : fn(std::move(f)) {}
  SkPath route(const SkRect& from, const SkRect& to) const override {
    return fn ? fn(from, to) : SkPath();
  }
};

}  // namespace detail

/** THE ROUTE BETWEEN TWO RECTS, type-erased: what a connecting operator
 *  holds.
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
  Router(R scheme)  // NOLINT: implicit by design (.router = arc())
      : m_held(detail::RouteModel<R>(std::move(scheme))) {}

  /** The escape hatch: any callable over the two endpoint rects. Never
   *  compares equal to a separately-constructed Router. */
  template <typename F>
    requires(!RouteScheme<std::remove_cvref_t<F>> &&
             !std::same_as<std::remove_cvref_t<F>, Router> &&
             std::is_invocable_r_v<SkPath, const std::remove_cvref_t<F>&,
                                   const SkRect&, const SkRect&>)
  Router(F fn)  // NOLINT: implicit by design (.router = [](…){…})
      : m_held(core::Erased<detail::RouteOperations>(
            detail::RouteFunction(std::move(fn)))) {}

  explicit operator bool() const { return (bool)m_held; }
  SkPath operator()(const SkRect& from, const SkRect& to) const {
    return m_held ? m_held->route(from, to) : SkPath();
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
  bool comparable() const { return m_held.comparable(); }

  /** Shared state (copies of one Router) is equal; comparable schemes of
   *  one type compare their values; anything else is conservative — the
   *  erased value's own rule. */
  bool operator==(const Router& o) const { return m_held == o.m_held; }

 private:
  core::Erased<detail::RouteOperations> m_held;
};

/** THE PATH A ROUTE DRAWS BETWEEN TWO RECTS: @p router's answer, or a
 *  straight line centre to centre when it is empty, with each END pulled
 *  back @p gap px along itself — clamped so a short route keeps a visible
 *  run, and left alone on a closed contour, which has no ends. Routes run
 *  centre to centre and a node's box can be much larger than its visible
 *  shape, so the gap is how a wire stops at the glow instead of piercing
 *  it. The one statement of that rule, so nothing routing a wire can
 *  disagree with anything else about where it ends. */
SkPath routeBetween(const Router& router, const SkRect& from, const SkRect& to,
                    float gap = 0.0f);

/** A WIRE'S STOP — an endpoint or a waypoint. It is ONE OF TWO THINGS,
 *  and the type says which: a point bound to a keyed node, or a point
 *  bound to nothing. `where` is the choice itself, so an anchor carries
 *  the coordinates of the kind it IS and no others — there is no field to
 *  fill that the resolver will not read.
 *
 *  `Anchor::on(key, norm)` is the bound form: a NORMALIZED point on that
 *  node's resolved bounds ((0,0)=top-left, (1,1)=bottom-right — the
 *  binding form tldraw and Excalidraw both converged on; never absolute
 *  coordinates, so a wire survives layout, drag, and reflow). The
 *  constructor spells it too, so `{"port", {1, 0.5f}}` in a run of stops
 *  is an anchor on a node's right edge.
 *
 *  `Anchor::at(point)` is the free form: a point in the coordinates the
 *  run is read in, bound to nothing. A route through a place rather than
 *  through a thing — the bend that clears a corner, the fan-out a
 *  diagram's own drawing puts at a fixed offset — is a real waypoint and
 *  not a node, and standing invisible boxes up to carry those coordinates
 *  mounts, lays out and reconciles a node per bend for a number the
 *  caller already had.
 *
 *  `gap` belongs to neither half: it pulls a TERMINAL anchor back along
 *  its segment (breathing room at the ends; ignored on waypoints).
 *
 *  A run whose anchors are ALL free points is a polyline the router draws
 *  and nothing binds; one that mixes them is the ordinary case — a wire
 *  that leaves a port, turns in the gutter, and arrives at another. */
struct Anchor {
  /** Bound to a node: `norm` is read on that node's resolved bounds. */
  struct OnNode {
    std::string key;
    SkPoint norm = {0.5f, 0.5f};
    bool operator==(const OnNode&) const = default;
  };
  /** Bound to nothing: `point` is read in the rail's own coordinates. */
  struct FreePoint {
    SkPoint point = {0.0f, 0.0f};
    bool operator==(const FreePoint&) const = default;
  };

  /** Spelled with the bound form's own default, so a default-constructed
   *  anchor is an anchor on no node rather than an empty variant. */
  std::variant<OnNode, FreePoint> where = OnNode{};
  float gap = 0.0f;

  Anchor() = default;
  /** The bound form, spelled as the anchor's own constructor so a rail's
   *  initializer list reads `{{"a"}, {"b", {1, 0.5f}}}`. */
  Anchor(std::string key, SkPoint norm = {0.5f, 0.5f}, float gap = 0.0f)
      : where(OnNode{std::move(key), norm}), gap(gap) {}
  /** The free form. */
  static Anchor at(SkPoint point, float gap = 0.0f) {
    Anchor a;
    a.where = FreePoint{point};
    a.gap = gap;
    return a;
  }
  /** The bound form, named, for a call site that reads better with a verb
   *  than with a brace. */
  static Anchor on(std::string key, SkPoint norm = {0.5f, 0.5f},
                   float gap = 0.0f) {
    return Anchor(std::move(key), norm, gap);
  }

  /** The node this anchor is bound to, or empty when it is a free point —
   *  the one question the reads list and the route index ask. */
  std::string_view key() const {
    const OnNode* n = std::get_if<OnNode>(&where);
    return n ? std::string_view(n->key) : std::string_view();
  }
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
 *  taken. When none fits, the first one that RESOLVED stands — the stated
 *  tether wherever it names a node that is there, and otherwise the first
 *  fallback that does — so a box that cannot be placed anywhere is still
 *  placed. A fallback's own `fallbacks` are not read — the list is the
 *  list.
 *
 *  `within` empty is the composer's own bounds, which is what "on screen"
 *  means when nothing narrower is stated.
 *
 *  AN UNKNOWN KEY IS SILENT, the family's rule: a tether naming a node
 *  that is not there places nothing. `pin::Request` is what reads one on
 *  the scope's nodes, and it ignores the `key` — the node stating the
 *  request is the anchor — so the field is for a caller that hangs a box
 *  off a named anchor itself. */
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
    return SkRect::MakeXYWH(anchor.left() + anchor.width() * on.x() +
                                offset.x() - size.width() * at.x(),
                            anchor.top() + anchor.height() * on.y() +
                                offset.y() - size.height() * at.y(),
                            size.width(), size.height());
  }
};

/** A rail-route scheme: `SkPath route(std::span<const SkPoint>) const`,
 *  plus equality — the pointwise seam's half of the same convention
 *  `RouteScheme` states. Equal values must route identical paths through
 *  every anchor run. */
template <typename R>
concept RailScheme = std::equality_comparable<R> &&
                     requires(const R& r, std::span<const SkPoint> anchors) {
                       { r.route(anchors) } -> std::convertible_to<SkPath>;
                     };

namespace detail {

/** WHAT A RAIL SEAM DOES: answer the path through an ordered anchor run.
 *  The rail's half of the same one mechanism the route seam uses. */
struct RailOperations {
  virtual ~RailOperations() = default;
  virtual SkPath route(std::span<const SkPoint> anchors) const = 0;
};

template <RailScheme R>
struct RailModel : RailOperations {
  R scheme;
  explicit RailModel(R s) : scheme(std::move(s)) {}
  bool operator==(const RailModel& o) const { return scheme == o.scheme; }
  SkPath route(std::span<const SkPoint> anchors) const override {
    return scheme.route(anchors);
  }
};

/** The callable escape hatch, which carries no equality at all. */
struct RailFunction : RailOperations {
  std::function<SkPath(std::span<const SkPoint>)> fn;
  explicit RailFunction(std::function<SkPath(std::span<const SkPoint>)> f)
      : fn(std::move(f)) {}
  SkPath route(std::span<const SkPoint> anchors) const override {
    return fn ? fn(anchors) : SkPath();
  }
};

}  // namespace detail

/** THE PATH THROUGH AN ORDERED RUN OF ANCHORS, type-erased: what
 *  `connect::Along` holds. Stock values in <sigilcompose/kit/Routers.h>
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
  RailRouter(R scheme)  // NOLINT: implicit by design (.router = polyline())
      : m_held(detail::RailModel<R>(std::move(scheme))) {}

  /** The escape hatch: any callable over the resolved anchor run. Never
   *  compares equal to a separately-constructed RailRouter. */
  template <typename F>
    requires(!RailScheme<std::remove_cvref_t<F>> &&
             !std::same_as<std::remove_cvref_t<F>, RailRouter> &&
             std::is_invocable_r_v<SkPath, const std::remove_cvref_t<F>&,
                                   std::span<const SkPoint>>)
  RailRouter(F fn)  // NOLINT: implicit by design (.router = [](auto p){…})
      : m_held(core::Erased<detail::RailOperations>(
            detail::RailFunction(std::move(fn)))) {}

  explicit operator bool() const { return (bool)m_held; }
  SkPath operator()(std::span<const SkPoint> anchors) const {
    return m_held ? m_held->route(anchors) : SkPath();
  }
  /** A RailRouter is itself a scheme, so it NESTS — same reason a Router
   *  and a Shape do. */
  SkPath route(std::span<const SkPoint> anchors) const {
    return (*this)(anchors);
  }
  /** Does this value participate in structural equality? (False for the
   *  callable escape hatch.) */
  bool comparable() const { return m_held.comparable(); }

  /** Shared state (copies of one RailRouter) is equal; comparable schemes
   *  of one type compare their values; anything else is conservative. */
  bool operator==(const RailRouter& o) const { return m_held == o.m_held; }

 private:
  core::Erased<detail::RailOperations> m_held;
};

/** THE PATH THROUGH AN ORDERED RUN OF POINTS: @p router's answer, or the
 *  straight polyline through them when it is empty, with the FIRST point
 *  pulled @p gapStart px back along its own segment and the LAST one
 *  @p gapEnd px back along its — clamped so a short segment keeps a
 *  visible run and a two-point run pulled from both ends cannot invert.
 *  Fewer than two points draw nothing. The one statement of that rule, so
 *  an operator threading a wire through stops and the kernel cannot
 *  disagree about where the wire ends. */
SkPath routeAlong(const RailRouter& router, std::span<const SkPoint> stops,
                  float gapStart = 0.0f, float gapEnd = 0.0f);

/** A BAND: the shape a spine sweeps out at a given width across it.
 *
 *      band(shapes::circle(), across(22))
 *          .bandAlignment(geometry::path::Formation::Inner).fill(brass)
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
 *  The spine is a `Shape`, exactly like shape()'s value: a comparable
 *  generator (any `shapes::` value) prunes; a raw callable is the escape
 *  hatch that never compares equal — memo() such a node, or hold the
 *  Shape value stable, to prune it. A band along a node's own resolved
 *  outline is `outline::Around`, the operator, which hands this verb a
 *  held path.
 *
 *  Formation is explicit: `bandAlignment()` takes `Formation::Center`
 *  (the default), which straddles the spine, or `Outer` or `Inner`,
 *  which take one side of it. The spine is guide DATA, never an element —
 *  a path participates as an element's shape, as an operator's reading of
 *  a settled node's outline, or as pure guide data in no tree, and this
 *  is the third case.
 *
 *  The profile's `max()` is what the paint cull grows by, so a band whose
 *  width varies is never silently clipped. */
Band band(Shape spine, Across width);

/** The band's own (along, across) space, addressable: `along` is a
 *  fraction of the spine's total arc length, `across` is px on the normal.
 *
 *  **Positive `across` is to the LEFT of travel**, which in screen space
 *  (y down) is OUTSIDE a clockwise path — SkPath's own direction for rects
 *  and circles, so `Formation::Outer` exits the shape.
 *
 *  THIS IS THE ONE STATEMENT OF THAT CONVENTION for the whole library.
 *  `Profile::across`, `strand::offset`, `geometry::parallel`,
 *  `lines::Rail::across`, `geometry::shapes::offset` and
 *  `TextPath::offset` all mean this same side. Anything placing content on
 *  a band reads it here, so the placement and the band's own geometry
 *  cannot disagree. */
SkPoint bandPointAt(const SkPath& spine, float along, float acrossPx);

// ---------------------------------------------------------------------------
// WHAT EVERY DERIVATION SHARES
//
// A derivation is content whose input is another keyed node's RESOLVED
// geometry: `Text::contentFlowAround`, `spans::fit`, a decoration's
// `strand::from`, `Text::textThreadTo`, and every adding operator that
// reads a `Scope`. One flat store, walked once per render, under three
// rules:
//
//  1. AN UNKNOWN KEY IS SILENT. `contentFlowAround("typo")`,
//     `spans::fit("typo")`, an operator naming a node that is not in its
//     scope — each resolves to nothing and draws nothing, with no
//     diagnostic. A misspelled key looks exactly like a feature you did
//     not write.
//  2. ONE SECOND PASS, cycle-guarded. Backward influence inside a frame
//     is this declared exception and nothing else: answers are computed
//     from the FIRST layout and fed to at most one more pass. A borrow
//     that would close a cycle is dropped, not chased.
//  3. THE ANSWER CAN LAG BY A FRAME when the borrowed node's own geometry
//     only settles during that layout, so a borrow taken on the very
//     first frame may resolve against a not-yet-final rect.


}  // namespace sigil::compose
