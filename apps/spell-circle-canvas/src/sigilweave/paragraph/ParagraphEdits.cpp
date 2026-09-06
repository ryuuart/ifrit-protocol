/** @file
 * What an edit does to a paragraph: the edit log external range trackers
 * follow, span normalization, the text and placeholder edits, and the
 * paint-only edit that re-derives segments without reshaping a word.
 */

#include <algorithm>
#include <atomic>
#include <utility>
#include <vector>

#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/paragraph/Paragraph.h"

namespace sigil::weave {

void Paragraph::recordEdit(uint32_t start, uint32_t removedLength,
                           uint32_t insertedLength) {
  ++m_revision;
  m_sentenceStartsValid = false;  // the only thing that can move sentences
  // Bounded history, trimmed half at a time so trimming is amortized instead
  // of running on every edit once the cap is reached. The consequence for
  // callers is in editsSince(): the lookback that is always available is
  // kMaxHistory / 2 edits, not kMaxHistory.
  constexpr size_t kMaxHistory = 256;
  if (m_editHistory.size() >= kMaxHistory) {
    m_editHistory.erase(
        m_editHistory.begin(),
        m_editHistory.begin() + static_cast<long>(kMaxHistory / 2));
    m_editHistoryBaseRevision += kMaxHistory / 2;
  }
  m_editHistory.push_back({start, removedLength, insertedLength});
}

bool Paragraph::editsSince(uint64_t sinceRevision,
                           std::vector<TextEdit>& edits) const {
  if (sinceRevision > m_revision || sinceRevision < m_editHistoryBaseRevision)
    return false;
  for (uint64_t revision = sinceRevision; revision < m_revision; ++revision)
    edits.push_back(m_editHistory[static_cast<size_t>(
        revision - m_editHistoryBaseRevision)]);
  return true;
}

void Paragraph::clear() {
  if (!m_text.empty()) recordEdit(0, static_cast<uint32_t>(m_text.size()), 0);
  m_text.clear();
  m_spans.clear();
  m_words.clear();
  m_placeholders.clear();
  markDirty();
}

void Paragraph::appendPlaceholder(const Placeholder& placeholder,
                                  const TextStyle& style) {
  m_placeholders.push_back(placeholder);
  appendText(std::u16string_view(u"\uFFFC"), style);
}

void Paragraph::setPlaceholder(size_t index, const Placeholder& placeholder) {
  if (index >= m_placeholders.size()) return;
  m_placeholders[index] = placeholder;
  markDirty();
}

void Paragraph::appendText(std::u8string_view utf8, const TextStyle& style) {
  appendText(std::u16string_view(unicode::toUtf16(utf8)), style);
}

void Paragraph::setWritingMode(WritingMode mode) {
  if (m_writingMode == mode) return;
  m_writingMode = mode;
  markDirty();
}

void Paragraph::setSoftHyphenBreaks(bool enabled) {
  if (m_softHyphenBreaks == enabled) return;
  m_softHyphenBreaks = enabled;
  markDirty();  // break opportunities are decided in analyze()
}

void Paragraph::setHyphenator(const Hyphenator* hyphenator,
                              HyphenationLimits limits) {
  if (m_hyphenator == hyphenator && m_hyphenationLimits == limits) return;
  m_hyphenator = hyphenator;
  m_hyphenationLimits = limits;
  markDirty();  // break opportunities are decided in analyze()
}

void Paragraph::appendText(std::u16string_view utf16, const TextStyle& style) {
  if (utf16.empty()) return;
  const uint32_t start = static_cast<uint32_t>(m_text.size());
  m_text.append(utf16);
  m_spans.push_back({start, static_cast<uint32_t>(m_text.size()), style});
  recordEdit(start, 0, static_cast<uint32_t>(utf16.size()));
  markDirty();
}

void Paragraph::normalizeSpans() {
  if (m_text.empty()) {
    m_spans.clear();
    return;
  }
  std::stable_sort(
      m_spans.begin(), m_spans.end(),
      [](const StyleSpan& a, const StyleSpan& b) { return a.start < b.start; });
  // Drop empties, clamp to text, and fill any gaps with the previous span's
  // style so every position resolves to exactly one span.
  std::vector<StyleSpan> normalizedSpans;
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());
  uint32_t cursor = 0;
  for (StyleSpan& span : m_spans) {
    span.start = std::min(span.start, textLength);
    span.end = std::min(span.end, textLength);
    if (span.end <= span.start) continue;
    if (span.start > cursor) {
      const TextStyle& fillStyle =
          normalizedSpans.empty() ? span.style : normalizedSpans.back().style;
      normalizedSpans.push_back({cursor, span.start, fillStyle});
    }
    if (span.start < cursor)
      span.start = cursor;  // overlapping spans: later one yields
    if (span.end <= span.start) continue;
    // Merge adjacent equal-styled spans, otherwise repeated restyling
    // fragments the span list without bound (and every span boundary splits
    // a word into separately shaped segments).
    if (!normalizedSpans.empty() && normalizedSpans.back().end == span.start &&
        normalizedSpans.back().style == span.style) {
      normalizedSpans.back().end = span.end;
    } else {
      normalizedSpans.push_back(span);
    }
    cursor = span.end;
  }
  if (cursor < textLength) {
    const TextStyle fillStyle =
        normalizedSpans.empty() ? TextStyle{} : normalizedSpans.back().style;
    if (!normalizedSpans.empty() && normalizedSpans.back().end == cursor &&
        normalizedSpans.back().style == fillStyle)
      normalizedSpans.back().end = textLength;
    else
      normalizedSpans.push_back({cursor, textLength, fillStyle});
  }
  m_spans = std::move(normalizedSpans);
}

