#pragma once

/** @file
 * @ingroup compose-kit
 *
 * THE OUTLINE OPERATORS: adding operators that build a figure from where
 * nodes resolved their edges — a band along one node's outline, and a
 * hull enclosing a set of nodes. Both read the outline the derive family
 * reads: a node's shape, a routed path, or its box when it declares none.
 */

#include <sigilcompose/core/Derive.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilgeometry/path/Band.h>

#include <limits>
#include <optional>
#include <string>

namespace sigil::compose::outline {

/** A BAND ALONG ONE NODE'S OUTLINE, `across` wide, attached to that node
 *  in its own coordinates so it moves, turns and goes with it — the ring
 *  round a dial, the halo round a selected card. The band is a `Band` in
 *  every way: `formation` puts it on, inside or outside the edge, and
 *  `fill` is what it is painted with, or nothing when unset.
 *
 *      box().children({dial}).operators({outline::Around{
 *          .key = "dial", .across = across(14),
 *          .formation = geometry::path::Formation::Outer, .fill = brass}})
 *
 *  A key the scope does not carry draws nothing, silently. */
struct Around {
  std::string key;
  Across across = compose::across(8.0f);
  geometry::path::Formation formation = geometry::path::Formation::Center;
  std::optional<Fill> fill;
  bool operator==(const Around&) const = default;

  void add(Scope& scope) const;
};

/** THE HULL ENCLOSING A SET OF NODES: every node stating a fact under
 *  `lane`, or naming class `styleClass`, when `lane` is empty. The
 *  outlines are flattened, their points hulled — the convex hull at the
 *  default `alpha`, an outline that reaches into concavities at a smaller
 *  one — and grown by `margin` px; the figure is attached to the scope,
 *  keyed `hull:<lane or class>`, and painted with `fill` when set.
 *
 *      .operators({Operator(outline::Hull{.styleClass = "selected",
 *                                         .margin = 12, .fill = glow})
 *                      .zIndex(-1)})
 *
 *  Fewer than three distinct points enclose nothing and attach nothing. */
struct Hull {
  std::string lane;
  std::string styleClass;
  float alpha = std::numeric_limits<float>::infinity();
  float margin = 0.0f;
  std::optional<Fill> fill;
  bool operator==(const Hull&) const = default;

  void add(Scope& scope) const;
};

}  // namespace sigil::compose::outline
