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

}  // namespace sigil::weave
