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
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Selector.h>
#include <sigilcompose/core/Var.h>
#include <sigilmaterial/color/Color.h>
#include <sigilweave/layout/Block.h>
#include <sigilweave/style/Type.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::compose {

/** ONE RULE: which elements it speaks about, and what it states about
 *  them. It states the same partials the node's own verbs write, folded
 *  by the same merge, so a property is spelled once whether a rule or a
 *  verb says it; what a rule leaves unsaid the element inherits.
 *  @trap A rule holds STATIC values. A live binding, an entrance and an
 *  animation stay verbs on the element. */
class Rule {
 public:
  explicit Rule(ElementSelector subject) : m_selector(std::move(subject)) {}

  /** The font partial: @p partial's fields over what this rule already
   *  states, the later call winning field by field. */
  Rule& font(sigil::weave::Type partial);
  /** The block partial, folded the same way. */
  Rule& block(sigil::weave::Block partial);
  /** The ink — the font's colour, which everything under a matched
   *  element inherits. */
  Rule& ink(material::Color colour);
  /** The ink read from a custom property, resolved where the rule
   *  matches. Exclusive with a colour: the later call stands. */
  Rule& ink(VarRef reference);
  /** A custom property set on every element this rule matches, for that
   *  element and everything under it. */
  Rule& var(std::string_view name, material::Color colour);
  Rule& var(std::string_view name, Dimension length);

  /** Which elements this rule speaks about. */
  [[nodiscard]] const ElementSelector& selector() const { return m_selector; }
  /** The font half of what it states. */
  [[nodiscard]] const sigil::weave::Type& type() const { return m_type; }
  /** The block half of what it states. */
  [[nodiscard]] const sigil::weave::Block& block() const { return m_block; }
  /** The property the ink reads, where it was written as one. */
  [[nodiscard]] const std::optional<VarRef>& inkVar() const { return m_inkVar; }
  /** The custom properties it sets. */
  [[nodiscard]] const VarTable& vars() const { return m_vars; }

  bool operator==(const Rule&) const = default;

 private:
  ElementSelector m_selector;
  sigil::weave::Type m_type;
  sigil::weave::Block m_block;
  std::optional<VarRef> m_inkVar;
  VarTable m_vars;
};

/** A rule speaking about the elements @p cssText names. A text this
 *  library does not read matches nothing, so the rule states what it
 *  states about no element at all. */
[[nodiscard]] Rule rule(std::string_view cssText);
/** A rule speaking about the elements @p subject names. */
[[nodiscard]] Rule rule(ElementSelector subject);

/** THE RULES A SHEET STATES, in the order they were written, as one
 *  immutable value: declared once and applied at as many subtrees as
 *  the author likes. Copies are cheap and share one stored form, and
 *  two of them compare by pointer before they compare by value, which
 *  is what lets a reconcile ask cheaply whether the sheets a node
 *  applies changed.
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
  std::shared_ptr<const std::vector<Rule>> m_rules;
};

/** THE TWO SHEETS AS ONE, @p later's rules after @p earlier's:
 *  `house + darkTheme + local`. Later is only a tiebreak, so a later
 *  rule wins where the two weigh the same and loses where it weighs
 *  less. */
[[nodiscard]] StyleSheet operator+(const StyleSheet& earlier,
                                   const StyleSheet& later);

}  // namespace sigil::compose
