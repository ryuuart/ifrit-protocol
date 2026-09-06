/** @file
 * THE GREEDY BREAKER: one block fitted word by word into the intervals the
 * geometry hands out, with the hyphenation limits and the last-line indent
 * it must respect.
 */

#include <hb.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkTextBlob.h>
#include <unicode/ubidi.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "Blobs.h"
#include "ParagraphLayoutInternal.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

namespace detail {

namespace {

// After this many consecutive intervals rejected a word outright, it is
// force-placed (overflowing) rather than skipping arbitrarily far down the
// geometry — matches browser overflow behavior for unbreakably-wide content.
constexpr int kMaxIntervalSkips = 12;

/** The interval a block's LAST line is placed in: the last-line indent adds
 *  to the near end, exactly as the first-line one does on the first. */
detail::FlatInterval withLastLineIndent(const detail::FlatInterval& flat,
                                        float indent) {
  if (indent == 0 || flat.interval.contour.valid()) return flat;
  detail::FlatInterval shortened = flat;
  shortened.interval.origin += SkVector{flat.interval.direction.x() * indent,
                                        flat.interval.direction.y() * indent};
  shortened.interval.length = std::max(0.0f, flat.interval.length - indent);
  return shortened;
}

/** Whether a hyphen break at `endWordIndex` is one the block's limits allow
 *  a line to take: the last word of a block, and a run of hyphenated lines
 *  longer than the block permits, are break decisions rather than facts
 *  about the word, so they are settled here and not in the analysis. */
bool hyphenAllowedHere(const detail::Block& block,
                       const std::vector<Word>& words, uint32_t endWordIndex,
                       int consecutiveHyphens) {
  const HyphenationOptions& hyphenation = block.options->hyphenation;
  if (!hyphenation.lastWordOfBlock && endWordIndex + 1 >= block.endWord)
    return false;
  return hyphenation.consecutiveLimit <= 0 ||
         consecutiveHyphens < hyphenation.consecutiveLimit;
}

/** Where the WHOLE word a break at `endWordIndex` cuts into begins: a
 *  hyphenated word is a run of pieces, and the piece boundaries before this
 *  one belong to the same word. */
uint32_t wholeWordStart(const std::vector<Word>& words, uint32_t endWordIndex) {
  uint32_t start = endWordIndex > 0 ? endWordIndex - 1 : 0;
  while (start > 0 && words[start - 1].hyphenBreak) --start;
  return start;
}

/** Whether the HYPHENATION ZONE leaves this break alone.
 *
 *  The zone is a band at the ragged edge: a line whose last WHOLE word ends
 *  inside it is already square enough for the eye, so breaking a word to
 *  reach further is a hyphen the page did not need. So the question is
 *  asked of the line WITHOUT the break — everything up to the start of the
 *  word the break cuts into — and the answer is a fact about that line, not
 *  about the demerits of the break.
 *
 *  It is a ragged-setting rule and nothing else. A justified line spends
 *  its slack on the gaps rather than showing it at the edge, so a zone
 *  there would only remove breaks the spacing was relying on. */
bool zoneAllowsHyphen(const detail::Block& block,
                      const std::vector<Word>& words, uint32_t lineStart,
                      uint32_t endWordIndex, float measure) {
  const float zone = block.options->hyphenation.zone;
  if (zone <= 0 || block.options->alignment == TextAlignment::kJustify)
    return true;
  const uint32_t whole = wholeWordStart(words, endWordIndex);
  if (whole <= lineStart) return true;  // the word is the whole line
  return measure - naturalWidth(words, lineStart, whole) > zone;
}

}  // namespace

/** Fills one block greedily from `firstInterval`, appending to `result`.
 *  Returns the index of the last interval it placed anything in, or
 *  SIZE_MAX when it placed nothing; sets `overflowWord` when the geometry
 *  ran out before the block's words did. */
size_t greedyBlock(FontContext& fontContext, Paragraph& paragraph,
                   detail::IntervalSequence& intervalSequence,
                   const detail::Block& block, size_t firstInterval,
                   ParagraphLayout& result, uint32_t& overflowWord) {
  using namespace detail;
  const std::vector<Word>& words = paragraph.words();
  const ParagraphLayoutOptions& options = *block.options;
  const float lastLineIndent = block.style.indent.lastLine;

  const bool spacedByTable = !block.mojikumiAfter.empty();
  size_t intervalIndex = firstInterval;
  const FlatInterval* flatInterval = intervalSequence.intervalAt(intervalIndex);
  uint32_t firstWordIndex = block.firstWord;
  uint32_t wordIndex = block.firstWord;
  float penPosition = 0;
  int skippedIntervalCount = 0;
  size_t lastIntervalUsed = SIZE_MAX;
  int consecutiveHyphens = 0;
  // Widest interval passed over during the current skip run — the fallback
  // landing spot if the geometry runs out while a wide word keeps skipping.
  size_t widestSkippedIntervalIndex = SIZE_MAX;
  float widestSkippedIntervalLength = -1;

  auto flushLine = [&](uint32_t endWordIndex, bool isLast) {
    if (flatInterval && firstWordIndex < endWordIndex) {
      const bool hyphenated =
          hyphenTakenAt(words, endWordIndex, isLast, options) &&
          hyphenAllowedHere(block, words, endWordIndex, consecutiveHyphens);
      const FlatInterval placed =
          isLast ? withLastLineIndent(*flatInterval, lastLineIndent)
                 : *flatInterval;
      placeWords(fontContext, paragraph, firstWordIndex, endWordIndex, placed,
                 options.alignment, isLast, hyphenated, options, result,
                 block.mojikumiAfter);
      consecutiveHyphens = hyphenated ? consecutiveHyphens + 1 : 0;
      lastIntervalUsed = intervalIndex;
    }
    firstWordIndex = endWordIndex;
    penPosition = 0;
  };

  while (wordIndex < block.endWord) {
    if (!flatInterval) {
      overflowWord = wordIndex;
      break;
    }
    // Shape just ahead of the greedy frontier so overflowing tails remain
    // completely untouched by HarfBuzz.
    paragraph.ensureShapedTo(fontContext, wordIndex + 1);
    const Word& word = words[wordIndex];
    const float glue =
        wordIndex > firstWordIndex
            ? glueAfter(words[wordIndex - 1], penPosition, options) +
                  (spacedByTable ? mojikumiAfter(block, wordIndex - 1) : 0.0f)
            : 0;
    // Soft-hyphen words reserve room for the hyphen so a break taken right
    // after them always fits.
    const float hyphenReserve =
        (options.hyphenation.enabled && word.hyphenBreak && word.hyphenGlyph)
            ? word.hyphenGlyph->advance
            : 0;
    // The block's last line is set in its own measure, so the fit that
    // decides whether the remaining words ARE the last line must ask about
    // that measure and not the one every other line gets.
    const bool couldBeLastLine = wordIndex + 1 >= block.endWord;
    const float measure =
        flatInterval->interval.length -
        (couldBeLastLine ? std::max(0.0f, lastLineIndent) : 0.0f);
    const bool fits = penPosition + glue + word.width + hyphenReserve <=
                      measure + kFitEpsilon;
    const bool intervalEmpty = (wordIndex == firstWordIndex);

    if (fits || (intervalEmpty && skippedIntervalCount >= kMaxIntervalSkips)) {
      penPosition += glue + word.width;
      wordIndex++;
      skippedIntervalCount = 0;
      widestSkippedIntervalIndex = SIZE_MAX;
      widestSkippedIntervalLength = -1;
      continue;
    }

    if (intervalEmpty) {
      if (flatInterval->interval.length > widestSkippedIntervalLength) {
        widestSkippedIntervalLength = flatInterval->interval.length;
        widestSkippedIntervalIndex = intervalIndex;
      }
      skippedIntervalCount++;
      flatInterval = intervalSequence.intervalAt(++intervalIndex);
      if ((!flatInterval || skippedIntervalCount >= kMaxIntervalSkips) &&
          widestSkippedIntervalIndex != SIZE_MAX) {
        // Geometry exhausted or skips spent: rather than dropping the rest
        // of the text (or jamming the word into whatever narrow interval
        // the skip run happened to stop on — visibly overflowing into an
        // exclusion shape), back up to the widest interval we passed and
        // force the word in there.
        intervalIndex = widestSkippedIntervalIndex;
        flatInterval = intervalSequence.intervalAt(intervalIndex);
        skippedIntervalCount = kMaxIntervalSkips;
        widestSkippedIntervalIndex = SIZE_MAX;
        widestSkippedIntervalLength = -1;
      }
      continue;
    }

    // A break the zone refuses hands the whole word to the next interval
    // instead of cutting it, and the line ends where that word began.
    uint32_t breakAt = wordIndex;
    if (hyphenTakenAt(words, breakAt, false, options) &&
        !zoneAllowsHyphen(block, words, firstWordIndex, breakAt,
                          flatInterval->interval.length))
      breakAt = wholeWordStart(words, breakAt);
    flushLine(breakAt, /*isLast=*/false);
    wordIndex = breakAt;
    flatInterval = intervalSequence.intervalAt(++intervalIndex);
  }

  flushLine(wordIndex, /*isLast=*/true);
  return lastIntervalUsed;
}

}  // namespace detail

}  // namespace sigil::weave