void Paragraph::replaceText(uint32_t start, uint32_t end,
                            std::u8string_view utf8) {
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());
  start = std::min(start, textLength);
  end = std::min(std::max(end, start), textLength);
  const std::u16string insertedText = unicode::toUtf16(utf8);
  const uint32_t insertedLength = static_cast<uint32_t>(insertedText.size());

  // Style the inserted range like the text at the edit point.
  TextStyle insertedStyle;
  for (const StyleSpan& span : m_spans) {
    if (span.start <= start && (start < span.end || span.end == textLength)) {
      insertedStyle = span.style;
      break;
    }
  }
  if (m_spans.empty()) insertedStyle = TextStyle{};

  m_text.replace(start, end - start, insertedText);

  const uint32_t deletedLength = end - start;
  auto remapPosition = [&](uint32_t position, bool marksRangeEnd) -> uint32_t {
    // Delete [start, end) …
    if (position > start)
      position = position >= end ? position - deletedLength : start;
    // Then open an insertedLength gap. Positions equal to start remain in
    // place for range ends but shift for starts, so the new span owns it.
    if (position > start || (position == start && !marksRangeEnd))
      position += insertedLength;
    return position;
  };
  for (StyleSpan& span : m_spans) {
    span.start = remapPosition(span.start, false);
    span.end = remapPosition(span.end, true);
  }
  if (insertedLength > 0)
    m_spans.push_back({start, start + insertedLength, insertedStyle});
  recordEdit(start, deletedLength, insertedLength);
  normalizeSpans();
  markDirty();
}

void Paragraph::setStyle(uint32_t start, uint32_t end, const TextStyle& style) {
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());
  start = std::min(start, textLength);
  end = std::min(std::max(end, start), textLength);
  if (start == end) return;

  std::vector<StyleSpan> updatedSpans;
  updatedSpans.reserve(m_spans.size() + 2);
  for (const StyleSpan& span : m_spans) {
    if (span.end <= start || span.start >= end) {
      updatedSpans.push_back(span);
      continue;
    }
    if (span.start < start)
      updatedSpans.push_back({span.start, start, span.style});
    if (span.end > end) updatedSpans.push_back({end, span.end, span.style});
  }
  updatedSpans.push_back({start, end, style});
  m_spans = std::move(updatedSpans);
  normalizeSpans();
  markDirty();
}

namespace {

// Span boundaries unchanged means every WordSegment::styleIndex still points
// at the right span — a repaint over the same ranges (hue cycling a marker
// set every frame) needs no reconcile at all.
std::vector<std::pair<uint32_t, uint32_t>> spanBoundaries(
    const std::vector<StyleSpan>& spans) {
  std::vector<std::pair<uint32_t, uint32_t>> boundaries;
  boundaries.reserve(spans.size());
  for (const StyleSpan& span : spans)
    boundaries.emplace_back(span.start, span.end);
  return boundaries;
}

bool sameSpanBoundaries(
    const std::vector<std::pair<uint32_t, uint32_t>>& previousBoundaries,
    const std::vector<StyleSpan>& spans) {
  if (previousBoundaries.size() != spans.size()) return false;
  for (size_t spanIndex = 0; spanIndex < previousBoundaries.size(); ++spanIndex)
    if (previousBoundaries[spanIndex].first != spans[spanIndex].start ||
        previousBoundaries[spanIndex].second != spans[spanIndex].end)
      return false;
  return true;
}

}  // namespace

