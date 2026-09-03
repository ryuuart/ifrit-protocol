#pragma once

/** @file
 * SigilCompose typography — the ADDRESSING: `Selector`, the comparable
 * value that says which glyphs a track deviates, which characters a span
 * restyle covers and which units stand beside a passage, and the `sel::`
 * vocabulary that builds one. Every member is defined here: a selector is
 * a value, and the kernel compares one wherever a description carries it.
 */

#include <sigilcompose/typography/Units.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::compose {

/** WHICH GLYPHS a track addresses, as a comparable value.
 *
 *  Built from `sel::` (see below), combined with `|` (union), `&`
 *  (intersection) and `!` (complement). A default-constructed selector
 *  addresses EVERYTHING, which is what a track that names none gets.
 *
 *  Resolution happens once per (content, layout, selector) and is cached
 *  on the element, so a regular expression over a paragraph is matched
 *  when the text changes or reflows rather than once per frame. A pattern
 *  that does not compile selects nothing and warns once. */
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

  /** The forms a selector can take. Public because the resolver is a free
   *  function over a paragraph rather than a member. */
  enum class Kind : uint8_t {
    All,
    Word,
    Words,
    Line,
    Sentence,
    Range,
    Regex,
    Text,
    Style,
    Each,
    InFrame,
    Union,
    Intersect,
    Complement,
  };
  /** Everything a selector of any kind needs, in one shape. The fields
   *  a given Kind ignores stay at their defaults — one flat state keeps
   *  selectors cheap to copy and comparable by value, which is what
   *  lets a track's selector take part in prop equality. */
  struct State {
    Kind kind = Kind::All;
    uint32_t lo = 0, hi = 0;  ///< Word/Words/Line/Sentence/Range bounds
    /** Regex/Text needle, the style NAME an `sel::style` addresses, or the
     *  frame KEY an `sel::inFrame` names — one slot, because no selector
     *  carries two of them and a second string would ride on every selector
     *  in the tree to serve one form. */
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

/** The i-th word of the paragraph (SigilWeave's line-break units). */
[[nodiscard]] inline Selector word(uint32_t index) {
  return Selector::of({.kind = Selector::Kind::Word, .lo = index, .hi = index + 1});
}
/** Words `[lo, hi)`. */
[[nodiscard]] inline Selector words(uint32_t lo, uint32_t hi) {
  return Selector::of({.kind = Selector::Kind::Words, .lo = lo, .hi = hi});
}
/** The i-th flow line OF THE STORY — re-resolved when the text reflows,
 *  so a narrower box moves the selection with the break, and numbered from
 *  the story's first line however many frames it runs through. On a text
 *  that is not a frame of a chain — the ordinary case — the story is that
 *  one leaf and the number is the leaf's own. Compose it with
 *  `sel::inFrame` to address a line within one frame. */
[[nodiscard]] inline Selector line(uint32_t index) {
  return Selector::of({.kind = Selector::Kind::Line, .lo = index, .hi = index + 1});
}
/** Lines `[lo, hi)` of the story. */
[[nodiscard]] inline Selector lines(uint32_t lo, uint32_t hi) {
  return Selector::of({.kind = Selector::Kind::Line, .lo = lo, .hi = hi});
}
/** The i-th sentence (ICU sentence segmentation). */
[[nodiscard]] inline Selector sentence(uint32_t index) {
  return Selector::of(
      {.kind = Selector::Kind::Sentence, .lo = index, .hi = index + 1});
}
/** Every glyph whose cluster falls inside a UTF-16 range of the text. */
[[nodiscard]] inline Selector range(sigil::weave::CharRange chars) {
  return Selector::of(
      {.kind = Selector::Kind::Range, .lo = chars.start, .hi = chars.end});
}
/** Every match of an ICU regular expression (the pattern is UTF-8). A
 *  pattern that does not compile selects NOTHING and warns once. */
[[nodiscard]] inline Selector regex(std::u8string_view utf8Pattern) {
  return Selector::of(
      {.kind = Selector::Kind::Regex, .pattern = std::u8string(utf8Pattern)});
}
/** Every occurrence of a literal substring. */
[[nodiscard]] inline Selector text(std::u8string_view utf8Substring) {
  return Selector::of(
      {.kind = Selector::Kind::Text, .pattern = std::u8string(utf8Substring)});
}
/** EVERY RUN DRESSED UNDER THIS NAME — the runs a `rich()` value added with
 *  `add(utf8, styleName)`, addressed by the name rather than by the words
 *  they happen to contain.
 *
 *  This is the treatment as a handle: a glossary set in one registered
 *  style stays addressable when the copy changes, where naming the literal
 *  text means editing the selector every time an author edits a sentence.
 *
 *  It addresses the run's TEXT, so it survives everything that changes what
 *  that text looks like: re-registering the name against a different
 *  `weave::StyleSet` entry, or a `spanPaint`/`spanStyle` cutting across it,
 *  leaves the same runs selected.
 *
 *  ONLY A NAMED `rich()` RUN CARRIES A NAME. Plain `text(utf8, style)`, a
 *  `rich()` run given a style directly, and the `shared_ptr<Paragraph>`
 *  overload have none, so this addresses nothing there — as does a name no
 *  run was written with. Either way it selects nothing and warns once per
 *  name. */
[[nodiscard]] inline Selector style(std::string_view name) {
  // A style name is ASCII-or-whatever the author typed, and the needle slot
  // holds UTF-8 bytes; both resolvers compare it against the same bytes a
  // run's name was written with, so no transcoding is involved either way.
  return Selector::of({.kind = Selector::Kind::Style,
                       .pattern = std::u8string(
                           (const char8_t*)name.data(), name.size())});
}
/** Every unit of `granularity`, ready to be sliced with `.take()` /
 *  `.drop()`. Unsliced it is the same as selecting everything. */
[[nodiscard]] inline Selector each(Unit granularity) {
  return Selector::of({.kind = Selector::Kind::Each, .each = granularity});
}
/** EVERYTHING THE NAMED FRAME HOLDS — the frame-local address, since every
 *  other form here numbers the story.
 *
 *      sel::inFrame("b") & sel::line(0)   // no line: line 0 is in frame a
 *      sel::inFrame("b")                  // the text frame b actually got
 *
 *  Resolved on the leaf being addressed and nowhere else: it selects
 *  everything on the frame whose `key` it names and nothing anywhere else,
 *  so it composes with the story-wide forms as a plain intersection. A key
 *  no frame carries selects nothing and warns once — a frame-local address
 *  that quietly became story-wide is the silent no-op this vocabulary
 *  refuses. */
[[nodiscard]] inline Selector inFrame(std::string_view key) {
  // The key rides the needle slot the same way a style name does, and for
  // the same reason: it is compared against the bytes the frame's key() was
  // written with, so no transcoding is involved either way.
  return Selector::of({.kind = Selector::Kind::InFrame,
                       .pattern = std::u8string((const char8_t*)key.data(),
                                                key.size())});
}

}  // namespace sel

}  // namespace sigil::compose
