#pragma once

/** @file
 * WHAT A CANDIDATE LINE'S WIDTH AND ELASTICITY ARE READ OUT OF: prefix
 * sums over one block's content and glue, extended as the breaker's
 * frontier advances, and the four readings the dynamic program takes off
 * them.
 */

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include "ParagraphLayoutInternal.h"

namespace sigil::weave::detail {

/** THE PREFIX SUMS ONE BLOCK IS BROKEN AGAINST.
 *
 *  A candidate line is a half-open word range, and its natural width and
 *  its elasticity are differences of running totals: content width per
 *  word, and glue width, stretch and shrink per gap, where gap i sits
 *  after word i and the last word's gap is never on a line. Every array
 *  is indexed from the block's first word, so a block in the middle of a
 *  text costs what IT holds and not what precedes it.
 *
 *  THE CALLER OWNS THE STORAGE, and this only points at it. The composer
 *  is meant to run on moving text, which means running every frame, and a
 *  breaker that allocates four vectors per block per frame spends its
 *  budget in the allocator, so the caller holds one `Tables` for the life
 *  of its thread; cleared and refilled here, never freed. One block is
 *  summed at a time, so a second reader over the same tables would find
 *  the first's totals — the breaker builds one and runs it to the end of
 *  the block.
 *
 *  IT POINTS AT THE TABLES RATHER THAN HOLDING THEM, AND THAT IS THE
 *  WHOLE REASON THIS TYPE IS SHAPED AS IT IS. `natural`, `stretch` and
 *  `shrink` are asked once per candidate line per surviving path, which
 *  is the innermost thing the breaker does. Reaching the totals through
 *  thread-local storage from inside those readings puts a thread-local
 *  access on the innermost loop, and the bench's Knuth-Plass arms all
 *  move together when it does — measurably, and by more than the readings
 *  themselves cost. A plain reference costs nothing there, and it is what
 *  lets the readings live in a header instead of inside the dynamic
 *  program.
 *
 *  EVERY READING IS DEFINED HERE, IN THE CLASS, for the same loop: the
 *  same bodies compiled behind a translation-unit boundary move the same
 *  arms the same way. */
class LinePrefixSums {
 public:
  /** THE STORAGE, which the caller owns. */
  struct Tables {
    std::vector<float> width, glue, stretch, shrink;
    std::vector<uint32_t> tabGaps;
  };

  LinePrefixSums(Tables& tables, const std::vector<Word>& words,
                 const Block& block)
      : m_width(tables.width),
        m_glue(tables.glue),
        m_stretch(tables.stretch),
        m_shrink(tables.shrink),
        m_tabGaps(tables.tabGaps),
        m_words(words),
        m_block(block),
        m_options(*block.options),
        m_base(block.firstWord),
        m_wordCount(block.endWord),
        m_tabAware(tabStopsActive(m_options)),
        m_spacedByTable(!block.mojikumiAfter.empty()),
        m_hyphenating(m_options.hyphenation.enabled) {
    m_width.assign(1, 0);
    m_glue.assign(1, 0);
    m_stretch.assign(1, 0);
    m_shrink.assign(1, 0);
    m_tabGaps.clear();
  }

  /** Whether the block's setting puts tab gaps between words at all. A
   *  tab gap is pen-dependent, so it contributes nothing to the sums and
   *  `tabResolvedNatural` answers for the lines that hold one. */
  [[nodiscard]] bool tabAware() const { return m_tabAware; }

  /** Sums every word up to @p endWordIndex, from wherever the last call
   *  reached. The breaker calls this as its frontier advances, so a block
   *  the search never reaches the end of is never summed to the end. */
  void ensureTo(uint32_t endWordIndex) {
    for (uint32_t wordIndex =
             m_base + static_cast<uint32_t>(m_width.size()) - 1;
         wordIndex < endWordIndex; ++wordIndex) {
      m_width.push_back(m_width[at(wordIndex)] + m_words[wordIndex].width);
      float glue = 0;
      float stretch = 0;
      float shrink = 0;
      if (m_tabAware && m_words[wordIndex].tabAfter) {
        // Tab gaps are rigid (columns pin to stops) and positional; the
        // width they'll actually take is resolved per candidate line.
        m_tabGaps.push_back(wordIndex);
      } else if (m_words[wordIndex].spaceWidth > 0) {
        glue =
            m_words[wordIndex].spaceWidth * m_options.justification.wordSpacing;
        stretch = glue * m_options.justification.spaceStretch;
        shrink = glue * m_options.justification.spaceShrink;
      } else if (m_options.justification.expandIdeographicGaps &&
                 wordIndex + 1 < m_wordCount &&
                 (m_words[wordIndex].ideographic ||
                  m_words[wordIndex + 1].ideographic)) {
        const float fontSize =
            m_words[wordIndex].segments().empty()
                ? 16.0f
                : m_words[wordIndex].segments()[0].shaped->fontSize;
        stretch = fontSize * 0.25f;
        shrink = fontSize * 0.03f;
      }
      // The room a mojikumi table or tsume puts after this word is part of
      // the gap and none of it is elastic: a table states a distance and a
      // justified line spends its slack in the gaps the face gave it. A
      // layout that asked for neither answers the question once, here,
      // rather than once per word.
      if (m_spacedByTable) glue += mojikumiAfter(m_block, wordIndex);
      m_glue.push_back(m_glue[at(wordIndex)] + glue);
      m_stretch.push_back(m_stretch[at(wordIndex)] + stretch);
      m_shrink.push_back(m_shrink[at(wordIndex)] + shrink);
    }
  }

