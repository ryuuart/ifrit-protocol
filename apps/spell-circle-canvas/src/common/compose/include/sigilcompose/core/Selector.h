#pragma once

/** @file
 * @ingroup compose-core
 *
 * WHICH ELEMENTS A RULE SPEAKS ABOUT: CSS's selector grammar over the
 * element tree, as a comparable value, and the weight CSS gives it.
 * A bare word is a ROLE, `.name` is a class, and `*` is any element;
 * there are no ids and no attributes, so a weight is a pair rather
 * than a triple.
 */

#include <compare>
#include <memory>
#include <string_view>
#include <vector>

namespace sigil::compose {

namespace detail {
struct SelectorBody;
struct SelectorAccess;
}  // namespace detail

/** HOW HEAVILY A SELECTOR WEIGHS, as CSS counts it where nothing has an
 *  id: the classes and pseudo-classes it names, then the roles. The
 *  pair compares left to right, so one class outweighs any number of
 *  roles, and combinators and `*` weigh nothing at all. */
struct Specificity {
  int classes = 0;
  int roles = 0;

  friend bool operator==(const Specificity&, const Specificity&) = default;
  friend auto operator<=>(const Specificity&, const Specificity&) = default;
};

/** WHICH ELEMENTS A RULE SPEAKS ABOUT, as one immutable value: either a
 *  LIST of alternatives (CSS's comma) or one COMPLEX selector — a chain
 *  of compounds joined by combinators whose last compound is the
 *  SUBJECT, the element the rule actually styles.
 *  Every method returns a NEW selector; copies are cheap and share
 *  their storage. A default-built one, and one a bad CSS text produced,
 *  match nothing, and nothing ABSORBS: compounding or qualifying it
 *  matches nothing too, so a misprint can never widen a rule.
 *  @trap `a.child(b)` reads "a then b", so the SUBJECT is `b`. */
class ElementSelector {
 public:
  ElementSelector() = default;

  /** This selector, then @p subject as its direct child — CSS `a > b`. */
  [[nodiscard]] ElementSelector child(ElementSelector subject) const;
  /** This selector, then @p subject anywhere under it — CSS `a b`. */
  [[nodiscard]] ElementSelector descendant(ElementSelector subject) const;
  /** This selector, then @p subject as the sibling immediately after
   *  it — CSS `a + b`. */
  [[nodiscard]] ElementSelector next(ElementSelector subject) const;
  /** This selector, then @p subject as any later sibling of it —
   *  CSS `a ~ b`. */
  [[nodiscard]] ElementSelector sibling(ElementSelector subject) const;

  /** The subject only where it is its parent's first child. */
  [[nodiscard]] ElementSelector firstChild() const;
  /** The subject only where it is its parent's last child. */
  [[nodiscard]] ElementSelector lastChild() const;
  /** The subject only where it is its parent's one and only child. */
  [[nodiscard]] ElementSelector onlyChild() const;
  /** The subject only at the positions `@p step * n + @p offset` counted
   *  from the first child, n running 0, 1, 2… — CSS `:nth-child(an+b)`,
   *  whose `odd` is `(2, 1)` and `even` is `(2, 0)`. */
  [[nodiscard]] ElementSelector nthChild(int step, int offset) const;
  /** The same count taken over only the siblings matching @p of —
   *  CSS `:nth-child(an+b of S)`. */
  [[nodiscard]] ElementSelector nthChild(int step, int offset,
                                         ElementSelector of) const;
  /** `:nth-child`, counted from the last child instead. */
  [[nodiscard]] ElementSelector nthLastChild(int step, int offset) const;
  /** `:nth-child(an+b of S)`, counted from the last child instead. */
  [[nodiscard]] ElementSelector nthLastChild(int step, int offset,
                                             ElementSelector of) const;

  /** The subject only where it is the first sibling of its ROLE, which
   *  is what a type is here. An element with no role has no type, so no
   *  of-type pseudo-class matches it. */
  [[nodiscard]] ElementSelector firstOfType() const;
  /** The last sibling of the subject's role. */
  [[nodiscard]] ElementSelector lastOfType() const;
  /** The only sibling of the subject's role. */
  [[nodiscard]] ElementSelector onlyOfType() const;
  /** `:nth-child`'s count taken over the siblings of the subject's role
   *  alone — CSS `:nth-of-type(an+b)`, which takes no `of` filter. */
  [[nodiscard]] ElementSelector nthOfType(int step, int offset) const;
  /** `:nth-of-type`, counted from the last sibling instead. */
  [[nodiscard]] ElementSelector nthLastOfType(int step, int offset) const;

  /** The subject only where it has no children at all. */
  [[nodiscard]] ElementSelector empty() const;
  /** The subject only where it is the root of the tree its sheet sees:
   *  the node that applied the sheet, or the tree's own root where the
   *  sheet stands on it. */
  [[nodiscard]] ElementSelector root() const;

  /** CSS's weight for this selector: the heaviest alternative where it
   *  is a list, and the sum over every compound where it is a complex.
   *  A matcher weighs the alternative that actually matched. */
  [[nodiscard]] Specificity specificity() const;
  /** Whether this selector can never match: default-built, or built
   *  from a CSS text that did not parse. */
  [[nodiscard]] bool matchesNothing() const;

  [[nodiscard]] bool operator==(const ElementSelector& other) const;

