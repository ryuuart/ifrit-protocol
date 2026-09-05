#pragma once

/** @file
 * @ingroup query
 *
 * SELECTING TEXT AS A VALUE: `Selector`, which says which of a passage a
 * caller means, and the `sel::` vocabulary that builds one.
 *
 * The other half of this feature answers a question NOW — `findAllOccurrences`
 * hands back the ranges a needle matches in the paragraph it was given. A
 * selector is the same question written down and not yet asked: a small
 * comparable value that can ride in a larger one, be compared frame to
 * frame, and be resolved again after the text changed or the lines
 * re-broke. Every form here names a position in the text or a granularity
 * to slice; none of them holds a paragraph.
 *
 * RESOLVING one is the caller's, and deliberately so. What a selection
 * means as GLYPHS depends on a layout — which line a word landed on, which
 * cluster a mark belongs to — and this library hands its layout out rather
 * than owning a canonical one. So a selector is a value here and a set of
 * glyphs wherever the glyphs are.
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

/** WHICH OF A PASSAGE A CALLER MEANS, as a comparable value.
 *
 *  Built from `sel::` (see below), combined with `|` (union), `&`
 *  (intersection) and `!` (complement). A default-constructed selector
 *  addresses EVERYTHING, which is what a caller who names nothing gets.
 *
 *  It is cheap to copy and compares by state, so resolving one can be
 *  cached against the (content, layout, selector) it was resolved for: a
 *  regular expression over a paragraph is matched when the text changes or
 *  reflows rather than once per frame. */
class Selector {
 public:
  Selector() = default;  ///< everything

  /** Within EACH unit of an `sel::each` selector, keep `n` glyphs from
   *  wherever `drop()` left off. `take(n)` and `drop(n)` on their own
   *  partition every unit exactly: no glyph is in both, none is in
   *  neither. */
  [[nodiscard]] Selector take(int n) const;
  /** Within each unit, skip the first `n` glyphs and keep the rest. */
  [[nodiscard]] Selector drop(int n) const;

  [[nodiscard]] Selector operator|(const Selector& other) const;
  [[nodiscard]] Selector operator&(const Selector& other) const;
  [[nodiscard]] Selector operator!() const;
  bool operator==(const Selector& other) const;

  /** The forms a selector can take. Public because resolving one is the
   *  caller's: a resolver reads the state and answers for its own glyphs.
   *
   *  `Named` and `Scope` are the two a CALLER defines. Everything above
   *  them addresses the text itself — words, lines, sentences, characters,
   *  patterns — and any resolver over a paragraph answers them the same
   *  way. Those two address something the caller named: a set of spans
   *  registered under a name, and a whole passage identified by one. This
   *  library ships no builder for either, because what a name addresses is
   *  the caller's to say; `Selector::of` is how a caller spells its own
   *  form, and the combinators then treat it like any other. */
  enum class Kind : uint8_t {
    All,
    Word,
    Words,
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
    uint32_t lo = 0, hi = 0;  ///< Word/Words/Line/Sentence/Range bounds
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
namespace sel {

/** The i-th word — the line-break units the analysis produced. */
[[nodiscard]] inline Selector word(uint32_t index) {
  return Selector::of(
      {.kind = Selector::Kind::Word, .lo = index, .hi = index + 1});
}
/** Words `[lo, hi)`. */
[[nodiscard]] inline Selector words(uint32_t lo, uint32_t hi) {
  return Selector::of({.kind = Selector::Kind::Words, .lo = lo, .hi = hi});
}
/** The i-th FLOW LINE — a line as the breaker made it, so a narrower
 *  measure moves the selection with the break rather than leaving it on a
 *  number that no longer means anything. Numbered from the first line of
 *  the whole text, however many frames it runs through. */
[[nodiscard]] inline Selector line(uint32_t index) {
  return Selector::of(
      {.kind = Selector::Kind::Line, .lo = index, .hi = index + 1});
}
/** Flow lines `[lo, hi)`. */
[[nodiscard]] inline Selector lines(uint32_t lo, uint32_t hi) {
  return Selector::of({.kind = Selector::Kind::Line, .lo = lo, .hi = hi});
}
/** The i-th sentence, as the Unicode sentence segmentation finds them. */
[[nodiscard]] inline Selector sentence(uint32_t index) {
  return Selector::of(
      {.kind = Selector::Kind::Sentence, .lo = index, .hi = index + 1});
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

}  // namespace sel

}  // namespace sigil::weave
