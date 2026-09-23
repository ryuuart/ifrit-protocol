#pragma once

/** @file
 * @ingroup compose-core
 *
 * WHAT A RULE STATES, AND THE SHEET THAT ORDERS RULES: a selector
 * paired with the partials it lays on every element it speaks about,
 * and the ordered, comparable value of such rules that a node applies
 * to its subtree.
 */

#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/PaintAnchor.h>
#include <sigilcompose/core/Selector.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Var.h>
#include <sigilcompose/core/verbs/Box.h>
#include <sigilcompose/core/verbs/Cascade.h>
#include <sigilcompose/core/verbs/Effects.h>
#include <sigilcompose/core/verbs/Flex.h>
#include <sigilcompose/core/verbs/Font.h>
#include <sigilcompose/core/verbs/Paint.h>
#include <sigilcompose/core/verbs/Placement.h>
#include <sigilcompose/core/verbs/Shape.h>
#include <sigilcompose/core/verbs/TextStyle.h>
#include <sigilcompose/core/verbs/Transform.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Transition.h>
#include <sigilweave/layout/Block.h>
#include <sigilweave/style/Type.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::compose {

namespace detail {
struct SheetBody;
struct SheetAccess;
}  // namespace detail

/** ONE RULE: which elements it speaks about, and what it states about
 *  them. Its verbs ARE the element's — the same families, signatures and
 *  briefs, writing the same declarations — so a property is spelled once
 *  whether a rule or a verb says it, and the element's own verb stands
 *  over the rule's. What a rule leaves unsaid the element inherits or
 *  keeps at its initial value, as the property's own behaviour says.
 *
 *  A rule states the TEXT PROPERTIES too, which an element cannot: they
 *  reach the text leaves a rule matches and nothing else. It never states
 *  the element's structure, identity or callbacks.
 *  @trap A rule holds STATIC values. A live binding, an entrance and an
 *  animation stay verbs on the element; one written here is left out of
 *  the fold and said once. What a rule cannot carry at all — a shape
 *  generator, a grid area, a filter, a travel path, `cover()`'s flag, an
 *  exclusion — does not compile on a rule. */
class Rule : public detail::Declaring,
             public BoxVerbs<Rule>,
             public FlexVerbs<Rule>,
             public PlacementVerbs<Rule>,
             public ShapeVerbs<Rule>,
             public CascadeVerbs<Rule>,
             public FontVerbs<Rule>,
             public PaintVerbs<Rule>,
             public EffectVerbs<Rule>,
             public TransformVerbs<Rule>,
             public TextStyleVerbs<Rule> {
 public:
  explicit Rule(ElementSelector subject);

  using CascadeVerbs<Rule>::paragraph;

  /** HOW A MATCHED ELEMENT'S VALUES CHANGE when a later describe moves
   *  them: the element's `transition`, stated by the rule, so a class
   *  toggle that recolours or resizes an element eases rather than
   *  snapping. The element's own `transition()` stands over it, and
   *  among matched rules the strongest that states one wins. */
  Rule& transition(motion::Transition how);

  // What a rule cannot carry, refused where it is written. Each is kept
  // on the element's description rather than in the style the cascade
  // folds, so a rule has nowhere to put it.
  template <class... Arguments>
  Rule& shape(Arguments&&...) = delete;
  Rule& cover() = delete;
  Rule& gridArea(std::string_view) = delete;
  Rule& filter(material::skia::Effect) = delete;
  Rule& backdropFilter(material::skia::Effect) = delete;
  Rule& travel(MotionPath) = delete;
  Rule& contentFlowAround(std::string_view, float = 0.0f) = delete;

  /** Which elements this rule speaks about. */
  [[nodiscard]] const ElementSelector& selector() const { return m_selector; }
  /** The font half of what it states. */
  [[nodiscard]] const sigil::weave::Type& type() const;
  /** The paragraph half of what it states. */
  [[nodiscard]] const sigil::weave::ParagraphBlock& paragraph() const;
  /** The property the ink reads, where it was written as one. */
  [[nodiscard]] const std::optional<VarRef>& inkVar() const;
  /** The paint the ink is, where it was written as one. */
  [[nodiscard]] const std::optional<material::skia::Paint>& inkPaint() const;
  /** The box that paint's unit square maps onto. */
  [[nodiscard]] PaintAnchor inkAnchor() const;
  /** Whether this rule writes the ink lane at all — a colour, a
   *  property, a paint, or an empty paint, which is the lane cleared. */
  [[nodiscard]] bool statesInk() const;
  /** The custom properties it sets. */
  [[nodiscard]] const VarTable& vars() const;
  /** The transition it states, where it states one. */
  [[nodiscard]] const std::optional<motion::Transition>& transition() const;

  [[nodiscard]] bool operator==(const Rule& other) const;

 private:
  friend struct detail::NodeAccess;
  ElementSelector m_selector;
};

/** A rule speaking about the elements @p cssText names. A text this
 *  library does not read matches nothing, so the rule states what it
 *  states about no element at all. */
[[nodiscard]] Rule rule(std::string_view cssText);
/** A rule speaking about the elements @p subject names. */
[[nodiscard]] Rule rule(ElementSelector subject);

/** THE RULES A SHEET STATES, in the order they were written, as one
 *  immutable value: declared once and applied at as many subtrees as
 *  the author likes, and compiled once — each rule filed under the
 *  element its selector's subject names. Copies are cheap and share
 *  that one form, and two of them compare by pointer before they
 *  compare by value, which is what lets a reconcile ask cheaply whether
 *  the sheets a node applies changed.
 *  @trap Order is only the LAST tiebreak. Which rule wins at an element
 *  is CSS's: specificity first, and order after it. */
class StyleSheet {
 public:
  /** ONE STATEMENT OF A SHEET LITERAL: a rule, or another sheet whose
   *  rules stand in its place, in order — which is what an import is
   *  here. */
  class Statement {
   public:
    Statement(Rule one);  // NOLINT: implicit by design (a sheet literal)
    Statement(const StyleSheet& included);  // NOLINT: implicit by design

    [[nodiscard]] const std::vector<Rule>& rules() const { return m_rules; }

   private:
    std::vector<Rule> m_rules;
  };

  StyleSheet() = default;
  StyleSheet(std::initializer_list<Statement> statements);

  /** Every rule the sheet states, an included sheet's rules standing
   *  where it stood. */
  [[nodiscard]] const std::vector<Rule>& rules() const;
  [[nodiscard]] size_t size() const { return rules().size(); }
  [[nodiscard]] bool empty() const { return rules().empty(); }

  [[nodiscard]] bool operator==(const StyleSheet& other) const;

 private:
  friend struct detail::SheetAccess;
  std::shared_ptr<const detail::SheetBody> m_body;
};

/** THE TWO SHEETS AS ONE, @p later's rules after @p earlier's:
 *  `house + darkTheme + local`. Later is only a tiebreak, so a later
 *  rule wins where the two weigh the same and loses where it weighs
 *  less. */
[[nodiscard]] StyleSheet operator+(const StyleSheet& earlier,
                                   const StyleSheet& later);

}  // namespace sigil::compose
