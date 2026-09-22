#pragma once

/** @file
 * @ingroup compose-kit
 *
 * THE STAMPING OPERATOR: an adding operator that attaches an element to
 * every node stating a fact under a lane, made by a function of the node
 * — the numeral on each hour of a dial, the badge on each card that
 * states a count, the tick on each station.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Operator.h>

#include <functional>
#include <string>
#include <string_view>

namespace sigil::compose::stamp {

/** WHAT MAKES ONE STAMP: the element for a node, given the node as the
 *  operator sees it — its key, its facts, its bounds in the scope's
 *  coordinates. The element is attached to THAT node, in the node's own
 *  coordinates, so a stamp centred on the node is
 *  `.centerAt({node.bounds.width() / 2, node.bounds.height() / 2})`. */
using Maker = std::function<Element(const Scope::Node&)>;

/** AN ELEMENT MADE FOR EVERY NODE STATING A FACT under `lane`, attached
 *  to that node and keyed `<node key>-stamp`.
 *
 *      box().children({each(hours, hour)}).operators({
 *          layouts::Radial{.lane = "hour", .divisions = 12},
 *          stamp::ByLane{.lane = "hour", .key = "numerals",
 *                        .make = [](const Scope::Node& node) {
 *                          return text(std::to_string(*node.attribute<int>("hour")));
 *                        }}})
 *
 *  A maker is a callable with no equality, so the KEY is the operator's:
 *  equal keys assert equal makers, the contract `custom(key)` states, and
 *  an unkeyed stamp compares equal to nothing and its node is described
 *  afresh every frame. */
struct ByLane {
  std::string lane;
  std::string key;
  Maker make;
  bool operator==(const ByLane& other) const {
    return !key.empty() && key == other.key && lane == other.lane;
  }

  void add(Scope& scope) const;
};

}  // namespace sigil::compose::stamp
