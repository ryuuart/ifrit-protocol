/** @file
 * The element selector value: how one is built from the typed front
 * door, how two are joined by the set algebra, and how heavily CSS
 * weighs the result.
 */

#include <include/core/SkTypes.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "SelectorInternal.h"

namespace sigil::compose {
namespace detail {

// ---------------------------------------------------------------------------
// Diagnostics

void warnBadSelector(std::string_view cssText, std::string_view reason) {
  static thread_local std::set<std::string> warned;
  if (!warned.insert(std::string(cssText)).second) return;
  SkDebugf(
      "[compose] selector(\"%.*s\") is not a selector this library reads: "
      "%.*s. It matches nothing. A bare word is a role, .name is a class, "
      "* is any element; the combinators are > + ~ and a space, and the "
      "pseudo-classes are the structural ones with :is, :where and :not. "
      "(warned once)\n",
      (int)cssText.size(), cssText.data(), (int)reason.size(), reason.data());
}

void warnChainInCompound() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] a & b asks ONE element to match both sides, and b is a "
      "chain of elements, which no one element can be. The compound "
      "matches nothing. Join chains with child/descendant/next/sibling "
      "instead. (warned once)\n");
}

// ---------------------------------------------------------------------------
// Building

ElementSelector SelectorAccess::make(SelectorBody body) {
  ElementSelector value;
  if (body.alternatives.empty() && body.steps.empty()) return value;
  value.m_body = std::make_shared<const SelectorBody>(std::move(body));
  return value;
}

ElementSelector SelectorAccess::fromSimples(std::vector<Simple> simples) {
  // `*` says nothing wherever anything else stands beside it, so a
  // compound carries it only when it is the whole of the compound.
  // That is what lets `:root` and `*:root` be the same value.
  if (simples.size() > 1)
    std::erase_if(simples, [](const Simple& simple) {
      return simple.kind == SimpleKind::Universal;
    });
  if (simples.empty()) simples.push_back(Simple{});
  SelectorBody body;
  body.steps.push_back(Step{Combinator::Descendant, Compound{std::move(simples)}});
  return make(std::move(body));
}

std::vector<ElementSelector> SelectorAccess::alternatives(
    const ElementSelector& value) {
  const SelectorBody* body = SelectorAccess::body(value);
  if (!body) return {};
  if (!body->alternatives.empty()) return body->alternatives;
  return {value};
}

namespace {

/** A compound holding one pseudo-class of @p kind. */
ElementSelector pseudo(SimpleKind kind) {
  Simple simple;
  simple.kind = kind;
  return SelectorAccess::fromSimples({std::move(simple)});
}

/** A compound holding one an+b pseudo-class, counted over every
 *  sibling. */
ElementSelector counted(SimpleKind kind, int step, int offset) {
  Simple simple;
  simple.kind = kind;
  simple.step = step;
  simple.offset = offset;
  return SelectorAccess::fromSimples({std::move(simple)});
}

/** The same count taken over only the siblings matching @p of. A
 *  filter that matches nothing counts nothing, rather than widening
 *  into the unfiltered count. */
ElementSelector counted(SimpleKind kind, int step, int offset,
                        const ElementSelector& of) {
  if (of.matchesNothing()) return {};
  Simple simple;
  simple.kind = kind;
  simple.step = step;
  simple.offset = offset;
  simple.arguments = SelectorAccess::alternatives(of);
  return SelectorAccess::fromSimples({std::move(simple)});
}

/** A compound holding one of the selector-list pseudo-classes. */
ElementSelector listPseudo(SimpleKind kind, const ElementSelector& arguments) {
  Simple simple;
  simple.kind = kind;
  simple.arguments = SelectorAccess::alternatives(arguments);
  if (simple.arguments.empty()) return {};
  return SelectorAccess::fromSimples({std::move(simple)});
}

/** @p value with one more simple selector folded into its SUBJECT
 *  compound — the last one, which is the element a rule styles. */
ElementSelector withSubjectSimple(const ElementSelector& value,
                                  const ElementSelector& addition) {
  const SelectorBody* body = SelectorAccess::body(value);
  // Nothing absorbs: qualifying a selector that matches nothing leaves
  // one that matches nothing, never `*` carrying the qualification.
  if (!body || addition.matchesNothing()) return {};
  const std::optional<Compound> extra = SelectorAccess::asCompound(addition);
  if (!extra) {
    warnChainInCompound();
    return {};
  }
  std::vector<Step> steps = SelectorAccess::asSteps(value);
  if (steps.empty()) return {};
  std::vector<Simple>& simples = steps.back().compound.simples;
  simples.insert(simples.end(), extra->simples.begin(), extra->simples.end());
  if (simples.size() > 1)
    std::erase_if(simples, [](const Simple& simple) {
      return simple.kind == SimpleKind::Universal;
    });
  // Erasing every `*` of a compound made only of them leaves the `*`
  // the compound still says, so `*` compounded with `*` is `*`.
  if (simples.empty()) simples.push_back(Simple{});
  SelectorBody joined;
  joined.steps = std::move(steps);
  return SelectorAccess::make(std::move(joined));
}

/** @p left, then @p subject reached by @p combinator. */
ElementSelector chained(const ElementSelector& left, Combinator combinator,
                        const ElementSelector& subject) {
  std::vector<Step> steps = SelectorAccess::asSteps(left);
  std::vector<Step> tail = SelectorAccess::asSteps(subject);
  if (steps.empty() || tail.empty()) return {};
  tail.front().combinator = combinator;
  steps.insert(steps.end(), tail.begin(), tail.end());
  SelectorBody body;
  body.steps = std::move(steps);
  return SelectorAccess::make(std::move(body));
}

}  // namespace