 private:
  friend struct detail::SelectorAccess;
  std::shared_ptr<const detail::SelectorBody> m_body;
};

/** WHAT A `:has()` LOOKS FOR, read from the element it stands on — CSS's
 *  relative selector: a chain whose first compound is reached from that
 *  element by a RELATION, anywhere under it (a plain selector converts
 *  to that), as a direct child, as the next sibling or as any later
 *  sibling. Only `select::has` takes one, so a relation cannot be used
 *  as a rule's selector, where it would speak about nothing.
 *  @trap `select::child(a).descendant(b)` is `> a b`: the relation opens
 *  the chain, and the methods extend it exactly as they extend an
 *  `ElementSelector`. */
class RelativeSelector {
 public:
  RelativeSelector() = default;
  /** @p descendant reached anywhere under the element — CSS `:has(b)`.
   *  Implicit, so a plain selector stands wherever a relative one is
   *  asked for. */
  RelativeSelector(ElementSelector descendant);
  /** This chain, then @p subject as its direct child. */
  [[nodiscard]] RelativeSelector child(ElementSelector subject) const;
  /** This chain, then @p subject anywhere under it. */
  [[nodiscard]] RelativeSelector descendant(ElementSelector subject) const;
  /** This chain, then @p subject as the sibling immediately after it. */
  [[nodiscard]] RelativeSelector next(ElementSelector subject) const;
  /** This chain, then @p subject as any later sibling of it. */
  [[nodiscard]] RelativeSelector sibling(ElementSelector subject) const;
  /** Whether this can never be reached: default-built, or built from a
   *  selector that matches nothing. */
  [[nodiscard]] bool matchesNothing() const;

  [[nodiscard]] bool operator==(const RelativeSelector& other) const;

 private:
  friend struct detail::SelectorAccess;
  ElementSelector m_chain;
};

/** THE CSS FRONT DOOR, parsed once into the value above:
 *  `".card > .title:first-child"`, `".row:nth-child(odd)"`,
 *  `":is(.a, .b) .c"`, `":not(.x)"`, `".card:has(> .badge)"`, `"*"`.
 *  A text this library does not read warns once and matches nothing —
 *  including an `an+b` number too large to hold and a nesting too deep
 *  to read, which are REFUSED rather than clamped. */
[[nodiscard]] ElementSelector selector(std::string_view cssText);

/** THE TYPED FRONT DOOR: the same selectors built name by name, for
 *  the caller who would rather not write a string. `|` is a selector
 *  list, `&` a compound on one element, `!` a negation. */
namespace select {

/** Elements carrying @p name among their style classes — CSS `.name`. */
[[nodiscard]] ElementSelector styleClass(std::string_view name);
/** Elements whose role is @p name — CSS's type selector, a bare word. */
[[nodiscard]] ElementSelector role(std::string_view name);
/** Any element at all — CSS `*`, which weighs nothing. */
[[nodiscard]] ElementSelector any();
/** Elements matching any of @p alternatives, weighing as the heaviest
 *  of them — CSS `:is(...)`. */
[[nodiscard]] ElementSelector is(ElementSelector alternatives);
/** Elements matching any of @p alternatives, weighing NOTHING at all —
 *  CSS `:where(...)`. */
[[nodiscard]] ElementSelector where(ElementSelector alternatives);
/** Elements matching none of @p alternatives, weighing as the heaviest
 *  of them — CSS `:not(...)`, which `!` also spells. */
[[nodiscard]] ElementSelector notAnyOf(ElementSelector alternatives);
/** Elements from which some of @p relatives can be reached — CSS
 *  `:has(...)`, weighing as the heaviest of them. A plain selector is
 *  reached anywhere under the element; `child`, `next` and `sibling`
 *  below name the other three relations. `has(a | b)` asks for either,
 *  `has(a) & has(b)` for both.
 *  @trap A `:has()` inside another matches nothing, as in CSS. */
[[nodiscard]] ElementSelector has(RelativeSelector relatives);
/** @p subject as a direct child of the element a `:has()` stands on —
 *  CSS `:has(> b)`. */
[[nodiscard]] RelativeSelector child(ElementSelector subject);
/** @p subject as the sibling immediately after that element — CSS
 *  `:has(+ b)`. */
[[nodiscard]] RelativeSelector next(ElementSelector subject);
/** @p subject as any later sibling of that element — CSS `:has(~ b)`. */
[[nodiscard]] RelativeSelector sibling(ElementSelector subject);

}  // namespace select

/** A SELECTOR LIST: either side matching is a match — CSS's comma.
 *  A side that matches nothing leaves the other side standing, which
 *  is the one place nothing is the identity rather than the zero. */
[[nodiscard]] ElementSelector operator|(const ElementSelector& left,
                                        const ElementSelector& right);
/** A COMPOUND: both sides matching the SAME element — CSS `.card.wide`.
 *  The right side is folded into the left's subject compound, so a
 *  right side that is itself a chain matches nothing and says so, and
 *  either side matching nothing leaves the compound matching nothing. */
[[nodiscard]] ElementSelector operator&(const ElementSelector& left,
                                        const ElementSelector& right);
/** A LIST OF RELATIVE SELECTORS, either one reaching being a match —
 *  the comma inside CSS's `:has(+ a, ~ b)`. A plain selector on either
 *  side reaches anywhere under the element. */
[[nodiscard]] RelativeSelector operator|(const RelativeSelector& left,
                                         const RelativeSelector& right);
/** A NEGATION: `select::notAnyOf` under CSS's own spelling. */
[[nodiscard]] ElementSelector operator!(const ElementSelector& inner);

}  // namespace sigil::compose
