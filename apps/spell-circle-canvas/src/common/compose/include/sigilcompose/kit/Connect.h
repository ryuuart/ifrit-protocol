#pragma once

/** @file
 * @ingroup compose-kit
 *
 * THE CONNECTING OPERATORS: adding operators that draw a wire between
 * nodes of their scope — the pair stated in the operator, a run of stops
 * stated in it, or every pairing read off the nodes. A wire is an
 * ordinary element — a path figure the route became, dressed by the
 * decoration the operator carries and keyed by the nodes it joins —
 * attached to the scope, so it stands beside those nodes and paints where
 * the operator's `zIndex` says.
 */

#include <include/core/SkPath.h>
#include <sigilcompose/core/Derive.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/core/Shape.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sigil::compose::connect {

/** THE WIRE ITSELF: the route between two rects as a path figure, dressed
 *  and keyed. `bleed` is the room the figure's box keeps around the path
 *  for the decoration's width; an under-grown box clips the wire. */
[[nodiscard]] Element wire(const SkRect& from, const SkRect& to,
                           const Router& router, float gap,
                           const std::optional<Decoration>& dress, float bleed,
                           std::string key);

/** THE SAME WIRE THROUGH A RUN OF STOPS: the route along @p stops as a
 *  path figure, with the ends held @p gapStart and @p gapEnd px clear of
 *  the first and last of them. */
[[nodiscard]] Element wire(std::span<const SkPoint> stops,
                           const RailRouter& router, float gapStart,
                           float gapEnd,
                           const std::optional<Decoration>& dress, float bleed,
                           std::string key);

/** ONE WIRE BETWEEN TWO KEYED NODES, the pairing stated here:
 *
 *      connect::Between{.from = "gateway", .to = "ledger",
 *                       .router = routers::arc(), .wire = rule}
 *
 *  A key the scope does not carry draws nothing, silently, as an unknown
 *  key does across the derive family. */
struct Between {
  std::string from;
  std::string to;
  Router router;
  float gap = 0.0f;
  /// What dresses the wire; unset draws the figure with no mark of its own.
  std::optional<Decoration> wire;
  float bleed = 6.0f;
  bool operator==(const Between&) const = default;

  void add(Scope& scope) const {
    const Scope::Node* start = scope.find(from);
    const Scope::Node* end = scope.find(to);
    if (!start || !end) return;
    scope.attach(connect::wire(start->bounds, end->bounds, router, gap, wire,
                               bleed, from + "->" + to));
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
 *  stops' keys joined with "->", a free waypoint contributing nothing. */
struct Along {
  std::vector<Anchor> stops;
  RailRouter router;
  /// What dresses the wire; unset draws the figure with no mark of its own.
  std::optional<Decoration> wire;
  float bleed = 6.0f;
  bool operator==(const Along&) const = default;

  void add(Scope& scope) const {
    std::vector<SkPoint> points;
    points.reserve(stops.size());
    std::string key;
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
      if (!key.empty()) key += "->";
      key += on.key;
    }
    if (points.size() < 2) return;
    scope.attach(connect::wire(points, router, stops.front().gap,
                               stops.back().gap, wire, bleed, std::move(key)));
  }
};

/** A WIRE PER PAIRING THE NODES STATE: every node with a fact under
 *  `lane` — one key, or a list of keys — is wired to each node it names.
 *
 *      card.attribute("calls", std::vector<std::string>{"ledger", "cache"});
 *      box().children({…}).operators({connect::ByLane{.lane = "calls"}})
 *
 *  A name the scope does not carry is skipped, silently. */
struct ByLane {
  std::string lane;
  Router router;
  float gap = 0.0f;
  std::optional<Decoration> wire;
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
        scope.attach(connect::wire(node->bounds, target->bounds, router, gap,
                                   wire, bleed, node->key + "->" + name));
      }
    }
  }
};

}  // namespace sigil::compose::connect
