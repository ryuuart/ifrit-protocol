#pragma once

/** @file
 * What an ElementSelector is made of, shared by the value's own
 * translation unit and the CSS parser that builds one.
 *
 * A COMPOUND is the simple selectors that must all hold of ONE element.
 * A COMPLEX is compounds from the outermost ancestor to the SUBJECT,
 * each carrying the combinator that reaches it from the one before; the
 * first step's combinator is read only in a RELATIVE selector — an
 * argument of `:has()` — where it is the relation from the element the
 * `:has()` stands on to the first compound; everywhere else it is the
 * descendant one, and a selector whose first step says otherwise matches
 * nothing. A BODY is either a list of alternatives or one complex, never
 * both.
 */

#include <sigilcompose/core/Selector.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::detail {

enum class SimpleKind : uint8_t {
  Universal,
  StyleClass,
  Role,
  FirstChild,
  LastChild,
  OnlyChild,
  NthChild,
  NthLastChild,
  FirstOfType,
  LastOfType,
  OnlyOfType,
  NthOfType,
  NthLastOfType,
  Empty,
  Root,
  Is,
  Where,
  Not,
  Has,
};

/** ONE TEST OF ONE ELEMENT. @p name carries the class or role; @p step
 *  and @p offset carry an+b; @p arguments carries the alternatives of
 *  `:is`, `:where` and `:not`, the relative selectors of `:has`, or the
 *  `of S` filter of an nth-child. */
struct Simple {
  SimpleKind kind = SimpleKind::Universal;
  std::string name;
  int step = 0;
  int offset = 0;
  std::vector<ElementSelector> arguments;

  bool operator==(const Simple&) const = default;
};

struct Compound {
  std::vector<Simple> simples;

  bool operator==(const Compound&) const = default;
};

enum class Combinator : uint8_t { Descendant, Child, Next, Sibling };

struct Step {
  Combinator combinator = Combinator::Descendant;
  Compound compound;

  bool operator==(const Step&) const = default;
};

struct SelectorBody {
  /** Non-empty exactly when this selector is a LIST. */
  std::vector<ElementSelector> alternatives;
  /** Non-empty exactly when this selector is a COMPLEX. */
  std::vector<Step> steps;

  bool operator==(const SelectorBody&) const = default;
};

/** The one door that builds a selector value and reads one apart. */
struct SelectorAccess {
  static ElementSelector make(SelectorBody body);
  static const SelectorBody* body(const ElementSelector& value) {
    return value.m_body.get();
  }
  /** The alternatives @p value stands for: its own where it is a
   *  list, and itself where it is one complex selector. */
  static std::vector<ElementSelector> alternatives(const ElementSelector& value);
  /** @p value as a single compound, wrapping a list in `:is(...)`, or
   *  nothing where it is a chain no one compound can hold. */
  static std::optional<Compound> asCompound(const ElementSelector& value);
  /** @p value's steps, wrapping a list in `:is(...)` first. */
  static std::vector<Step> asSteps(const ElementSelector& value);
  /** A compound holding exactly @p simples, with `*` dropped wherever
   *  anything else stands beside it. */
  static ElementSelector fromSimples(std::vector<Simple> simples);
  /** @p subject made RELATIVE: every alternative's first compound
   *  reached from a `:has()` anchor by @p relation. */
  static ElementSelector relative(Combinator relation,
                                  const ElementSelector& subject);
};

/** Whether @p value holds a `:has()` anywhere in it, the arguments of
 *  `:is`, `:where`, `:not` and an `of` filter included. */
[[nodiscard]] bool containsHas(const ElementSelector& value);

/** Says once that a `:has()` was given another `:has()` inside it,
 *  which CSS refuses too, and that it therefore matches nothing. */
void warnHasInsideHas();

/** Says once that @p cssText is not a selector this library reads,
 *  naming @p reason, and that it therefore matches nothing. */
void warnBadSelector(std::string_view cssText, std::string_view reason);

/** Says once that `&` was given a right side that is itself a chain of
 *  elements, which no ONE element can carry, and that the compound
 *  therefore matches nothing. */
void warnChainInCompound();

}  // namespace sigil::compose::detail
