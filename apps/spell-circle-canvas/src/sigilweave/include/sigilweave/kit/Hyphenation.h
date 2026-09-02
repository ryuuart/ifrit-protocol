#pragma once

/** @file
 * WHERE WORDS MAY BREAK, as DATA — a shelf of tables rather than a rule
 * the engine holds.
 *
 * The engine asks one question (paragraph/Hyphenation.h) and decides
 * nothing about the answer, because where an English word may be broken is
 * not a property of text layout. PatternHyphenator answers it from Liang's
 * pattern method, the same tables TeX and every typesetter after it use: a
 * word is padded, every substring is looked up, the odd values it collects
 * are break points, and an explicit exception spelling overrides the lot.
 *
 * The patterns are the caller's to choose. `patterns::english()` is the
 * one set this kit carries; anything else is loaded from a file in the
 * same format and is a peer of it. Each set records its licence beside it,
 * because pattern tables are somebody's work and travel under terms.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sigilweave/paragraph/Hyphenation.h"

namespace sigil::weave::kit {

/**
 * Liang's pattern method, over a table loaded once.
 *
 * `load` takes the standard pattern-file text: whitespace-separated
 * patterns, each a lowercase letter run with digits between the letters and
 * `.` standing for a word boundary (`hy3ph`, `.mis1`), optionally followed
 * by a line reading `exceptions` and then hyphenated spellings
 * (`ta-ble`, `present`). Later entries win; anything unparsable is skipped,
 * so a truncated file costs break points and never correctness.
 *
 * Matching is over lowercased text and answers only for words made of
 * letters — a word carrying a digit, an apostrophe or a hyphen is left
 * whole, because a pattern table has nothing to say about it. The
 * `languageTag` a paragraph is set in must start with `language()` or the
 * table declines to answer at all: a word broken by the wrong language's
 * rules is a misspelling, and no answer is a ragged line.
 */
class PatternHyphenator final : public Hyphenator {
 public:
  /** Builds an empty table that answers nothing. */
  PatternHyphenator() = default;
  /** Builds a table for `languagePrefix` (BCP 47, e.g. "en") from
   * `patternFile`. */
  PatternHyphenator(std::string languagePrefix, std::string_view patternFile);
  ~PatternHyphenator() override;

  /** Replaces the table. */
  void load(std::string languagePrefix, std::string_view patternFile);
  /** The language this table answers for, as a BCP 47 prefix. */
  [[nodiscard]] const std::string& language() const { return m_language; }
  /** How many patterns the table holds. */
  [[nodiscard]] size_t patternCount() const;

  void breakPoints(std::u16string_view word, std::string_view languageTag,
                   std::vector<uint32_t>& out) const override;

 private:
  struct Table;
  std::string m_language;
  std::unique_ptr<Table> m_table;
};

/** The pattern sets this kit carries. Each is the text a
 * PatternHyphenator loads, and each states its terms where it is defined. */
namespace patterns {

/** Liang's English (US) patterns, in the form TeX's `hyphen.tex`
 * distributes them.
 *
 * Terms: Donald Knuth's original file may be copied and redistributed
 * freely so long as it is unmodified; a modified table must not carry the
 * same name. What is here is a SUBSET, so it carries neither the name nor
 * the claim: a subset of a Liang table proposes fewer break points than
 * the whole one and never a different one where an inhibiting pattern is
 * present, which is why the exceptions below carry the words whose
 * inhibitions matter most. A document that needs the whole table loads the
 * whole table.
 */
[[nodiscard]] std::string_view english();

}  // namespace patterns

}  // namespace sigil::weave::kit
