/** @file
 * Selector terms, their combinators, and the match.
 */

#include <sigilworld/element/Selector.h>

#include <algorithm>
#include <utility>

namespace sigil::world {

namespace {
const std::string& emptyWord() {
  static const std::string empty;
  return empty;
}
}  // namespace

Selector Selector::leaf(Op op, std::string word) {
  Selector out;
  auto term = std::make_shared<Term>();
  term->op = op;
  term->word = std::move(word);
  out.m_term = std::move(term);
  return out;
}

Selector Selector::leaf(Op op, ::sigil::material::Material material) {
  Selector out;
  auto term = std::make_shared<Term>();
  term->op = op;
  term->material =
      std::make_shared<const ::sigil::material::Material>(std::move(material));
  out.m_term = std::move(term);
  return out;
}

Selector Selector::combine(Op op, std::vector<Selector> operands) {
  Selector out;
  auto term = std::make_shared<Term>();
  term->op = op;
  term->operands = std::move(operands);
  out.m_term = std::move(term);
  return out;
}

const std::string& Selector::word() const {
  return m_term ? m_term->word : emptyWord();
}

std::span<const Selector> Selector::operands() const {
  if (!m_term) return {};
  return m_term->operands;
}

bool Selector::matches(const Subject& subject) const {
  if (!m_term) return true;
  switch (m_term->op) {
    case Op::All:
      return true;
    case Op::Tag:
      return std::find(subject.tags.begin(), subject.tags.end(),
                       m_term->word) != subject.tags.end();
    case Op::Key:
      return subject.key == m_term->word;
    case Op::Under:
      return std::find(subject.ancestorKeys.begin(), subject.ancestorKeys.end(),
                       std::string_view(m_term->word)) !=
             subject.ancestorKeys.end();
    case Op::Material:
      return subject.material && m_term->material &&
             *subject.material == *m_term->material;
    case Op::Or:
      for (const Selector& operand : m_term->operands)
        if (operand.matches(subject)) return true;
      return false;
    case Op::And:
      for (const Selector& operand : m_term->operands)
        if (!operand.matches(subject)) return false;
      return true;
    case Op::Not:
      return !m_term->operands.empty() &&
             !m_term->operands.front().matches(subject);
  }
  return true;
}

bool Selector::operator==(const Selector& other) const {
  if (m_term == other.m_term) return true;
  // One side empty and the other holding an All term describe the same
  // set, and a caller that built the term deliberately must not read as
  // a different selector from one that defaulted.
  const bool selfAll = !m_term || m_term->op == Op::All;
  const bool otherAll = !other.m_term || other.m_term->op == Op::All;
  if (selfAll || otherAll) return selfAll && otherAll;
  if (m_term->op != other.m_term->op) return false;
  if (m_term->word != other.m_term->word) return false;
  if ((m_term->material != nullptr) != (other.m_term->material != nullptr))
    return false;
  if (m_term->material && !(*m_term->material == *other.m_term->material))
    return false;
  return m_term->operands == other.m_term->operands;
}

Selector operator|(Selector a, Selector b) {
  return Selector::combine(Selector::Op::Or, {std::move(a), std::move(b)});
}

Selector operator&(Selector a, Selector b) {
  return Selector::combine(Selector::Op::And, {std::move(a), std::move(b)});
}

Selector operator!(Selector a) {
  return Selector::combine(Selector::Op::Not, {std::move(a)});
}

namespace sel {

Selector tag(std::string word) {
  return Selector::leaf(Selector::Op::Tag, std::move(word));
}
Selector key(std::string k) {
  return Selector::leaf(Selector::Op::Key, std::move(k));
}
Selector under(std::string k) {
  return Selector::leaf(Selector::Op::Under, std::move(k));
}
Selector material(::sigil::material::Material m) {
  return Selector::leaf(Selector::Op::Material, std::move(m));
}

}  // namespace sel

}  // namespace sigil::world