  /** Extra width when a line ending at @p breakIndex ends on a
   *  discretionary (soft-hyphen) break. */
  [[nodiscard]] float hyphenWidthAt(uint32_t breakIndex) const {
    return m_hyphenating && hyphenTakenAt(m_words, breakIndex,
                                          breakIndex == m_wordCount, m_options)
               ? m_words[breakIndex - 1].hyphenGlyph->advance
               : 0.0f;
  }

  /** Natural width of the line holding [@p lineStart, @p lineEnd). */
  [[nodiscard]] float natural(uint32_t lineStart, uint32_t lineEnd) const {
    return (m_width[at(lineEnd)] - m_width[at(lineStart)]) +
           (m_glue[at(lineEnd - 1)] - m_glue[at(lineStart)]) +
           hyphenWidthAt(lineEnd);
  }
  /** How far that line's gaps can open. */
  [[nodiscard]] float stretch(uint32_t lineStart, uint32_t lineEnd) const {
    return m_stretch[at(lineEnd - 1)] - m_stretch[at(lineStart)];
  }
  /** How far they can close. */
  [[nodiscard]] float shrink(uint32_t lineStart, uint32_t lineEnd) const {
    return m_shrink[at(lineEnd - 1)] - m_shrink[at(lineStart)];
  }

  /** The tab gaps interior to [@p lineStart, @p lineEnd): gap indices in
   *  [lineStart, lineEnd - 1), since the break-side gap is never on the
   *  line. */
  [[nodiscard]] std::span<const uint32_t> tabGapsIn(uint32_t lineStart,
                                                    uint32_t lineEnd) const {
    const auto first =
        std::lower_bound(m_tabGaps.begin(), m_tabGaps.end(), lineStart);
    const auto last = std::lower_bound(first, m_tabGaps.end(), lineEnd - 1);
    return std::span<const uint32_t>(first, last);
  }

  /** Natural width of a line that contains tab gaps: tab-separated
   *  segments accumulate from the sums (tab gaps contributed zero there);
   *  each tab then jumps the pen to its stop through the same glueAfter
   *  placement will use, so the breaker's width for a candidate line is
   *  exactly the width it renders at. */
  [[nodiscard]] float tabResolvedNatural(uint32_t lineStart, uint32_t lineEnd,
                                         std::span<const uint32_t> tabs) const {
    float pen = 0;
    uint32_t segmentStart = lineStart;
    for (const uint32_t tabIndex : tabs) {
      pen += (m_width[at(tabIndex + 1)] - m_width[at(segmentStart)]) +
             (m_glue[at(tabIndex)] - m_glue[at(segmentStart)]);
      pen += glueAfter(m_words[tabIndex], pen, m_options);
      segmentStart = tabIndex + 1;
    }
    return pen + (m_width[at(lineEnd)] - m_width[at(segmentStart)]) +
           (m_glue[at(lineEnd - 1)] - m_glue[at(segmentStart)]) +
           hyphenWidthAt(lineEnd);
  }

 private:
  [[nodiscard]] uint32_t at(uint32_t wordIndex) const {
    return wordIndex - m_base;
  }

  std::vector<float>& m_width;
  std::vector<float>& m_glue;
  std::vector<float>& m_stretch;
  std::vector<float>& m_shrink;
  std::vector<uint32_t>& m_tabGaps;
  const std::vector<Word>& m_words;
  const Block& m_block;
  const ParagraphLayoutOptions& m_options;
  uint32_t m_base;
  uint32_t m_wordCount;
  bool m_tabAware;
  bool m_spacedByTable;
  bool m_hyphenating;
};

}  // namespace sigil::weave::detail
