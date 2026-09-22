#pragma once

/** @file
 * @ingroup compose-kit
 *
 * THE CONNECTING OPERATORS: adding operators that draw a wire between
 * nodes of their scope — the pair stated in the operator, a run of stops
 * stated in it, or every pairing read off the nodes. A wire is an
 * ordinary element — a path figure the route became, dressed by what the
 * operator carries and keyed by the nodes it joins — attached to the
 * scope, so it stands beside those nodes and paints where the operator's
 * `zIndex` says.
 */

#include <include/core/SkPath.h>
#include <sigilcompose/core/Derive.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Mask.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/Stroke.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sigil::compose::connect {

/** HOW A WIRE IS DRESSED — the paint half of every connecting operator,
 *  stated in one place so the operators cannot drift apart:
 *
 *  - `mark` is what the wire is drawn with; unset leaves the figure with
 *    no mark of its own.
 *  - `where` is WHERE ON THE WIRE that mark paints — the whole of it when
 *    unset, and a CLAIMED RUN when stated, which is how a wire draws
 *    itself on (`spans::upTo(animate(from(0).to(1), {620ms}))`) or carries
 *    a travelling pulse (`spans::range(&begin, &end)`). A claim fits the
 *    mark to the run it claims, which is not what a gate over the whole
 *    mark does.
 *  - `gate` is what of the wire is SHOWN, as `Element::mask` gates any
 *    node: the reveal that draws a whole dress on at once.
 *  - `style` is the wire's whole dress where one mark will not do — the
 *    marks under it, the marks over it and the echoes beneath, as
 *    `Element::layerStyle` takes them. A cased road is a bed, a lane and
 *    a curb.
 *
 *  Every member is a comparable value, so an operator stating the same
 *  dress every frame prunes. */
struct Dressing {
  std::optional<Decoration> mark;
  std::optional<Spans> where;
  std::optional<Gate> gate;
  LayerStyle style;
  bool operator==(const Dressing&) const = default;
};

/** THE WIRE ITSELF: the route between two rects as a path figure, dressed
 *  and keyed. `bleed` is the room the figure's box keeps around the path
 *  for the dress's width; an under-grown box clips the wire. */
[[nodiscard]] Element wire(const SkRect& from, const SkRect& to,
                           const Router& router, float gap, float bleed,
                           std::string key, const Dressing& dressing);

/** THE SAME WIRE THROUGH A RUN OF STOPS: the route along @p stops as a
 *  path figure, with the ends held @p gapStart and @p gapEnd px clear of
 *  the first and last of them. */
[[nodiscard]] Element wire(std::span<const SkPoint> stops,
                           const RailRouter& router, float gapStart,
                           float gapEnd, float bleed, std::string key,
                           const Dressing& dressing);

/** ONE WIRE BETWEEN TWO KEYED NODES, the pairing stated here:
 *
 *      connect::Between{.from = "gateway", .to = "ledger",
 *                       .router = routers::arc(), .wire = rule}
 *
 *  A key the scope does not carry draws nothing, silently, as an unknown
 *  key does across the derive family. The wire is keyed `from->to`, or by
 *  `key` where that is stated — which is what two wires between the same
 *  pair need, and what a verb reading the wire elsewhere addresses it by. */
struct Between {
  std::string from;
  std::string to;
  Router router;
  float gap = 0.0f;
  /// What dresses the wire; unset draws the figure with no mark of its own.
  std::optional<Decoration> wire;
  /// Where on the wire `wire` paints — see Dressing::where.
  std::optional<Spans> where;
  /// What of the wire is shown — see Dressing::gate.
  std::optional<Gate> mask;
  /// The wire's whole dress, where one mark will not do.
  LayerStyle style;
  float bleed = 6.0f;
  /// The wire's key; `from->to` when empty.
  std::string key;
  bool operator==(const Between&) const = default;

  void add(Scope& scope) const {
    const Scope::Node* start = scope.find(from);
    const Scope::Node* end = scope.find(to);
    if (!start || !end) return;
    scope.attach(connect::wire(
        start->bounds, end->bounds, router, gap, bleed,
        key.empty() ? from + "->" + to : key,
        {.mark = wire, .where = where, .gate = mask, .style = style}));
  }
};

