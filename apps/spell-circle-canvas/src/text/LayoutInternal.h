#pragma once

// Shared internals of the greedy (Layout.cpp) and Knuth-Plass
// (KnuthPlass.cpp) breakers.

#include "textflow/Flow.h"
#include "textflow/Layout.h"
#include "textflow/Paragraph.h"

#include <cstdint>
#include <vector>

namespace textflow {
namespace detail {

struct FlatInterval {
  LineInterval iv;
  int line = 0;
};

// Flattens a FlowGeometry's lines into one ordered sequence of intervals,
// fetched lazily. Both breakers consume geometry exclusively through this,
// so break decisions and placement always agree on interval numbering.
class IntervalSequence {
public:
  IntervalSequence(FlowGeometry &geometry, float lineHeight, float ascent,
                   float minWidth = 0)
      : m_geometry(geometry), m_lineHeight(lineHeight), m_ascent(ascent),
        m_minWidth(minWidth) {}

  // Interval `k`, or nullptr once the geometry is exhausted.
  const FlatInterval *get(size_t k) {
    while (k >= m_flat.size() && !m_done)
      fetchLine();
    return k < m_flat.size() ? &m_flat[k] : nullptr;
  }

private:
  void fetchLine() {
    m_scratch.clear();
    if (!m_geometry.lineIntervals(m_nextLine, m_lineHeight, m_ascent,
                                  m_scratch)) {
      m_done = true;
      return;
    }
    for (const LineInterval &iv : m_scratch)
      if (iv.length >= m_minWidth)
        m_flat.push_back({iv, m_nextLine});
    m_nextLine++;
  }

  FlowGeometry &m_geometry;
  float m_lineHeight;
  float m_ascent;
  float m_minWidth;
  std::vector<FlatInterval> m_flat;
  std::vector<LineInterval> m_scratch;
  int m_nextLine = 0;
  bool m_done = false;
};

// Natural (unjustified) width of words [first, last) on one line: content
// widths plus inter-word glue, the last word's trailing space excluded.
float naturalWidth(const std::vector<Word> &words, uint32_t first,
                   uint32_t last);

// Places words [first, last) into `interval` with the given alignment,
// appending PlacedRuns to `out`. Pure arithmetic over cached ShapedWords:
// straight horizontal intervals reuse each word's shared blob; rotated and
// contour intervals bake per-glyph RSXforms. When `hyphenBreakTaken`, the
// line ends at a soft hyphen and the word's hyphen glyph is rendered.
void placeWords(const std::vector<Word> &words, uint32_t first, uint32_t last,
                const FlatInterval &interval, Alignment alignment,
                bool lastLine, bool hyphenBreakTaken,
                const LayoutOptions &options, Layout &out);

// Whether a (non-final) break after word `last-1` lands on a soft hyphen.
inline bool hyphenTakenAt(const std::vector<Word> &words, uint32_t last,
                          bool lineIsFinal, const LayoutOptions &options) {
  return options.hyphenate && !lineIsFinal && last > 0 &&
         last <= words.size() && words[last - 1].hyphenBreak &&
         words[last - 1].hyphenGlyph &&
         !words[last - 1].mandatoryBreakAfter;
}

// Knuth-Plass entry: breaks + places every word. Defined in KnuthPlass.cpp.
Layout knuthPlassLayout(const std::vector<Word> &words, IntervalSequence &seq,
                        const LayoutOptions &options);

} // namespace detail
} // namespace textflow
