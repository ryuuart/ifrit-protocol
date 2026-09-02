#pragma once

/** @file
 * @ingroup document
 *
 * WHERE A WORD MAY BREAK, asked of something outside the engine. The
 * analysis knows every break opportunity BETWEEN words — that is UAX #14 —
 * and none at all inside one, because where a word may be split is a fact
 * about a language rather than about Unicode. A Hyphenator answers that
 * one question and nothing else; the engine inserts the opportunities it
 * names and the breakers then decide, under the limits a block states,
 * which of them a line actually takes.
 *
 * The kit ships Liang pattern sets (kit/Hyphenation.h) and a caller's own
 * implementation is a peer of them: a dictionary, a server, a table of
 * exceptions for one document.
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::weave {

/**
 * WHICH OF A WORD'S BREAK POINTS BECOME OPPORTUNITIES AT ALL.
 *
 * Each of these is a fact about the word rather than about the line it
 * lands on, so they are settled during segmentation and the whole text
 * shares them: the word list either carries an opportunity or it does not,
 * and no block can conjure one the analysis did not open. The limits that
 * depend on the LINE — how many hyphens in a row, how ragged is ragged
 * enough, may the last word of a block break — are break decisions and
 * live on the block's own style.
 */
struct HyphenationLimits {
  int minimumWordLength = 5;   ///< shorter words are never broken
  int minimumLettersBefore = 2;  ///< kept on the line before the hyphen
  int minimumLettersAfter = 3;   ///< carried to the next line
  bool capitalizedWords = true;  ///< whether a capitalised word may break
  bool operator==(const HyphenationLimits&) const = default;
};

/**
 * The one question: given a word and the language it is set in, where may
 * it be broken?
 *
 * Offsets are UTF-16 code units from the START OF THE WORD, strictly
 * inside it — 0 and the length are not break points — and ascending. The
 * word arrives without its trailing whitespace and without any soft hyphen
 * the author typed, which is already an opportunity in its own right.
 *
 * `languageTag` is the shaping style's own tag (BCP 47, possibly empty).
 * An implementation that does not know it should answer nothing rather
 * than guess: an unbroken word is a ragged line, and a word broken by the
 * wrong language's rules is a misspelling.
 *
 * Called during analysis, which runs on the FontContext's thread, once per
 * word per analysis. It must be cheap and it must be pure — the same word
 * and tag answer the same way every time, or the shape cache and the
 * breakers will disagree about the same text.
 */
class Hyphenator {
 public:
  virtual ~Hyphenator() = default;
  /** Appends this word's break offsets to `out`, ascending. */
  virtual void breakPoints(std::u16string_view word,
                           std::string_view languageTag,
                           std::vector<uint32_t>& out) const = 0;
};

/**
 * WHICH CHARACTERS MAY NOT STAND AT A LINE'S EDGE — kinsoku shori, the
 * Japanese line-breaking prohibitions, and the same idea wherever else a
 * script has one.
 *
 * `notLineStart` holds the characters that may not OPEN a line: closing
 * brackets, the small kana, the sound marks, a full stop or a comma. A
 * break that would put one there is simply not a break: the boundary is
 * dropped during segmentation, so the character before it comes down to
 * the next line with it and no breaker has to know the rule. That is
 * "push-out", the resolution a reader expects.
 *
 * `notLineEnd` holds the characters that may not CLOSE one — the opening
 * brackets — and works the same way from the other side.
 *
 * Both are plain UTF-16 strings, one character per prohibition, because a
 * prohibition set is a fact about a language's punctuation and a caller's
 * own set is a peer of the ones the kit ships.
 */
struct KinsokuTable {
  std::u16string notLineStart;
  std::u16string notLineEnd;
  [[nodiscard]] bool empty() const {
    return notLineStart.empty() && notLineEnd.empty();
  }
  bool operator==(const KinsokuTable&) const = default;
};

/**
 * HOW FAR A CHARACTER MAY HANG PAST THE MEASURE — optical margin
 * alignment, and in a column the same rule under the name burasagari.
 *
 * A line that begins with an opening quote or ends in a comma reads as
 * indented and as short, because the eye squares a margin on the mass of
 * the type rather than on its advances. Letting those characters hang
 * OUTSIDE the measure squares it again. Each entry is a fraction of that
 * character's own advance, so the rule scales with the type and needs no
 * per-size table.
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
