#pragma once

/** @file
 * @ingroup weave-layout
 *
 * `Rule` and `StyleSheet` — the classes a tree states: rules under names,
 * each a type half and a block half, written with the verbs the tree is
 * written with.
 */

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilweave/layout/Block.h"
#include "sigilweave/style/TypeSheet.h"

namespace sigil::weave {

/** ONE CLASS OF A SHEET: a name, and what it states — a partial over the
 *  type and a partial over the block, either or both. Spelled as a
 *  literal by the half it names, `{"note", {.size = 11}}`, or with the
 *  verbs. What a rule leaves unsaid is what the node inherits. */
class Rule {
 public:
  explicit Rule(std::string name) : m_name(std::move(name)) {}
  Rule(std::string name, Type type)
      : m_name(std::move(name)), m_type(std::move(type)) {}
  Rule(std::string name, Block block)
      : m_name(std::move(name)), m_block(std::move(block)) {}

  /** The type half: @p partial's fields over what this rule already states. */
  Rule& font(Type partial) {
    merge(m_type, partial);
    return *this;
  }
  /** The block half: @p partial's fields over what this rule already states. */
  Rule& block(Block partial) {
    merge(m_block, partial);
    return *this;
  }

  [[nodiscard]] const std::string& name() const { return m_name; }
  [[nodiscard]] const Type& type() const { return m_type; }
  [[nodiscard]] const Block& block() const { return m_block; }
  bool operator==(const Rule&) const = default;

 private:
  std::string m_name;
  Type m_type;
  Block m_block;
};

/** A rule under @p name, stating nothing yet. */
[[nodiscard]] inline Rule rule(std::string name) {
  return Rule(std::move(name));
}

/** THE CLASSES A TREE STATES, as one value: rules in the order they were
 *  written, comparable by value, and a base style for the runs of a rich
 *  text that name nothing. Lookup is a linear scan over a handful of
 *  classes. `StyleSheet::types` is the type half alone, which the
 *  paragraph layer shapes rich runs through.
 *  @trap A name stated again ADDS to its rule, the later fields standing
 *  and the rest as they were, rather than replacing it. */
class StyleSheet {
 public:
  StyleSheet() = default;
  explicit StyleSheet(TextStyle baseStyle) : m_base(std::move(baseStyle)) {}
  /** A sheet spelled as a literal, a rule per line. */
  StyleSheet(std::initializer_list<Rule> rules) {
    for (const Rule& r : rules) set(r);
  }
  StyleSheet(TextStyle baseStyle, std::initializer_list<Rule> rules)
      : m_base(std::move(baseStyle)) {
    for (const Rule& r : rules) set(r);
  }

  /** The style a rich run that names nothing is set in, and the base of
   *  `types()`. */
  StyleSheet& base(TextStyle style) {
    m_base = std::move(style);
    return *this;
  }
  [[nodiscard]] const TextStyle& base() const { return m_base; }

  /** States @p r: added to the rule of that name where one stands, its
   *  later fields winning; appended otherwise. */
  StyleSheet& set(const Rule& r) {
    for (Rule& mine : m_rules)
      if (mine.name() == r.name()) {
        mine.font(r.type()).block(r.block());
        return *this;
      }
    m_rules.push_back(r);
    return *this;
  }
  /** The type half of @p name. */
  StyleSheet& set(std::string name, Type partial) {
    return set(Rule(std::move(name), std::move(partial)));
  }
  /** The block half of @p name. */
  StyleSheet& set(std::string name, Block partial) {
    return set(Rule(std::move(name), std::move(partial)));
  }

  /** The base with @p name's type half over it, or the base alone when no
   *  such rule stands — a whole style for a caller that sets one whole
   *  style per row and inherits nothing. */
  [[nodiscard]] TextStyle operator[](std::string_view name) const {
    const Rule* r = find(name);
    return r != nullptr ? overlay(m_base, r->type()) : m_base;
  }
  /** The rule under @p name, or null. */
  [[nodiscard]] const Rule* find(std::string_view name) const {
    for (const Rule& r : m_rules)
      if (r.name() == name) return &r;
    return nullptr;
  }
  [[nodiscard]] bool contains(std::string_view name) const {
    return find(name) != nullptr;
  }
  /** The rules, in the order they were stated. */
  [[nodiscard]] const std::vector<Rule>& rules() const { return m_rules; }
  [[nodiscard]] size_t size() const { return m_rules.size(); }
  [[nodiscard]] bool empty() const { return m_rules.empty(); }

  /** The type half, with the base: every rule's type partial under its
   *  name, for the paragraph layer. */
  [[nodiscard]] TypeSheet types() const {
    TypeSheet half(m_base);
    for (const Rule& r : m_rules) half.set(r.name(), r.type());
    return half;
  }

  bool operator==(const StyleSheet&) const = default;

 private:
  TextStyle m_base;
  std::vector<Rule> m_rules;
};

}  // namespace sigil::weave