std::optional<Compound> SelectorAccess::asCompound(
    const ElementSelector& value) {
  const SelectorBody* body = SelectorAccess::body(value);
  if (!body) return std::nullopt;
  if (!body->alternatives.empty()) {
    // A list on one element is CSS's `:is(...)`, which is exactly a
    // compound of one simple selector. The wrapper is held by a named
    // value, because the body belongs to it and dies with it.
    const ElementSelector wrapper = listPseudo(SimpleKind::Is, value);
    const SelectorBody* wrapped = SelectorAccess::body(wrapper);
    if (!wrapped || wrapped->steps.size() != 1) return std::nullopt;
    return wrapped->steps.front().compound;
  }
  if (body->steps.size() != 1) return std::nullopt;
  return body->steps.front().compound;
}

std::vector<Step> SelectorAccess::asSteps(const ElementSelector& value) {
  const SelectorBody* body = SelectorAccess::body(value);
  if (!body) return {};
  if (body->alternatives.empty()) return body->steps;
  const std::optional<Compound> one = asCompound(value);
  if (!one) return {};
  return {Step{Combinator::Descendant, *one}};
}

// ---------------------------------------------------------------------------
// Weight

namespace {

Specificity weigh(const Compound& compound);

/** The heaviest of @p alternatives, which is what `:is` and `:not`
 *  weigh and what a list's own weight reports. */
Specificity heaviest(const std::vector<ElementSelector>& alternatives) {
  Specificity most;
  for (const ElementSelector& one : alternatives)
    most = std::max(most, one.specificity());
  return most;
}

Specificity weigh(const Simple& simple) {
  switch (simple.kind) {
    case SimpleKind::Universal:
      return {};
    case SimpleKind::Role:
      return {0, 1};
    case SimpleKind::Where:
      return {};
    case SimpleKind::Is:
    case SimpleKind::Not:
      return heaviest(simple.arguments);
    case SimpleKind::NthChild:
    case SimpleKind::NthLastChild: {
      // The filtered form adds its heaviest argument to the one class
      // the pseudo-class itself is worth.
      Specificity total = heaviest(simple.arguments);
      total.classes += 1;
      return total;
    }
    default:
      // Every remaining structural pseudo-class, and a style class,
      // weighs exactly one class.
      return {1, 0};
  }
}

Specificity weigh(const Compound& compound) {
  Specificity total;
  for (const Simple& simple : compound.simples) {
    const Specificity one = weigh(simple);
    total.classes += one.classes;
    total.roles += one.roles;
  }
  return total;
}

}  // namespace
}  // namespace detail

// ---------------------------------------------------------------------------
// The value

bool ElementSelector::operator==(const ElementSelector& other) const {
  if (m_body == other.m_body) return true;
  if (!m_body || !other.m_body) return false;
  return *m_body == *other.m_body;
}

bool ElementSelector::matchesNothing() const { return m_body == nullptr; }

Specificity ElementSelector::specificity() const {
  if (!m_body) return {};
  if (!m_body->alternatives.empty()) return detail::heaviest(m_body->alternatives);
  Specificity total;
  for (const detail::Step& step : m_body->steps) {
    const Specificity one = detail::weigh(step.compound);
    total.classes += one.classes;
    total.roles += one.roles;
  }
  return total;
}

ElementSelector ElementSelector::child(ElementSelector subject) const {
  return detail::chained(*this, detail::Combinator::Child, subject);
}
ElementSelector ElementSelector::descendant(ElementSelector subject) const {
  return detail::chained(*this, detail::Combinator::Descendant, subject);
}
ElementSelector ElementSelector::next(ElementSelector subject) const {
  return detail::chained(*this, detail::Combinator::Next, subject);
}
ElementSelector ElementSelector::sibling(ElementSelector subject) const {
  return detail::chained(*this, detail::Combinator::Sibling, subject);
}

