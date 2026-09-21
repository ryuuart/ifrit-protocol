/** @file
 * A rule's partials, and the sheet that holds rules in order: how a
 * statement of a literal flattens, how two sheets join, and how two
 * sheets compare.
 */

#include <sigilcompose/core/StyleSheet.h>

#include <utility>

namespace sigil::compose {

// ---------------------------------------------------------------------------
// A rule

Rule& Rule::font(sigil::weave::Type partial) {
  // Later wins field by field, exactly as the verb on an element does.
  sigil::weave::merge(m_type, partial);
  // A colour written here is the ink, so a property the ink was read
  // from before no longer stands.
  if (partial.color) m_inkVar.reset();
  return *this;
}

Rule& Rule::block(sigil::weave::Block partial) {
  sigil::weave::merge(m_block, partial);
  return *this;
}

Rule& Rule::ink(SkColor4f colour) {
  m_type.color = colour;
  m_inkVar.reset();
  return *this;
}

Rule& Rule::ink(VarRef reference) {
  m_inkVar = reference;
  m_type.color.reset();
  return *this;
}

Rule& Rule::var(std::string_view name, SkColor4f colour) {
  m_vars.set(compose::var(name), colour);
  return *this;
}

Rule& Rule::var(std::string_view name, Dimension length) {
  m_vars.set(compose::var(name), length);
  return *this;
}

Rule rule(std::string_view cssText) { return Rule(selector(cssText)); }

Rule rule(ElementSelector subject) { return Rule(std::move(subject)); }

// ---------------------------------------------------------------------------
// A sheet

StyleSheet::Statement::Statement(Rule one) { m_rules.push_back(std::move(one)); }

StyleSheet::Statement::Statement(const StyleSheet& included)
    : m_rules(included.rules()) {}

StyleSheet::StyleSheet(std::initializer_list<Statement> statements) {
  std::vector<Rule> flat;
  size_t total = 0;
  for (const Statement& statement : statements) total += statement.rules().size();
  if (total == 0) return;
  flat.reserve(total);
  for (const Statement& statement : statements)
    flat.insert(flat.end(), statement.rules().begin(), statement.rules().end());
  m_rules = std::make_shared<const std::vector<Rule>>(std::move(flat));
}

const std::vector<Rule>& StyleSheet::rules() const {
  // A sheet stating nothing holds no storage at all, so the empty
  // answer is one every empty sheet shares.
  static const std::vector<Rule>* const none = new std::vector<Rule>;
  return m_rules ? *m_rules : *none;
}

bool StyleSheet::operator==(const StyleSheet& other) const {
  // One value applied at ten subtrees is one pointer, which is the
  // comparison a reconcile makes over and over.
  if (m_rules == other.m_rules) return true;
  return rules() == other.rules();
}

StyleSheet operator+(const StyleSheet& earlier, const StyleSheet& later) {
  if (earlier.empty()) return later;
  if (later.empty()) return earlier;
  return StyleSheet{earlier, later};
}

}  // namespace sigil::compose
