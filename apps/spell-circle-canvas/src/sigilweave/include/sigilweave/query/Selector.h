#pragma once

/** @file
 * @ingroup weave-query
 *
 * SELECTING TEXT AS A VALUE: `Selector`, which says which of a passage a
 * caller means, and the `selectors::` vocabulary that builds one. It is
 * a question written down and not yet asked, where `findAllOccurrences`
 * answers one now. RESOLVING one is the caller's: what a selection means
 * as GLYPHS depends on a layout, and this library owns no canonical one.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilweave/paragraph/Paragraph.h"
#include "sigilweave/paragraph/Unit.h"

namespace sigil::weave {

/** WHICH OF A PASSAGE A CALLER MEANS, as a comparable value: built from
 *  `selectors::` and combined with `|`, `&` and `!`. A default-constructed
 *  selector addresses EVERYTHING. It is cheap to copy and compares by
 *  state, so a resolution can be cached against the content, layout and
 *  selector it was resolved for. */
class Selector {
 public:
  Selector() = default;  ///< everything

  /** Within EACH unit of a `selectors::each` selector, keep @p n glyphs
   *  from wherever `drop` left off. The two on their own partition every
   *  unit exactly: no glyph is in both, none in neither. */
  [[nodiscard]] Selector take(int n) const;
  /** Within each unit, skip the first `n` glyphs and keep the rest. */
  [[nodiscard]] Selector drop(int n) const;

  [[nodiscard]] Selector operator|(const Selector& other) const;
  [[nodiscard]] Selector operator&(const Selector& other) const;
  [[nodiscard]] Selector operator!() const;
  bool operator==(const Selector& other) const;

  /** The forms a selector can take, public because resolving one is the
   *  caller's. Everything above `Named` addresses the text itself, and
   *  any resolver answers those the same way; `Named` and `Scope` address
   *  something the CALLER named, and this library ships no builder for
   *  either — `Selector::of` is how a caller spells its own form. */
  enum class Kind : uint8_t {
    All,
    Word,
    Line,
    Sentence,
    Range,
    Regex,
    Text,
    Each,
    Named,
    Scope,
    Union,
    Intersect,
    Complement,
  };
  /** Everything a selector of any kind needs, in one shape. The fields
   *  a given Kind ignores stay at their defaults — one flat state keeps
   *  selectors cheap to copy and comparable by value, which is what lets a
   *  selector take part in a larger value's equality. */
  struct State {
    Kind kind = Kind::All;
    uint32_t lo = 0, hi = 0;  ///< Word/Line/Sentence/Range bounds
    /** Regex/Text needle, or the NAME a `Named` or `Scope` form carries —
     *  one slot, because no selector carries two of them and a second
     *  string would ride on every selector to serve one form. */
    std::u8string pattern;
    Unit each = Unit::Glyph;  ///< Each granularity
    int take = -1;            ///< Each: glyphs kept per unit (-1 = all)
    int drop = 0;             ///< Each: glyphs skipped per unit
    std::vector<Selector> operands;
    bool operator==(const State&) const = default;
  };
  /** Null for a default-constructed (everything) selector. */
  [[nodiscard]] const State* state() const { return m_state.get(); }
  static Selector of(State s) {
    Selector out;
    out.m_state = std::make_shared<const State>(std::move(s));
    return out;
  }

 private:
  std::shared_ptr<const State> m_state;
};

inline Selector Selector::take(int n) const {
  State s = m_state ? *m_state : State{};
  s.take = n;
  return of(std::move(s));
}

inline Selector Selector::drop(int n) const {
  State s = m_state ? *m_state : State{};
  s.drop = n;
  return of(std::move(s));
}

inline Selector Selector::operator|(const Selector& other) const {
  State s;
  s.kind = Kind::Union;
  s.operands = {*this, other};
  return of(std::move(s));
}

inline Selector Selector::operator&(const Selector& other) const {
  State s;
  s.kind = Kind::Intersect;
  s.operands = {*this, other};
  return of(std::move(s));
}

inline Selector Selector::operator!() const {
  State s;
  s.kind = Kind::Complement;
  s.operands = {*this};
  return of(std::move(s));
}

/** Two selectors are equal when they are copies of one value, or when
 *  their whole states — kind, bounds, needle, slice and operands — are. A
 *  default-constructed selector (everything) equals only another one. */
inline bool Selector::operator==(const Selector& other) const {
  if (m_state == other.m_state) return true;
  if (!m_state || !other.m_state) return false;  // one is "everything"
  return *m_state == *other.m_state;
}

/** THE SELECTOR VOCABULARY. Absolute forms name a position in the text;
 *  `each` slices every unit of one granularity the same way. */
namespace selectors {

/** The i-th word — the line-break units the analysis produced. */
[[nodiscard]] inline Selector word(uint32_t index) {
  return Selector::of(
      {.kind = Selector::Kind::Word, .lo = index, .hi = index + 1});
}
/** Words `[lo, hi)` — the same kind `word` builds, since `word(i)` is
 *  `words(i, i + 1)` and a resolver answers one case rather than two that
 *  have to agree. */
[[nodiscard]] inline Selector words(uint32_t lo, uint32_t hi) {
  return Selector::of({.kind = Selector::Kind::Word, .lo = lo, .hi = hi});
}
/** The i-th FLOW LINE — a line as the breaker made it, so a narrower
 *  measure moves the selection with the break rather than leaving it on a
 *  number that no longer means anything. Numbered from the first line of
 *  the whole text, however many frames it runs through. */
[[nodiscard]] inline Selector line(uint32_t index) {
  return Selector::of(
      {.kind = Selector::Kind::Line, .lo = index, .hi = index + 1});
}
/** Everything whose cluster falls inside a UTF-16 range of the text. */
[[nodiscard]] inline Selector range(CharRange chars) {
  return Selector::of(
      {.kind = Selector::Kind::Range, .lo = chars.start, .hi = chars.end});
}
/** Every match of a regular expression (the pattern is UTF-8). A pattern
 *  that does not compile selects NOTHING — a resolver says so once and
 *  carries on, because a selection resolved every reflow would otherwise
 *  report the same mistake forever. */
[[nodiscard]] inline Selector regex(std::u8string_view utf8Pattern) {
  return Selector::of(
      {.kind = Selector::Kind::Regex, .pattern = std::u8string(utf8Pattern)});
}
/** Every occurrence of a literal substring. */
[[nodiscard]] inline Selector text(std::u8string_view utf8Substring) {
  return Selector::of(
      {.kind = Selector::Kind::Text, .pattern = std::u8string(utf8Substring)});
}
/** Every unit of `granularity`, ready to be sliced with `.take()` /
 *  `.drop()`. Unsliced it is the same as selecting everything. */
[[nodiscard]] inline Selector each(Unit granularity) {
  return Selector::of({.kind = Selector::Kind::Each, .each = granularity});
}

}  // namespace selectors

}  // namespace sigil::weave
