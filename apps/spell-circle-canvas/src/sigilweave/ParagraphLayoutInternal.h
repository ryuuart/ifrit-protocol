#pragma once

// Shared internals of the greedy (ParagraphLayout.cpp) and Knuth-Plass
// (KnuthPlass.cpp) breakers.

#include "sigilweave/Flow.h"
#include "sigilweave/Paragraph.h"
#include "sigilweave/ParagraphLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sigil::weave {
namespace detail {

struct FlatInterval {
  LineInterval interval;
  int sourceLineIndex = 0;
};

// Flattens a FlowGeometry's lines into one ordered sequence of intervals,
// fetched lazily. Both breakers consume geometry exclusively through this,
// so break decisions and placement always agree on interval numbering.
class IntervalSequence {
public:
  IntervalSequence(FlowGeometry &geometry, float lineHeight, float ascent,
                   float minimumWidth = 0)
      : m_geometry(geometry), m_lineHeight(lineHeight), m_ascent(ascent),
        m_minimumWidth(minimumWidth) {}

  /** Returns a flattened interval, fetching source lines on demand. */
  const FlatInterval *intervalAt(size_t intervalIndex) {
    while (intervalIndex >= m_flatIntervals.size() && !m_geometryExhausted)
      fetchLine();
    return intervalIndex < m_flatIntervals.size()
               ? &m_flatIntervals[intervalIndex]
               : nullptr;
  }

  /** Returns whether every source line has the same single measure. */
  bool uniform() const { return m_geometry.uniformIntervals(); }

private:
  /** Fetches and flattens the next source line into the interval cache. */
  void fetchLine() {
    m_sourceLineIntervals.clear();
    if (!m_geometry.lineIntervals(m_nextLineIndex, m_lineHeight, m_ascent,
                                  m_sourceLineIntervals)) {
      m_geometryExhausted = true;
      return;
    }
    for (const LineInterval &interval : m_sourceLineIntervals)
      if (interval.length >= m_minimumWidth)
        m_flatIntervals.push_back({interval, m_nextLineIndex});
    m_nextLineIndex++;
  }

  FlowGeometry &m_geometry;
  float m_lineHeight;
  float m_ascent;
  float m_minimumWidth;
  std::vector<FlatInterval> m_flatIntervals;
  std::vector<LineInterval> m_sourceLineIntervals;
  int m_nextLineIndex = 0;
  bool m_geometryExhausted = false;
};

// Natural (unjustified) width of a half-open word range on one line: content
// widths plus inter-word glue, the last word's trailing space excluded.
float naturalWidth(const std::vector<Word> &words, uint32_t firstWordIndex,
                   uint32_t endWordIndex);

// Whether tab stops are configured at all (ParagraphLayoutOptions::tabStops).
// Inline (as is glueAfter below): both sit on the breakers' hot loops, and
// keeping them header-defined preserves the inlining they had as
// TU-local functions.
inline bool tabStopsActive(const ParagraphLayoutOptions &options) {
  return !options.tabStops.positions.empty() || options.tabStops.interval > 0;
}

// The glue width after one word with the pen at `penPosition` (relative to
// the line interval's start): the distance to the next tab stop for tab
// gaps, the measured whitespace otherwise. Both breakers and placement
// resolve stops through this one function so they always agree on widths.
inline float glueAfter(const Word &word, float penPosition,
                       const ParagraphLayoutOptions &options) {
  if (!word.tabAfter || !tabStopsActive(options))
    return word.spaceWidth;
  constexpr float kMinTabAdvance = 0.5f; // a stop the pen already reached
                                         // is not "the next" stop
  for (const float stop : options.tabStops.positions)
    if (stop >= penPosition + kMinTabAdvance)
      return stop - penPosition;
  if (options.tabStops.interval > 0) {
    const float base = options.tabStops.positions.empty()
                           ? 0.0f
                           : options.tabStops.positions.back();
    const float distance = std::max(penPosition - base, 0.0f);
    const float repeats =
        std::floor(distance / options.tabStops.interval) + 1.0f;
    const float stop = base + repeats * options.tabStops.interval;
    if (stop >= penPosition + kMinTabAdvance)
      return stop - penPosition;
    return stop + options.tabStops.interval - penPosition;
  }
  return word.spaceWidth; // stops exhausted: tab degrades to a space
}

// Places a half-open word range into `interval` with the given alignment,
// appending PositionedRuns to `out`. Pure arithmetic over cached ShapedWords:
// straight horizontal intervals reuse each word's shared blob; rotated and
// contour intervals bake per-glyph RSXforms. When `hyphenBreakTaken`, the
// line ends at a soft hyphen and the word's hyphen glyph is rendered.
void placeWords(const std::vector<Word> &words, uint32_t firstWordIndex,
                uint32_t endWordIndex, const FlatInterval &interval,
                TextAlignment alignment, bool lastLine, bool hyphenBreakTaken,
                const ParagraphLayoutOptions &options, ParagraphLayout &out);

// Whether a non-final break before `endWordIndex` lands on a soft hyphen.
inline bool hyphenTakenAt(const std::vector<Word> &words, uint32_t endWordIndex,
                          bool lineIsFinal,
                          const ParagraphLayoutOptions &options) {
  return options.hyphenation.enabled && !lineIsFinal && endWordIndex > 0 &&
         endWordIndex <= words.size() && words[endWordIndex - 1].hyphenBreak &&
         words[endWordIndex - 1].hyphenGlyph &&
         !words[endWordIndex - 1].mandatoryBreakAfter;
}

// Knuth-Plass entry: breaks + places every word that fits. Takes the
// paragraph (not just its words) so it can pull lazy shaping along its own
// frontier. Defined in KnuthPlass.cpp.
ParagraphLayout knuthPlassLayout(FontContext &fontContext, Paragraph &paragraph,
                                 IntervalSequence &intervalSequence,
                                 const ParagraphLayoutOptions &options);

} // namespace detail
} // namespace sigil::weave
