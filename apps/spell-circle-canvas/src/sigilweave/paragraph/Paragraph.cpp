/** @file
 * The paragraph's incremental pipeline: span normalization, the edit log,
 * the analysis that turns text into words (ICU boundaries, scripts, bidi,
 * fallback), lazy shaping of those words through the shape cache, the
 * segment re-derivation a paint-only edit needs, sentence starts, the
 * strut and the natural width.
 */

#include "sigilweave/paragraph/Paragraph.h"

#include <hb.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <unicode/uchar.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>

#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/unicode/Unicode.h"

namespace sigil::weave {

namespace {

/** Applies the style's TextTransform to one segment slice, returning either
 *  `text` untouched or a view of `scratch` holding the mapped form. Case
 *  mapping is locale-sensitive through the style's languageTag. kCapitalize
 *  titlecases only the first code point of a word (CSS semantics), so it
 *  applies just to the segment that starts the word. */
std::u16string_view applyTextTransform(const ShapingStyle& shaping,
                                       std::u16string_view text,
                                       bool segmentStartsWord,
                                       std::u16string& scratch) {
  if (shaping.textTransform == TextTransform::kNone || text.empty())
    return text;
  unicode::Case mapping = unicode::Case::kUpper;
  switch (shaping.textTransform) {
    case TextTransform::kUppercase:
      mapping = unicode::Case::kUpper;
      break;
    case TextTransform::kLowercase:
      mapping = unicode::Case::kLower;
      break;
    case TextTransform::kCapitalize:
      if (!segmentStartsWord) return text;
      mapping = unicode::Case::kCapitalize;
      break;
    case TextTransform::kNone:
      return text;
  }
  return unicode::caseMap(text, mapping, shaping.languageTag, scratch)
             ? std::u16string_view(scratch)
             : text;
}

ScriptTag harfBuzzScriptFor(unicode::Script script) {
  if (!unicode::isSpecificScript(script) || script >= unicode::scriptLimit())
    return static_cast<ScriptTag>(HB_SCRIPT_COMMON);  // hb falls back to DFLT
  // hb_script_from_string re-parses a 4-char tag every call; memoize the
  // whole (small, dense) script-code space once.
  static const auto scriptTable = [] {
    std::vector<ScriptTag> scripts(static_cast<size_t>(unicode::scriptLimit()));
    for (unicode::Script scriptIndex = 0; scriptIndex < unicode::scriptLimit();
         ++scriptIndex) {
      const char* name = unicode::scriptShortName(scriptIndex);
      scripts[static_cast<size_t>(scriptIndex)] = static_cast<ScriptTag>(
          name ? hb_script_from_string(name, -1) : HB_SCRIPT_COMMON);
    }
    return scripts;
  }();
  return scriptTable[static_cast<size_t>(script)];
}

// Placement form of one codepoint in a vertical paragraph: the span's
// explicit override, or UTR#50's per-character vertical orientation (CJK
// upright, Latin rotated — the CSS text-orientation:mixed behaviour).
SegmentForm resolveVerticalForm(const ShapingStyle& shaping,
                                char32_t codePoint) {
  switch (shaping.verticalForm) {
    case VerticalForm::kUpright:
      return SegmentForm::kUpright;
    case VerticalForm::kRotated:
      return SegmentForm::kRotated;
    case VerticalForm::kTateChuYoko:
      return SegmentForm::kTateChuYoko;
    case VerticalForm::kAuto:
      break;
  }
  // Tr (transformed-rotated, e.g. CJK brackets and long vowel marks) stays
  // upright too: TTB shaping applies the font's 'vert' substitutions, which
  // supply the rotated forms — the browser behaviour for
  // text-orientation: mixed. Only plain R (Latin, etc.) physically rotates.
  return unicode::verticalOrientation(codePoint) ==
                 unicode::VerticalOrientation::kRotated
             ? SegmentForm::kRotated
             : SegmentForm::kUpright;
}

}  // namespace

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

namespace {

/** The strut of one shaping style, which is the whole of what a strut is:
 *  the face's own metrics at the size it is set. */
Paragraph::Strut strutOfShaping(FontContext& fontContext,
                                const ShapingStyle& shaping) {
  sk_sp<SkTypeface> typeface =
      fontContext.variedTypeface(shaping.typeface, shaping.variations);
  const SkFont font = makeFont(typeface, shaping.fontSize);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  Paragraph::Strut strut;
  strut.ascent = -metrics.fAscent;
  strut.height = -metrics.fAscent + metrics.fDescent + metrics.fLeading;
  strut.capHeight = metrics.fCapHeight;
  strut.xHeight = metrics.fXHeight;
  return strut;
}

}  // namespace

Paragraph::Strut Paragraph::strut(FontContext& fontContext) const {
  ShapingStyle shaping;
  if (!m_spans.empty()) shaping = m_spans.front().style.shaping;
  return strutOfShaping(fontContext, shaping);
}

Paragraph::Strut Paragraph::strutAt(FontContext& fontContext,
                                    uint32_t textOffset) const {
  ShapingStyle shaping;
  if (!m_spans.empty()) {
    // The span the offset falls in; past the end, the last one, so a block
    // that begins where the text does not still answers with real metrics.
    shaping = m_spans.back().style.shaping;
    for (const StyleSpan& span : m_spans)
      if (textOffset < span.end) {
        shaping = span.style.shaping;
        break;
      }
  }
  return strutOfShaping(fontContext, shaping);
}

float Paragraph::naturalWidth(FontContext& fontContext) {
  ensureShaped(fontContext);
  float width = 0;
  for (size_t wordIndex = 0; wordIndex < m_words.size(); ++wordIndex) {
    width += m_words[wordIndex].width;
    if (wordIndex + 1 < m_words.size()) width += m_words[wordIndex].spaceWidth;
  }
  return width;
}

void Paragraph::ensureAnalyzed(FontContext& fontContext) {
  if (m_dirty) {
    normalizeSpans();
    analyze(fontContext);
    m_dirty = false;
    m_paintDirty = false;
    return;
  }
  if (m_paintDirty) {
    m_paintDirty = false;
    reshapeShapedPrefix(fontContext);
  }
}

// Paint-only reconcile: the text didn't change, so line breaks, scripts,
// and bidi levels all stand — skip analyze()'s O(text) ICU passes and only
// re-derive segments for the words that were already shaped, against the
// new span list. Unchanged words hit the shape cache; only a boundary
// landing mid-word shapes new sub-segments.
void Paragraph::reshapeShapedPrefix(FontContext& fontContext) {
  const uint32_t previouslyShapedWordCount = m_shapedWordCount;
  m_shapedWordCount = 0;
  m_shapingSpanCursor = 0;
  m_shapingScriptCursor = 0;
  m_cachedWhitespaceStyleIndex = ~0u;
  m_cachedWhitespaceText.clear();
  for (; m_shapedWordCount < previouslyShapedWordCount; ++m_shapedWordCount) {
    Word& word = m_words[m_shapedWordCount];
    if (word.placeholderIndex >= 0)
      continue;  // slots carry no glyphs; width comes from the record
    word.m_segments.clear();
    word.width = 0;
    word.spaceWidth = 0;
    shapeWordContent(fontContext, word);
  }
}

void Paragraph::ensureShapedTo(FontContext& fontContext, uint32_t wordCount) {
  ensureAnalyzed(fontContext);
  const uint32_t upTo =
      std::min(wordCount, static_cast<uint32_t>(m_words.size()));
  for (; m_shapedWordCount < upTo; ++m_shapedWordCount)
    shapeWordContent(fontContext, m_words[m_shapedWordCount]);
}

void Paragraph::ensureShaped(FontContext& fontContext) {
  ensureAnalyzed(fontContext);
  ensureShapedTo(fontContext, static_cast<uint32_t>(m_words.size()));
}

std::span<const uint32_t> Paragraph::sentenceStarts() const {
  if (m_sentenceStartsValid) return m_sentenceStarts;
  m_sentenceStartsValid = true;
  m_sentenceStarts = unicode::sentenceStarts(m_text);
  return m_sentenceStarts;
}

void Paragraph::setLineBreakLocale(std::string locale) {
  if (m_lineBreakLocale == locale) return;
  m_lineBreakLocale = std::move(locale);
  markDirty();  // break opportunities are decided in analyze()
}

void Paragraph::setKinsoku(KinsokuTable table) {
  if (m_kinsoku == table) return;
  m_kinsoku = std::move(table);
  markDirty();  // break opportunities are decided in analyze()
}

void Paragraph::openPatternBreaks(std::vector<unicode::LineBreak>& boundaries,
                                  std::vector<uint8_t>& isHyphen) const {
  const HyphenationLimits& limits = m_hyphenationLimits;
  std::vector<unicode::LineBreak> merged;
  std::vector<uint8_t> mergedFlags;
  merged.reserve(boundaries.size());
  mergedFlags.reserve(boundaries.size());
  std::vector<uint32_t> points;
  size_t spanCursor = 0;
  int32_t segmentStart = 0;
  for (const unicode::LineBreak& breakEntry : boundaries) {
    const uint32_t breakOffset = breakEntry.offset;
    const auto boundary = static_cast<int32_t>(breakOffset);
    if (boundary <= segmentStart) {
      merged.push_back(breakEntry);
      mergedFlags.push_back(0);
      continue;
    }
    // The word's content: the trailing whitespace and any typed soft hyphen
    // are the analysis's own business and never reach the hyphenator.
    int32_t contentEnd = boundary;
    while (contentEnd > segmentStart &&
           (unicode::isHardLineBreak(m_text[contentEnd - 1]) ||
            unicode::isWhitespace(m_text[contentEnd - 1])))
      --contentEnd;
    if (contentEnd > segmentStart && m_text[contentEnd - 1] == 0x00AD)
      --contentEnd;
    const auto length = static_cast<int32_t>(contentEnd - segmentStart);
    const bool longEnough = length >= limits.minimumWordLength &&
                            length > limits.minimumLettersBefore +
                                         limits.minimumLettersAfter;
    if (longEnough) {
      while (spanCursor + 1 < m_spans.size() &&
             m_spans[spanCursor].end <= static_cast<uint32_t>(segmentStart))
        ++spanCursor;
      const std::u16string_view word(m_text.data() + segmentStart,
                                     static_cast<size_t>(length));
      size_t firstOffset = 0;
      const bool capitalised = u_isupper(
          static_cast<UChar32>(unicode::decodeAt(word, firstOffset)));
      if (limits.capitalizedWords || !capitalised) {
        points.clear();
        const std::string& tag = m_spans.empty()
                                     ? std::string()
                                     : m_spans[spanCursor].style.shaping.languageTag;
        m_hyphenator->breakPoints(word, tag, points);
        for (const uint32_t offset : points) {
          const auto inside = static_cast<int32_t>(offset);
          if (inside < limits.minimumLettersBefore ||
              length - inside < limits.minimumLettersAfter)
            continue;
          const auto absolute = static_cast<uint32_t>(segmentStart + inside);
          if (!merged.empty() && merged.back().offset >= absolute) continue;
          merged.push_back({absolute, false});
          mergedFlags.push_back(1);
        }
      }
    }
    merged.push_back(breakEntry);
    mergedFlags.push_back(0);
    segmentStart = boundary;
  }
  boundaries.swap(merged);
  isHyphen.swap(mergedFlags);
}

void Paragraph::analyze(FontContext& fontContext) {
  static_cast<void>(fontContext);  // analysis reads only the text
  m_words.clear();
  if (m_text.empty()) return;

  const int32_t textLength = static_cast<int32_t>(m_text.size());

  // ── Line-break opportunities (UAX #14) ─────────────────────────────────
  // Scratch buffers persist across analyses (one FontContext == one thread
  // by contract, so thread_local matches the ownership model).
  static thread_local std::vector<unicode::LineBreak> boundaries;
  unicode::lineBreaks(m_text, boundaries, m_lineBreakLocale);

  // A soft hyphen (U+00AD) is the one discretionary break opportunity the
  // word list carries. When it is turned off, the boundary UAX#14 opened
  // right after the hyphen is dropped here so the two halves fuse into a
  // single unbreakable Word: no breaker can split there, and the hyphen —
  // now interior to the word — shapes as the zero-advance default-ignorable
  // it is, so the word wraps or overflows whole and unmarked. A hyphen
  // followed by whitespace keeps its boundary either way, because the break
  // belongs to the space and not to the hyphen.
  if (!m_softHyphenBreaks) {
    size_t keptCount = 0;
    for (const unicode::LineBreak& entry : boundaries) {
      if (entry.offset < static_cast<uint32_t>(textLength) &&
          m_text[static_cast<size_t>(entry.offset) - 1] == 0x00AD)
        continue;
      boundaries[keptCount++] = entry;
    }
    boundaries.resize(keptCount);
    if (boundaries.empty())
      boundaries.push_back({static_cast<uint32_t>(textLength), false});
  }

  // KINSOKU SHORI: a boundary that would open a line with a character
  // that may not open one, or close a line with one that may not close
  // one, is simply not a boundary. Dropping it here is push-out — the
  // word before comes down with the character — and it means neither
  // breaker knows the rule, exactly as neither knows the soft-hyphen one.
  if (!m_kinsoku.empty()) {
    size_t keptCount = 0;
    for (const unicode::LineBreak& entry : boundaries) {
      const uint32_t boundary = entry.offset;
      const bool interior = boundary < static_cast<uint32_t>(textLength);
      const bool opensProhibited =
          interior &&
          m_kinsoku.notLineStart.find(m_text[boundary]) != std::u16string::npos;
      const bool closesProhibited =
          interior && boundary > 0 &&
          m_kinsoku.notLineEnd.find(m_text[boundary - 1]) !=
              std::u16string::npos;
      // A break the text itself demands is never a prohibition's to drop.
      if ((opensProhibited || closesProhibited) && !entry.mandatory) continue;
      boundaries[keptCount++] = entry;
    }
    boundaries.resize(keptCount);
    if (boundaries.empty())
      boundaries.push_back({static_cast<uint32_t>(textLength), false});
  }

  // Pattern hyphenation: break opportunities INSIDE a word, which UAX #14
  // never opens because where a word may be split is a fact about a
  // language and not about Unicode. Each opportunity becomes its own
  // boundary, so a hyphenated word is several Words that a breaker may end
  // a line at exactly as it may end one at a typed soft hyphen — and the
  // flag beside each boundary is what says which of them carry a hyphen.
  static thread_local std::vector<uint8_t> patternBreak;
  patternBreak.assign(boundaries.size(), 0);
  if (m_hyphenator && m_softHyphenBreaks)
    openPatternBreaks(boundaries, patternBreak);

  // ── Script runs ────────────────────────────────────────────────────────
  static thread_local std::vector<unicode::ScriptRun> scriptRuns;
  unicode::itemize(m_text, scriptRuns);

  // ── Bidi levels (one run for LTR-only text, per-unit levels otherwise) ─
  const std::vector<unicode::BidiRun> bidiRuns = unicode::bidi(m_text);
  m_uniformBidirectionalLevel = 0;
  m_bidirectionalLevels.clear();
  if (bidiRuns.size() == 1) {
    m_uniformBidirectionalLevel = bidiRuns.front().level;
  } else if (bidiRuns.size() > 1) {
    m_bidirectionalLevels.resize(static_cast<size_t>(textLength));
    for (const unicode::BidiRun& run : bidiRuns)
      std::fill(m_bidirectionalLevels.begin() + run.start,
                m_bidirectionalLevels.begin() + run.end, run.level);
  }
  const uint8_t* bidiLevels =
      m_bidirectionalLevels.empty() ? nullptr : m_bidirectionalLevels.data();
  const uint8_t uniformLevel = m_uniformBidirectionalLevel;

  // ── Words: one per break segment (segment/shape runs resolved lazily) ──
  m_words.reserve(boundaries.size());
  int placeholdersSeen = 0;
  int32_t segmentStart = 0;
  for (size_t boundaryIndex = 0; boundaryIndex < boundaries.size();
       ++boundaryIndex) {
    const int32_t boundary =
        static_cast<int32_t>(boundaries[boundaryIndex].offset);
    const bool patternHyphen = patternBreak[boundaryIndex] != 0;
    if (boundary <= segmentStart) continue;

    // Object-replacement characters are placeholder slots (see
    // appendPlaceholder): unbreakable fixed-size words with no glyphs.
    // UAX#14 usually isolates U+FFFC (class CB), but trailing punctuation
    // can glue to it (e.g. "￼." via LB13), so peel slots off the front of
    // the segment instead of requiring exact isolation.
    while (segmentStart < boundary &&
           m_text[static_cast<size_t>(segmentStart)] == 0xFFFC &&
           static_cast<size_t>(placeholdersSeen) < m_placeholders.size()) {
      Word placeholderWord;
      placeholderWord.textBegin = static_cast<uint32_t>(segmentStart);
      placeholderWord.textEnd = placeholderWord.whitespaceEnd =
          static_cast<uint32_t>(segmentStart + 1);
      placeholderWord.bidiLevel =
          bidiLevels ? bidiLevels[segmentStart] : uniformLevel;
      placeholderWord.placeholderIndex = placeholdersSeen++;
      placeholderWord.width =
          m_placeholders[static_cast<size_t>(placeholderWord.placeholderIndex)]
              .width;
      m_words.push_back(std::move(placeholderWord));
      ++segmentStart;
    }
    if (boundary <= segmentStart)
      continue;  // the segment was nothing but slots

    // Trailing whitespace (including any hard-break characters at the end).
    int32_t whitespaceStart = boundary;
    while (whitespaceStart > segmentStart &&
           (unicode::isHardLineBreak(m_text[whitespaceStart - 1]) ||
            unicode::isWhitespace(m_text[whitespaceStart - 1])))
      --whitespaceStart;

    Word word;
    word.textBegin = static_cast<uint32_t>(segmentStart);
    word.whitespaceEnd = static_cast<uint32_t>(boundary);
    // WHETHER THE TEXT DEMANDS A BREAK HERE IS THE SEGMENTATION'S ANSWER,
    // reported beside the boundary that opened. Re-deriving it from the
    // characters before the boundary is a second, worse implementation of
    // a rule the analysis already applied.
    word.mandatoryBreakAfter = boundaries[boundaryIndex].mandatory;
    word.bidiLevel = bidiLevels ? bidiLevels[segmentStart] : uniformLevel;

    // A trailing soft hyphen (U+00AD) is a discretionary break: it never
    // shapes, but marks that a hyphen may be rendered if a breaker splits
    // here.
    if (whitespaceStart > segmentStart &&
        m_text[whitespaceStart - 1] == 0x00AD) {
      word.hyphenBreak = true;
      --whitespaceStart;
    }
    // A pattern opportunity consumes no character: the word simply ends
    // where the language says it may, and the hyphen is drawn only if a
    // breaker takes the break.
    if (patternHyphen) word.hyphenBreak = true;
    word.textEnd = static_cast<uint32_t>(whitespaceStart);

    // Tab-aware layouts (ParagraphLayoutOptions::tabStops) treat the glue
    // after this word as an advance-to-stop instead of measured whitespace.
    for (int32_t codeUnitIndex = whitespaceStart;
         codeUnitIndex < boundary && !word.tabAfter; ++codeUnitIndex)
      word.tabAfter = m_text[static_cast<size_t>(codeUnitIndex)] == u'\t';

    if (whitespaceStart > segmentStart) {
      size_t codePointEnd = static_cast<size_t>(segmentStart);
      word.ideographic =
          unicode::isFullWidth(unicode::decodeAt(m_text, codePointEnd));
    }

    // Glyphs, widths, and glue come later: shapeWordContent() fills them in
    // lazily (see ensureShapedTo), so text past the layout frontier never
    // pays for HarfBuzz.
    m_words.push_back(std::move(word));
    segmentStart = boundary;
  }

  // Persist what lazy shaping needs: the script runs (the levels are
  // already in place).
  m_scriptRunEnds.clear();
  m_scriptRunEnds.reserve(scriptRuns.size());
  for (const unicode::ScriptRun& run : scriptRuns)
    m_scriptRunEnds.push_back({run.end, run.script});
  m_shapedWordCount = 0;
  m_shapingSpanCursor = 0;
  m_shapingScriptCursor = 0;
  m_cachedWhitespaceStyleIndex = ~0u;
  m_cachedWhitespaceText.clear();
}

// Shapes one word's content: segment splitting (style / script / bidi /
// fallback-typeface / vertical-form boundaries), the discretionary hyphen,
// and trailing-whitespace glue. Called in ascending word order only — the
// span/script cursors advance monotonically.
void Paragraph::shapeWordContent(FontContext& fontContext, Word& word) {
  if (word.placeholderIndex >= 0)
    return;  // slots carry their size from the placeholder record

  auto styleIndexAt = [&](uint32_t position) -> uint32_t {
    while (m_shapingSpanCursor + 1 < m_spans.size() &&
           m_spans[m_shapingSpanCursor].end <= position)
      ++m_shapingSpanCursor;
    return m_shapingSpanCursor;
  };
  auto scriptAt = [&](uint32_t position) -> const ScriptRunEnd& {
    while (m_shapingScriptCursor + 1 < m_scriptRunEnds.size() &&
           m_scriptRunEnds[m_shapingScriptCursor].end <= position)
      ++m_shapingScriptCursor;
    return m_scriptRunEnds[m_shapingScriptCursor];
  };
  auto levelAt = [&](int32_t position) -> uint8_t {
    return m_bidirectionalLevels.empty()
               ? m_uniformBidirectionalLevel
               : m_bidirectionalLevels[static_cast<size_t>(position)];
  };

  // Split [textBegin, textEnd) wherever style, script, bidi level, or the
  // fallback-resolved typeface changes; shape each piece via the cache.
  const int32_t whitespaceStart = static_cast<int32_t>(word.textEnd);
  int32_t segmentStart = static_cast<int32_t>(word.textBegin);
  while (segmentStart < whitespaceStart) {
    const uint32_t styleIndex =
        styleIndexAt(static_cast<uint32_t>(segmentStart));
    const StyleSpan& span = m_spans[styleIndex];
    const ScriptRunEnd& scriptRun =
        scriptAt(static_cast<uint32_t>(segmentStart));
    const uint8_t bidiLevel = levelAt(segmentStart);

    int32_t segmentLimit =
        std::min<int32_t>({whitespaceStart, static_cast<int32_t>(span.end),
                           static_cast<int32_t>(scriptRun.end)});
    if (!m_bidirectionalLevels.empty()) {
      int32_t levelEnd = segmentStart;
      while (levelEnd < segmentLimit &&
             m_bidirectionalLevels[static_cast<size_t>(levelEnd)] == bidiLevel)
        ++levelEnd;
      segmentLimit = levelEnd;
    }

    // Extend while the resolved typeface (and, in vertical mode, the
    // per-character vertical form) stays put.
    const bool verticalMode = m_writingMode == WritingMode::kVerticalRL;
    const char* languageTag = span.style.shaping.languageTag.empty()
                                  ? nullptr
                                  : span.style.shaping.languageTag.c_str();
    // Variations resolve once per segment: the varied clone (memoized in
    // the FontContext) becomes the primary for per-codepoint fallback, so
    // its uniqueID — not the base's — keys every shape-cache entry.
    const sk_sp<SkTypeface> primaryTypeface = fontContext.variedTypeface(
        span.style.shaping.typeface, span.style.shaping.variations);
    sk_sp<SkTypeface> resolvedTypeface;
    SegmentForm segmentForm =
        verticalMode ? SegmentForm::kUpright : SegmentForm::kFlow;
    bool hasResolvedForm = false;
    int32_t segmentEnd = segmentStart;
    int32_t codeUnitOffset = segmentStart;
    // Decoding stops at the segment limit, so a surrogate pair the limit
    // splits decodes as its lone high surrogate.
    const std::u16string_view segmentUnits(m_text.data(),
                                           static_cast<size_t>(segmentLimit));
    while (codeUnitOffset < segmentLimit) {
      const int32_t codePointStart = codeUnitOffset;
      size_t decodeOffset = static_cast<size_t>(codeUnitOffset);
      const char32_t codePoint = unicode::decodeAt(segmentUnits, decodeOffset);
      codeUnitOffset = static_cast<int32_t>(decodeOffset);
      if (unicode::inheritsTypeface(codePoint)) {
        segmentEnd = codeUnitOffset;
        continue;
      }
      if (verticalMode) {
        const SegmentForm codePointForm =
            resolveVerticalForm(span.style.shaping, codePoint);
        if (!hasResolvedForm) {
          segmentForm = codePointForm;
          hasResolvedForm = true;
        } else if (codePointForm != segmentForm) {
          segmentEnd = codePointStart;
          break;
        }
      }
      sk_sp<SkTypeface> codePointTypeface = fontContext.resolveTypeface(
          primaryTypeface, static_cast<int32_t>(codePoint), languageTag);
      if (!resolvedTypeface) {
        resolvedTypeface = std::move(codePointTypeface);
      } else if (codePointTypeface.get() != resolvedTypeface.get()) {
        segmentEnd = codePointStart;
        break;
      }
      segmentEnd = codeUnitOffset;
    }
    if (!resolvedTypeface)
      resolvedTypeface =
          primaryTypeface ? primaryTypeface : fontContext.defaultTypeface();
    if (segmentEnd <= segmentStart)
      segmentEnd =
          segmentLimit > segmentStart ? segmentLimit : segmentStart + 1;

    const bool shapeVertical =
        verticalMode && segmentForm == SegmentForm::kUpright;
    // The transformed slice is what gets shaped AND what keys the cache, so
    // "HELLO" and "hello"+kUppercase share one entry (see TextTransform).
    static thread_local std::u16string transformScratch;
    const std::u16string_view segmentText = applyTextTransform(
        span.style.shaping,
        std::u16string_view(m_text).substr(
            static_cast<size_t>(segmentStart),
            static_cast<size_t>(segmentEnd - segmentStart)),
        segmentStart == static_cast<int32_t>(word.textBegin), transformScratch);
    ShapedWordRef shapedWord =
        shapeWord(fontContext, span.style.shaping, resolvedTypeface,
                  segmentText, harfBuzzScriptFor(scriptRun.script),
                  (bidiLevel & 1u) != 0 && !shapeVertical, shapeVertical);
    if (verticalMode && segmentForm == SegmentForm::kTateChuYoko) {
      // 縦中横 occupies its font height along the column; advanceOffset lands
      // on the run's baseline inside that box so placement needs no metrics.
      const SkFont font =
          makeFont(resolvedTypeface, span.style.shaping.fontSize);
      SkFontMetrics fontMetrics;
      font.getMetrics(&fontMetrics);
      word.m_segments.append({shapedWord, styleIndex,
                              word.width - fontMetrics.fAscent, segmentForm,
                              static_cast<uint32_t>(segmentStart)});
      word.width += -fontMetrics.fAscent + fontMetrics.fDescent;
    } else {
      word.m_segments.append({shapedWord, styleIndex, word.width, segmentForm,
                              static_cast<uint32_t>(segmentStart)});
      word.width += shapedWord->advance;
    }
    segmentStart = segmentEnd;
  }

  if (word.hyphenBreak) {
    // Shape the hyphen the breaker may render here, in the style of the
    // word's tail. Content-addressed like everything else: one entry per
    // style, shared by every hyphenatable word.
    const uint32_t styleIndex = word.segments().empty()
                                    ? styleIndexAt(word.textBegin)
                                    : word.segments().back().styleIndex;
    const StyleSpan& span = m_spans[styleIndex];
    sk_sp<SkTypeface> typeface = fontContext.variedTypeface(
        span.style.shaping.typeface, span.style.shaping.variations);
    word.hyphenGlyph =
        shapeWord(fontContext, span.style.shaping, typeface, u"-",
                  static_cast<ScriptTag>(HB_SCRIPT_COMMON), false);
  }

  // Trailing whitespace becomes justification glue. Hard-break characters
  // are zero-width, and a tab is shaped as a space here; when tab stops are
  // configured, layout replaces this glue with an advance to the next stop
  // (see glueAfter in ParagraphLayoutInternal.h), so the shaped width is
  // only the fallback. The overwhelmingly common glue (" " in the same
  // style as the previous word) is memoized, skipping even the shape-cache
  // probe.
  const int32_t whitespaceEnd = static_cast<int32_t>(word.whitespaceEnd);
  if (whitespaceStart < whitespaceEnd) {
    static thread_local std::u16string whitespaceScratch;
    whitespaceScratch.clear();
    bool needsScratch = false;
    for (int32_t codeUnitIndex = whitespaceStart; codeUnitIndex < whitespaceEnd;
         ++codeUnitIndex) {
      const char16_t character = m_text[static_cast<size_t>(codeUnitIndex)];
      if (unicode::isHardLineBreak(character) || character == u'\t') {
        needsScratch = true;
        break;
      }
    }
    std::u16string_view whitespace;
    if (needsScratch) {
      for (int32_t codeUnitIndex = whitespaceStart;
           codeUnitIndex < whitespaceEnd; ++codeUnitIndex) {
        const char16_t character = m_text[static_cast<size_t>(codeUnitIndex)];
        if (unicode::isHardLineBreak(character)) continue;
        whitespaceScratch.push_back(character == u'\t' ? u' ' : character);
      }
      whitespace = whitespaceScratch;
    } else {
      whitespace = std::u16string_view(m_text).substr(
          static_cast<size_t>(whitespaceStart),
          static_cast<size_t>(whitespaceEnd - whitespaceStart));
    }
    if (!whitespace.empty()) {
      const uint32_t styleIndex =
          styleIndexAt(static_cast<uint32_t>(whitespaceStart));
      if (styleIndex == m_cachedWhitespaceStyleIndex &&
          whitespace == m_cachedWhitespaceText) {
        word.spaceWidth = m_cachedWhitespaceWidth;
      } else {
        const StyleSpan& span = m_spans[styleIndex];
        sk_sp<SkTypeface> whitespaceTypeface = fontContext.variedTypeface(
            span.style.shaping.typeface, span.style.shaping.variations);
        ShapedWordRef shapedWhitespace = shapeWord(
            fontContext, span.style.shaping, whitespaceTypeface, whitespace,
            static_cast<ScriptTag>(HB_SCRIPT_COMMON), false,
            /*vertical=*/m_writingMode == WritingMode::kVerticalRL);
        word.spaceWidth = shapedWhitespace->advance;
        m_cachedWhitespaceStyleIndex = styleIndex;
        m_cachedWhitespaceText.assign(whitespace);
        m_cachedWhitespaceWidth = word.spaceWidth;
      }
      // Word spacing adds to the glue after measuring (and after the memo,
      // which stores the shaped base width): the breakers and justification
      // consume spaceWidth, so they pick the extra up with no other change.
      // Not part of the shape-cache key — the glyphs are unchanged.
      const float wordSpacing = m_spans[styleIndex].style.shaping.wordSpacing;
      if (wordSpacing != 0)
        word.spaceWidth = std::max(0.0f, word.spaceWidth + wordSpacing);
    }
  }
}

}  // namespace sigil::weave
