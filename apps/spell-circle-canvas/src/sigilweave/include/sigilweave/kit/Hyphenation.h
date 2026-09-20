#pragma once

/** @file
 * @ingroup weave-kit
 *
 * WHERE WORDS MAY BREAK, as DATA — a shelf of tables rather than a rule
 * the engine holds. PatternHyphenator answers the engine's one question
 * from Liang's pattern method, and the tables are the caller's to
 * choose: `englishHyphenationPatterns()` is the one set this kit
 * carries, and a loaded table is a peer of it rather than a fallback
 * behind it. A table proposes positions; it never respells a word.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sigilweave/paragraph/Hyphenation.h"

namespace sigil::weave::kit {

/** Liang's pattern method, over a table loaded once from the standard
 * pattern-file text: whitespace-separated patterns of letters with digits
 * between them and a full stop for a word boundary, a percent sign
 * opening a comment, and optionally a line reading `exceptions` with
 * hyphenated spellings after it. Anything unparsable is skipped.
 * @silent the paragraph's language tag does not start with this table's
 * language, or the word carries a digit, an apostrophe or a hyphen.
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

/** The one pattern set this kit carries — the text a PatternHyphenator
 * loads — which states its terms where it is defined: Liang's English
 * (US) patterns, in the form TeX distributes them. What is here is a
 * SUBSET, so it proposes fewer break points than the whole table and
 * never a different one; a document that needs the whole table loads the
 * whole table. */
[[nodiscard]] std::string_view englishHyphenationPatterns();

}  // namespace sigil::weave::kit
