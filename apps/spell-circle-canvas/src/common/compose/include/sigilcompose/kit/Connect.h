#pragma once

/** @file
 * @ingroup compose-kit
 *
 * THE CONNECTING OPERATORS: adding operators that draw a wire between
 * nodes of their scope, the pairing stated in the operator or read off
 * the nodes. A wire is an ordinary element — a path figure the route
 * became, dressed by the decoration the operator carries and keyed by
 * the pair it joins — attached to the scope, so it stands beside the
 * nodes it joins and paints where the operator's `zIndex` says.
 */

#include <include/core/SkPath.h>
#include <sigilcompose/core/Derive.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/core/Shape.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::connect {

/** THE WIRE ITSELF: the route between two rects as a path figure, dressed
 *  and keyed. `bleed` is the room the figure's box keeps around the path
 *  for the decoration's width; an under-grown box clips the wire. */
[[nodiscard]] Element wire(const SkRect& from, const SkRect& to,
                           const Router& router, float gap,
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