ElementSelector ElementSelector::firstChild() const {
  return detail::withSubjectSimple(*this,
                                   detail::pseudo(detail::SimpleKind::FirstChild));
}
ElementSelector ElementSelector::lastChild() const {
  return detail::withSubjectSimple(*this,
                                   detail::pseudo(detail::SimpleKind::LastChild));
}
ElementSelector ElementSelector::onlyChild() const {
  return detail::withSubjectSimple(*this,
                                   detail::pseudo(detail::SimpleKind::OnlyChild));
}
ElementSelector ElementSelector::nthChild(int step, int offset) const {
  return detail::withSubjectSimple(
      *this, detail::counted(detail::SimpleKind::NthChild, step, offset));
}
ElementSelector ElementSelector::nthChild(int step, int offset,
                                          ElementSelector of) const {
  return detail::withSubjectSimple(
      *this, detail::counted(detail::SimpleKind::NthChild, step, offset, of));
}
ElementSelector ElementSelector::nthLastChild(int step, int offset) const {
  return detail::withSubjectSimple(
      *this, detail::counted(detail::SimpleKind::NthLastChild, step, offset));
}
ElementSelector ElementSelector::nthLastChild(int step, int offset,
                                              ElementSelector of) const {
  return detail::withSubjectSimple(
      *this,
      detail::counted(detail::SimpleKind::NthLastChild, step, offset, of));
}
ElementSelector ElementSelector::firstOfType() const {
  return detail::withSubjectSimple(
      *this, detail::pseudo(detail::SimpleKind::FirstOfType));
}
ElementSelector ElementSelector::lastOfType() const {
  return detail::withSubjectSimple(
      *this, detail::pseudo(detail::SimpleKind::LastOfType));
}
ElementSelector ElementSelector::onlyOfType() const {
  return detail::withSubjectSimple(
      *this, detail::pseudo(detail::SimpleKind::OnlyOfType));
}
ElementSelector ElementSelector::nthOfType(int step, int offset) const {
  return detail::withSubjectSimple(
      *this, detail::counted(detail::SimpleKind::NthOfType, step, offset));
}
ElementSelector ElementSelector::nthLastOfType(int step, int offset) const {
  return detail::withSubjectSimple(
      *this,
      detail::counted(detail::SimpleKind::NthLastOfType, step, offset));
}
ElementSelector ElementSelector::empty() const {
  return detail::withSubjectSimple(*this, detail::pseudo(detail::SimpleKind::Empty));
}
ElementSelector ElementSelector::root() const {
  return detail::withSubjectSimple(*this, detail::pseudo(detail::SimpleKind::Root));
}

// ---------------------------------------------------------------------------
// The typed front door and the set algebra

namespace select {

ElementSelector styleClass(std::string_view name) {
  if (name.empty()) return {};
  detail::Simple simple;
  simple.kind = detail::SimpleKind::StyleClass;
  simple.name = std::string(name);
  return detail::SelectorAccess::fromSimples({std::move(simple)});
}

ElementSelector role(std::string_view name) {
  if (name.empty()) return {};
  detail::Simple simple;
  simple.kind = detail::SimpleKind::Role;
  simple.name = std::string(name);
  return detail::SelectorAccess::fromSimples({std::move(simple)});
}

ElementSelector any() { return detail::SelectorAccess::fromSimples({}); }

ElementSelector is(ElementSelector alternatives) {
  return detail::listPseudo(detail::SimpleKind::Is, alternatives);
}
ElementSelector where(ElementSelector alternatives) {
  return detail::listPseudo(detail::SimpleKind::Where, alternatives);
}
ElementSelector notAnyOf(ElementSelector alternatives) {
  return detail::listPseudo(detail::SimpleKind::Not, alternatives);
}

}  // namespace select

ElementSelector operator|(const ElementSelector& left,
                          const ElementSelector& right) {
  // A union is the one place nothing is the IDENTITY: the alternatives
  // that do say something still say it.
  if (left.matchesNothing()) return right;
  if (right.matchesNothing()) return left;
  detail::SelectorBody body;
  // A list of lists is one flat list: the comma does not nest in CSS.
  for (const ElementSelector& source : {left, right})
    for (ElementSelector one : detail::SelectorAccess::alternatives(source))
      body.alternatives.push_back(std::move(one));
  return detail::SelectorAccess::make(std::move(body));
}

ElementSelector operator&(const ElementSelector& left,
                          const ElementSelector& right) {
  // A compound is the ZERO's home: an element asked to match a test
  // nothing passes matches nothing, so a misprint on either side
  // narrows the rule away rather than widening it to everything.
  if (left.matchesNothing() || right.matchesNothing()) return {};
  return detail::withSubjectSimple(left, right);
}

ElementSelector operator!(const ElementSelector& inner) {
  return select::notAnyOf(inner);
}

}  // namespace sigil::compose
