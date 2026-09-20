#pragma once

/** @file
 * @ingroup weave-document
 *
 * WHERE A WORD MAY BREAK, asked of something outside the engine: the
 * analysis knows every opportunity BETWEEN words and none inside one,
 * because where a word may be split is a fact about a language rather
 * than about Unicode. A Hyphenator answers that one question; the engine
 * inserts what it names and the breakers decide which a line takes.
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::weave {

/** WHICH OF A WORD'S BREAK POINTS BECOME OPPORTUNITIES AT ALL. Each is a
 * fact about the word rather than about the line it lands on, so they are
 * settled during segmentation and the whole text shares them.
 * @trap A limit that depends on the LINE — consecutive hyphens, the
 * ragged zone, the last word of a block — is a break decision and lives
 * on the block's own style.
 */
struct HyphenationLimits {
  int minimumWordLength = 5;     ///< shorter words are never broken
  int minimumLettersBefore = 2;  ///< kept on the line before the hyphen
  int minimumLettersAfter = 3;   ///< carried to the next line
  bool capitalizedWords = true;  ///< whether a capitalised word may break
  bool operator==(const HyphenationLimits&) const = default;
};

/** The one question: given a word and the language it is set in, where
 * may it be broken? Offsets are UTF-16 code units from the START OF THE
 * WORD, strictly inside it and ascending; the word arrives without its
 * trailing whitespace and without any typed soft hyphen. It is called
 * during analysis, on the font context's thread, once per word.
 * @trap It must be PURE — the same word and tag answering the same way
 * every time — or the shape cache and the breakers will disagree.
 */
class Hyphenator {
 public:
  virtual ~Hyphenator() = default;
  /** Appends this word's break offsets to `out`, ascending. */
  virtual void breakPoints(std::u16string_view word,
                           std::string_view languageTag,
                           std::vector<uint32_t>& out) const = 0;
};

/** WHICH CHARACTERS MAY NOT STAND AT A LINE'S EDGE — kinsoku shori, and
 * the same idea wherever else a script has one. Both members are plain
 * UTF-16 strings, one character per prohibition. A break that would put a
 * prohibited character at an edge is simply not a break: the boundary is
 * dropped during segmentation, which is the push-out a reader expects,
 * and no breaker has to know the rule.
 */
struct KinsokuTable {
  std::u16string notLineStart;
  std::u16string notLineEnd;
  [[nodiscard]] bool empty() const {
    return notLineStart.empty() && notLineEnd.empty();
  }
  bool operator==(const KinsokuTable&) const = default;
};

/** HOW FAR A CHARACTER MAY HANG PAST THE MEASURE — optical margin
 * alignment, and in a column the same rule under the name burasagari.
 * Each entry is a fraction of that character's OWN ADVANCE, so the rule
 * scales with the type and needs no per-size table.
 */
struct HangingEdge {
  char16_t character = 0;
  float atStart = 0;  ///< fraction hanging back past the line's start
  float atEnd = 0;    ///< fraction hanging past the line's end
  bool operator==(const HangingEdge&) const = default;
};

/** The hanging fractions, looked up by character. A linear scan: a table
 *  is a handful of punctuation marks and a scan of a handful beats a hash
 *  of one. */
struct HangingTable {
  std::vector<HangingEdge> entries;
  [[nodiscard]] const HangingEdge* find(char16_t character) const {
    for (const HangingEdge& entry : entries)
      if (entry.character == character) return &entry;
    return nullptr;
  }
  [[nodiscard]] bool empty() const { return entries.empty(); }
  bool operator==(const HangingTable&) const = default;
};

}  // namespace sigil::weave