void Paragraph::setPaint(uint32_t start, uint32_t end,
                         const PaintStyle& paint) {
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());
  start = std::min(start, textLength);
  end = std::min(std::max(end, start), textLength);
  if (start == end) return;

  const std::vector<std::pair<uint32_t, uint32_t>> previousBoundaries =
      spanBoundaries(m_spans);
  std::vector<StyleSpan> updatedSpans;
  updatedSpans.reserve(m_spans.size() + 2);
  for (const StyleSpan& span : m_spans) {
    if (span.end <= start || span.start >= end) {
      updatedSpans.push_back(span);
      continue;
    }
    if (span.start < start)
      updatedSpans.push_back({span.start, start, span.style});
    StyleSpan paintedSpan{std::max(span.start, start), std::min(span.end, end),
                          span.style};
    paintedSpan.style.paint = paint;
    updatedSpans.push_back(paintedSpan);
    if (span.end > end) updatedSpans.push_back({end, span.end, span.style});
  }
  m_spans = std::move(updatedSpans);
  normalizeSpans();
  // Shaping keys and the text itself are untouched, so the words/scripts/
  // bidi analysis stands — at most the shaped prefix's span indices need
  // re-deriving (see reshapeShapedPrefix). When even the boundaries came
  // out unchanged (repainting the same ranges), the indices are already
  // right and nothing at all is dirty: draws just read the new paint.
  if (!sameSpanBoundaries(previousBoundaries, m_spans)) markPaintDirty();
}

void Paragraph::setPaint(std::span<const CharRange> ranges,
                         const PaintStyle& paint) {
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());

  // Sanitize into sorted, clamped, non-overlapping ranges.
  std::vector<CharRange> sanitizedRanges;
  sanitizedRanges.reserve(ranges.size());
  for (const CharRange& range : ranges) {
    CharRange sanitizedRange{std::min(range.start, textLength),
                             std::min(range.end, textLength)};
    if (!sanitizedRange.empty()) sanitizedRanges.push_back(sanitizedRange);
  }
  std::ranges::sort(sanitizedRanges, {}, &CharRange::start);
  size_t mergedRangeIndex = 0;
  for (size_t rangeIndex = 1; rangeIndex < sanitizedRanges.size();
       ++rangeIndex) {
    if (sanitizedRanges[rangeIndex].start <=
        sanitizedRanges[mergedRangeIndex].end)
      sanitizedRanges[mergedRangeIndex].end =
          std::max(sanitizedRanges[mergedRangeIndex].end,
                   sanitizedRanges[rangeIndex].end);
    else
      sanitizedRanges[++mergedRangeIndex] = sanitizedRanges[rangeIndex];
  }
  if (!sanitizedRanges.empty()) sanitizedRanges.resize(mergedRangeIndex + 1);
  if (sanitizedRanges.empty()) return;

  const std::vector<std::pair<uint32_t, uint32_t>> previousBoundaries =
      spanBoundaries(m_spans);
  // One pass over spans and ranges together: each output span is either an
  // untouched piece or a painted intersection, so the whole batch costs
  // O(spans + ranges) instead of one full rebuild per range.
  std::vector<StyleSpan> updatedSpans;
  updatedSpans.reserve(m_spans.size() + 2 * sanitizedRanges.size());
  size_t rangeIndex = 0;
  for (const StyleSpan& span : m_spans) {
    uint32_t position = span.start;
    while (position < span.end) {
      while (rangeIndex < sanitizedRanges.size() &&
             sanitizedRanges[rangeIndex].end <= position)
        ++rangeIndex;
      if (rangeIndex == sanitizedRanges.size() ||
          sanitizedRanges[rangeIndex].start >= span.end) {
        updatedSpans.push_back({position, span.end, span.style});
        break;
      }
      if (sanitizedRanges[rangeIndex].start > position) {
        updatedSpans.push_back(
            {position, sanitizedRanges[rangeIndex].start, span.style});
        position = sanitizedRanges[rangeIndex].start;
      }
      StyleSpan paintedSpan{position,
                            std::min(span.end, sanitizedRanges[rangeIndex].end),
                            span.style};
      paintedSpan.style.paint = paint;
      position = paintedSpan.end;
      updatedSpans.push_back(std::move(paintedSpan));
    }
  }
  m_spans = std::move(updatedSpans);
  normalizeSpans();
  if (!sameSpanBoundaries(previousBoundaries, m_spans)) markPaintDirty();
}

}  // namespace sigil::weave