/** ONE WIRE THROUGH AN ORDERED RUN OF STOPS — a transit line through its
 *  stations, a wire that leaves a port and turns in the gutter before it
 *  arrives:
 *
 *      connect::Along{.stops = {{"port"}, Anchor::at({120, 40}), {"bus"}},
 *                     .router = routers::octilinear(8), .wire = rule}
 *
 *  A stop is an `Anchor`, and the two kinds it is are the two kinds it is
 *  everywhere: bound to a node, it is a NORMALISED point on that node's
 *  bounds ((0,0) top-left, (1,1) bottom-right), resolved on the scope's
 *  nodes so the wire survives layout and reflow; free (`Anchor::at`), it
 *  is a point in the SCOPE's coordinates, bound to nothing — a route
 *  through a place rather than through a thing, so a bend needs no node
 *  stood up to carry its coordinates. `gap` on a TERMINAL stop holds that
 *  end of the wire back along its own segment; on a waypoint it is
 *  ignored.
 *
 *  A stop naming a key the scope does not hold draws nothing, silently,
 *  as an unknown key does throughout. The wire is keyed by its bound
 *  stops' keys joined with "->", a free waypoint contributing nothing, or
 *  by `key` where that is stated. */
struct Along {
  std::vector<Anchor> stops;
  RailRouter router;
  /// What dresses the wire; unset draws the figure with no mark of its own.
  std::optional<Decoration> wire;
  /// Where on the wire `wire` paints — see Dressing::where.
  std::optional<Spans> where;
  /// What of the wire is shown — see Dressing::gate.
  std::optional<Gate> mask;
  /// The wire's whole dress, where one mark will not do.
  LayerStyle style;
  float bleed = 6.0f;
  /// The wire's key; its bound stops' keys, joined, when empty.
  std::string key;
  bool operator==(const Along&) const = default;

  void add(Scope& scope) const {
    std::vector<SkPoint> points;
    points.reserve(stops.size());
    std::string joined;
    for (const Anchor& stop : stops) {
      if (const Anchor::FreePoint* free =
              std::get_if<Anchor::FreePoint>(&stop.where)) {
        points.push_back(free->point);  // bound to nothing
        continue;
      }
      const Anchor::OnNode& on = std::get<Anchor::OnNode>(stop.where);
      const Scope::Node* node = scope.find(on.key);
      if (!node) return;  // a key the scope does not hold draws nothing
      points.push_back(
          {node->bounds.left() + node->bounds.width() * on.norm.x(),
           node->bounds.top() + node->bounds.height() * on.norm.y()});
      if (!joined.empty()) joined += "->";
      joined += on.key;
    }
    if (points.size() < 2) return;
    scope.attach(connect::wire(
        points, router, stops.front().gap, stops.back().gap, bleed,
        key.empty() ? std::move(joined) : key,
        {.mark = wire, .where = where, .gate = mask, .style = style}));
  }
};

/** A WIRE PER PAIRING THE NODES STATE: every node with a fact under
 *  `lane` — one key, or a list of keys — is wired to each node it names.
 *
 *      card.attribute("calls", std::vector<std::string>{"ledger", "cache"});
 *      box().children({…}).operators({connect::ByLane{.lane = "calls"}})
 *
 *  A name the scope does not carry is skipped, silently. Each wire is
 *  keyed by the pair it joins. */
struct ByLane {
  std::string lane;
  Router router;
  float gap = 0.0f;
  std::optional<Decoration> wire;
  /// Where on each wire `wire` paints — see Dressing::where.
  std::optional<Spans> where;
  /// What of each wire is shown — see Dressing::gate.
  std::optional<Gate> mask;
  /// Each wire's whole dress, where one mark will not do.
  LayerStyle style;
  float bleed = 6.0f;
  bool operator==(const ByLane&) const = default;

  void add(Scope& scope) const {
    for (const Scope::Node* node : scope.having(lane)) {
      std::vector<std::string> names;
      if (auto one = node->attribute<std::string>(lane))
        names.push_back(*one);
      else if (auto many = node->attribute<std::vector<std::string>>(lane))
        names = *many;
      for (const std::string& name : names) {
        const Scope::Node* target = scope.find(name);
        if (!target) continue;
        scope.attach(connect::wire(
            node->bounds, target->bounds, router, gap, bleed,
            node->key + "->" + name,
            {.mark = wire, .where = where, .gate = mask, .style = style}));
      }
    }
  }
};

}  // namespace sigil::compose::connect
