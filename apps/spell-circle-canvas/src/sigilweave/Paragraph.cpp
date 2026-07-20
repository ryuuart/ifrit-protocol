#include "sigilweave/Paragraph.h"
#include "FontContextImpl.h"
#include "sigilweave/FontContext.h"

#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>

#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cassert>

namespace sigil::weave {

namespace {

std::u16string utf8ToUtf16(std::u8string_view utf8) {
  if (utf8.empty())
    return {};
  std::u16string utf16;
  utf16.resize(utf8.size()); // UTF-16 never has more units than UTF-8 bytes
  UErrorCode status = U_ZERO_ERROR;
  int32_t codeUnitsWritten = 0;
  u_strFromUTF8(reinterpret_cast<UChar *>(utf16.data()),
                static_cast<int32_t>(utf16.size()), &codeUnitsWritten,
                reinterpret_cast<const char *>(utf8.data()),
                static_cast<int32_t>(utf8.size()), &status);
  if (U_FAILURE(status))
    return {};
  utf16.resize(codeUnitsWritten);
  return utf16;
}

/** Applies the style's TextTransform to one segment slice, returning either
 *  `text` untouched or a view of `scratch` holding the mapped form. ICU case
 *  mapping is locale-sensitive through the style's languageTag. kCapitalize
 *  titlecases only the first code point of a word (CSS semantics), so it
 *  applies just to the segment that starts the word. */
std::u16string_view applyTextTransform(const ShapingStyle &shaping,
                                       std::u16string_view text,
                                       bool segmentStartsWord,
                                       std::u16string &scratch) {
  if (shaping.textTransform == TextTransform::kNone || text.empty())
    return text;
  const char *locale =
      shaping.languageTag.empty() ? nullptr : shaping.languageTag.c_str();

  // Runs the double-call ICU pattern into `scratch` for the given mapper.
  auto caseMap = [&](std::u16string_view source, auto &&mapFunction,
                     size_t scratchOffset) -> bool {
    UErrorCode status = U_ZERO_ERROR;
    scratch.resize(scratchOffset + source.size() + 8);
    int32_t written = mapFunction(
        reinterpret_cast<UChar *>(scratch.data() + scratchOffset),
        static_cast<int32_t>(scratch.size() - scratchOffset),
        reinterpret_cast<const UChar *>(source.data()),
        static_cast<int32_t>(source.size()), locale, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      scratch.resize(scratchOffset + static_cast<size_t>(written));
      written = mapFunction(
          reinterpret_cast<UChar *>(scratch.data() + scratchOffset),
          static_cast<int32_t>(scratch.size() - scratchOffset),
          reinterpret_cast<const UChar *>(source.data()),
          static_cast<int32_t>(source.size()), locale, &status);
    }
    if (U_FAILURE(status))
      return false;
    scratch.resize(scratchOffset + static_cast<size_t>(written));
    return true;
  };

  switch (shaping.textTransform) {
  case TextTransform::kUppercase:
    return caseMap(text, u_strToUpper, 0) ? std::u16string_view(scratch)
                                          : text;
  case TextTransform::kLowercase:
    return caseMap(text, u_strToLower, 0) ? std::u16string_view(scratch)
                                          : text;
  case TextTransform::kCapitalize: {
    if (!segmentStartsWord)
      return text;
    // Titlecase exactly the first code point; the remainder is untouched
    // (u_strToTitle over the whole slice would lowercase it).
    int32_t firstEnd = 0;
    UChar32 firstCodePoint;
    U16_NEXT(text.data(), firstEnd, static_cast<int32_t>(text.size()),
             firstCodePoint);
    static_cast<void>(firstCodePoint);
    auto titleFirst = [](UChar *dest, int32_t destCapacity, const UChar *src,
                         int32_t srcLength, const char *mapLocale,
                         UErrorCode *status) {
      return u_strToTitle(dest, destCapacity, src, srcLength,
                          /*titleIter=*/nullptr, mapLocale, status);
    };
    if (!caseMap(text.substr(0, static_cast<size_t>(firstEnd)), titleFirst, 0))
      return text;
    scratch.append(text.substr(static_cast<size_t>(firstEnd)));
    return scratch;
  }
  case TextTransform::kNone:
    break;
  }
  return text;
}

bool isHardLineBreakCharacter(char16_t character) {
  return character == u'\n' || character == u'\r' || character == 0x0085 ||
         character == 0x2028 || character == 0x2029;
}

// Scripts whose break opportunities carry no glue (no spaces between words).
bool isIdeographicScript(UScriptCode script) {
  switch (script) {
  case USCRIPT_HAN:
  case USCRIPT_HIRAGANA:
  case USCRIPT_KATAKANA:
  case USCRIPT_KATAKANA_OR_HIRAGANA:
  case USCRIPT_HANGUL:
  case USCRIPT_BOPOMOFO:
  case USCRIPT_YI:
    return true;
  default:
    return false;
  }
}

// Codepoints that never trigger a font switch: they inherit the run's
// typeface (marks, joiners, variation selectors, whitespace, controls).
bool codePointInheritsTypeface(UChar32 codePoint) {
  if (codePoint == 0x200D /*ZWJ*/ || codePoint == 0x200C /*ZWNJ*/)
    return true;
  if (codePoint >= 0xFE00 && codePoint <= 0xFE0F) // variation selectors
    return true;
  if (u_isUWhiteSpace(codePoint))
    return true;
  const int8_t type = u_charType(codePoint);
  return type == U_NON_SPACING_MARK || type == U_ENCLOSING_MARK ||
         type == U_COMBINING_SPACING_MARK || type == U_CONTROL_CHAR ||
         type == U_FORMAT_CHAR;
}

ScriptTag harfBuzzScriptFor(UScriptCode script) {
  if (script <= USCRIPT_INHERITED || script >= USCRIPT_CODE_LIMIT)
    return static_cast<ScriptTag>(HB_SCRIPT_COMMON); // hb falls back to DFLT
  // hb_script_from_string re-parses a 4-char tag every call; memoize the
  // whole (small, dense) UScriptCode space once.
  static const auto scriptTable = [] {
    std::array<ScriptTag, USCRIPT_CODE_LIMIT> scripts{};
    for (int scriptIndex = 0; scriptIndex < USCRIPT_CODE_LIMIT; ++scriptIndex) {
      const char *name =
          uscript_getShortName(static_cast<UScriptCode>(scriptIndex));
      scripts[static_cast<size_t>(scriptIndex)] = static_cast<ScriptTag>(
          name ? hb_script_from_string(name, -1) : HB_SCRIPT_COMMON);
    }
    return scripts;
  }();
  return scriptTable[static_cast<size_t>(script)];
}

// Codepoints that can force right-to-left directionality: the RTL script
// blocks plus the explicit RTL controls. When a paragraph has none, the
// whole UBiDi pass is skipped.
bool mayRequireBidirectionalAnalysis(UChar32 codePoint) {
  if (codePoint < 0x0590)
    return false;
  if (codePoint <= 0x08FF) // Hebrew, Arabic, Syriac, Thaana, NKo, ...
    return true;
  if (codePoint == 0x200F || codePoint == 0x202B || codePoint == 0x202E ||
      codePoint == 0x2067)
    return true; // RLM / RLE / RLO / RLI
  return (codePoint >= 0xFB1D && codePoint <= 0xFDFF) ||
         (codePoint >= 0xFE70 && codePoint <= 0xFEFF) ||
         (codePoint >= 0x10800 && codePoint <= 0x10FFF) ||
         (codePoint >= 0x1E800 && codePoint <= 0x1EFFF);
}

// Placement form of one codepoint in a vertical paragraph: the span's
// explicit override, or UTR#50's per-character vertical orientation (CJK
// upright, Latin rotated — the CSS text-orientation:mixed behaviour).
SegmentForm resolveVerticalForm(const ShapingStyle &shaping,
                                UChar32 codePoint) {
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
  const int32_t orientation =
      u_getIntPropertyValue(codePoint, UCHAR_VERTICAL_ORIENTATION);
  return orientation == U_VO_ROTATED ? SegmentForm::kRotated
                                     : SegmentForm::kUpright;
}

struct ScriptRun {
  uint32_t end = 0;
  UScriptCode script = USCRIPT_COMMON;
};

// Classic script itemization: COMMON/INHERITED characters attach to the
// preceding real script (or the following one at the start of the text).
// Piggybacks RTL detection on the same codepoint walk so the (expensive)
// UBiDi pass can be skipped for the overwhelmingly common LTR-only case.
// Fills a caller-owned `runs` so per-analyze allocation amortizes away.
void itemizeScripts(const std::u16string &text, bool &hasRightToLeftText,
                    std::vector<ScriptRun> &scriptRuns) {
  scriptRuns.clear();
  const int32_t textLength = static_cast<int32_t>(text.size());
  UScriptCode currentScript = USCRIPT_COMMON;
  hasRightToLeftText = false;
  int32_t codeUnitOffset = 0;
  while (codeUnitOffset < textLength) {
    const int32_t codePointStart = codeUnitOffset;
    UChar32 codePoint;
    U16_NEXT(text.data(), codeUnitOffset, textLength, codePoint);
    hasRightToLeftText |= mayRequireBidirectionalAnalysis(codePoint);
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(codePoint, &status);
    if (U_FAILURE(status))
      script = USCRIPT_COMMON;
    if (script <= USCRIPT_INHERITED)
      continue; // stays in the current run whatever it is
    if (currentScript <= USCRIPT_INHERITED) {
      currentScript = script; // leading common adopts the first real script
    } else if (script != currentScript) {
      scriptRuns.push_back(
          {static_cast<uint32_t>(codePointStart), currentScript});
      currentScript = script;
    }
  }
  scriptRuns.push_back({static_cast<uint32_t>(textLength), currentScript});
}

} // namespace

void Paragraph::recordEdit(uint32_t start, uint32_t removedLength,
                           uint32_t insertedLength) {
  ++m_revision;
  constexpr size_t kMaxHistory = 256;
  if (m_editHistory.size() >= kMaxHistory) {
    m_editHistory.erase(m_editHistory.begin(),
                        m_editHistory.begin() +
                            static_cast<long>(kMaxHistory / 2));
    m_editHistoryBaseRevision += kMaxHistory / 2;
  }
  m_editHistory.push_back({start, removedLength, insertedLength});
}

bool Paragraph::editsSince(uint64_t sinceRevision,
                           std::vector<TextEdit> &edits) const {
  if (sinceRevision > m_revision || sinceRevision < m_editHistoryBaseRevision)
    return false;
  for (uint64_t revision = sinceRevision; revision < m_revision; ++revision)
    edits.push_back(m_editHistory[static_cast<size_t>(
        revision - m_editHistoryBaseRevision)]);
  return true;
}

void Paragraph::clear() {
  if (!m_text.empty())
    recordEdit(0, static_cast<uint32_t>(m_text.size()), 0);
  m_text.clear();
  m_spans.clear();
  m_words.clear();
  m_placeholders.clear();
  markDirty();
}

void Paragraph::appendPlaceholder(const Placeholder &placeholder,
                                  const TextStyle &style) {
  m_placeholders.push_back(placeholder);
  appendText(std::u16string_view(u"\uFFFC"), style);
}

void Paragraph::setPlaceholder(size_t index, const Placeholder &placeholder) {
  if (index >= m_placeholders.size())
    return;
  m_placeholders[index] = placeholder;
  markDirty();
}

void Paragraph::appendText(std::u8string_view utf8, const TextStyle &style) {
  appendText(std::u16string_view(utf8ToUtf16(utf8)), style);
}

void Paragraph::setWritingMode(WritingMode mode) {
  if (m_writingMode == mode)
    return;
  m_writingMode = mode;
  markDirty();
}

void Paragraph::appendText(std::u16string_view utf16, const TextStyle &style) {
  if (utf16.empty())
    return;
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
      [](const StyleSpan &a, const StyleSpan &b) { return a.start < b.start; });
  // Drop empties, clamp to text, and fill any gaps with the previous span's
  // style so every position resolves to exactly one span.
  std::vector<StyleSpan> normalizedSpans;
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());
  uint32_t cursor = 0;
  for (StyleSpan &span : m_spans) {
    span.start = std::min(span.start, textLength);
    span.end = std::min(span.end, textLength);
    if (span.end <= span.start)
      continue;
    if (span.start > cursor) {
      const TextStyle &fillStyle =
          normalizedSpans.empty() ? span.style : normalizedSpans.back().style;
      normalizedSpans.push_back({cursor, span.start, fillStyle});
    }
    if (span.start < cursor)
      span.start = cursor; // overlapping spans: later one yields
    if (span.end <= span.start)
      continue;
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
  const std::u16string insertedText = utf8ToUtf16(utf8);
  const uint32_t insertedLength = static_cast<uint32_t>(insertedText.size());

  // Style the inserted range like the text at the edit point.
  TextStyle insertedStyle;
  for (const StyleSpan &span : m_spans) {
    if (span.start <= start && (start < span.end || span.end == textLength)) {
      insertedStyle = span.style;
      break;
    }
  }
  if (m_spans.empty())
    insertedStyle = TextStyle{};

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
  for (StyleSpan &span : m_spans) {
    span.start = remapPosition(span.start, false);
    span.end = remapPosition(span.end, true);
  }
  if (insertedLength > 0)
    m_spans.push_back({start, start + insertedLength, insertedStyle});
  recordEdit(start, deletedLength, insertedLength);
  normalizeSpans();
  markDirty();
}

void Paragraph::setStyle(uint32_t start, uint32_t end, const TextStyle &style) {
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());
  start = std::min(start, textLength);
  end = std::min(std::max(end, start), textLength);
  if (start == end)
    return;

  std::vector<StyleSpan> updatedSpans;
  updatedSpans.reserve(m_spans.size() + 2);
  for (const StyleSpan &span : m_spans) {
    if (span.end <= start || span.start >= end) {
      updatedSpans.push_back(span);
      continue;
    }
    if (span.start < start)
      updatedSpans.push_back({span.start, start, span.style});
    if (span.end > end)
      updatedSpans.push_back({end, span.end, span.style});
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
std::vector<std::pair<uint32_t, uint32_t>>
spanBoundaries(const std::vector<StyleSpan> &spans) {
  std::vector<std::pair<uint32_t, uint32_t>> boundaries;
  boundaries.reserve(spans.size());
  for (const StyleSpan &span : spans)
    boundaries.push_back({span.start, span.end});
  return boundaries;
}

bool sameSpanBoundaries(
    const std::vector<std::pair<uint32_t, uint32_t>> &previousBoundaries,
    const std::vector<StyleSpan> &spans) {
  if (previousBoundaries.size() != spans.size())
    return false;
  for (size_t spanIndex = 0; spanIndex < previousBoundaries.size(); ++spanIndex)
    if (previousBoundaries[spanIndex].first != spans[spanIndex].start ||
        previousBoundaries[spanIndex].second != spans[spanIndex].end)
      return false;
  return true;
}

} // namespace

void Paragraph::setPaint(uint32_t start, uint32_t end,
                         const PaintStyle &paint) {
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());
  start = std::min(start, textLength);
  end = std::min(std::max(end, start), textLength);
  if (start == end)
    return;

  const std::vector<std::pair<uint32_t, uint32_t>> previousBoundaries =
      spanBoundaries(m_spans);
  std::vector<StyleSpan> updatedSpans;
  updatedSpans.reserve(m_spans.size() + 2);
  for (const StyleSpan &span : m_spans) {
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
    if (span.end > end)
      updatedSpans.push_back({end, span.end, span.style});
  }
  m_spans = std::move(updatedSpans);
  normalizeSpans();
  // Shaping keys and the text itself are untouched, so the words/scripts/
  // bidi analysis stands — at most the shaped prefix's span indices need
  // re-deriving (see reshapeShapedPrefix). When even the boundaries came
  // out unchanged (repainting the same ranges), the indices are already
  // right and nothing at all is dirty: draws just read the new paint.
  if (!sameSpanBoundaries(previousBoundaries, m_spans))
    markPaintDirty();
}

void Paragraph::setPaint(std::span<const CharRange> ranges,
                         const PaintStyle &paint) {
  const uint32_t textLength = static_cast<uint32_t>(m_text.size());

  // Sanitize into sorted, clamped, non-overlapping ranges.
  std::vector<CharRange> sanitizedRanges;
  sanitizedRanges.reserve(ranges.size());
  for (const CharRange &range : ranges) {
    CharRange sanitizedRange{std::min(range.start, textLength),
                             std::min(range.end, textLength)};
    if (!sanitizedRange.empty())
      sanitizedRanges.push_back(sanitizedRange);
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
  if (!sanitizedRanges.empty())
    sanitizedRanges.resize(mergedRangeIndex + 1);
  if (sanitizedRanges.empty())
    return;

  const std::vector<std::pair<uint32_t, uint32_t>> previousBoundaries =
      spanBoundaries(m_spans);
  // One pass over spans and ranges together: each output span is either an
  // untouched piece or a painted intersection, so the whole batch costs
  // O(spans + ranges) instead of one full rebuild per range.
  std::vector<StyleSpan> updatedSpans;
  updatedSpans.reserve(m_spans.size() + 2 * sanitizedRanges.size());
  size_t rangeIndex = 0;
  for (const StyleSpan &span : m_spans) {
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
  if (!sameSpanBoundaries(previousBoundaries, m_spans))
    markPaintDirty();
}

Paragraph::Strut Paragraph::strut(FontContext &fontContext) const {
  ShapingStyle shaping;
  if (!m_spans.empty())
    shaping = m_spans.front().style.shaping;
  sk_sp<SkTypeface> typeface =
      fontContext.variedTypeface(shaping.typeface, shaping.variations);
  const SkFont font = makeFont(typeface, shaping.fontSize);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  Strut strut;
  strut.ascent = -metrics.fAscent;
  strut.height = -metrics.fAscent + metrics.fDescent + metrics.fLeading;
  return strut;
}

float Paragraph::naturalWidth(FontContext &fontContext) {
  ensureShaped(fontContext);
  float width = 0;
  for (size_t wordIndex = 0; wordIndex < m_words.size(); ++wordIndex) {
    width += m_words[wordIndex].width;
    if (wordIndex + 1 < m_words.size())
      width += m_words[wordIndex].spaceWidth;
  }
  return width;
}

void Paragraph::ensureAnalyzed(FontContext &fontContext) {
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
void Paragraph::reshapeShapedPrefix(FontContext &fontContext) {
  const uint32_t previouslyShapedWordCount = m_shapedWordCount;
  m_shapedWordCount = 0;
  m_shapingSpanCursor = 0;
  m_shapingScriptCursor = 0;
  m_cachedWhitespaceStyleIndex = ~0u;
  m_cachedWhitespaceText.clear();
  for (; m_shapedWordCount < previouslyShapedWordCount; ++m_shapedWordCount) {
    Word &word = m_words[m_shapedWordCount];
    if (word.placeholderIndex >= 0)
      continue; // slots carry no glyphs; width comes from the record
    word.segments.clear();
    word.width = 0;
    word.spaceWidth = 0;
    shapeWordContent(fontContext, word);
  }
}

void Paragraph::ensureShapedTo(FontContext &fontContext, uint32_t wordCount) {
  ensureAnalyzed(fontContext);
  const uint32_t upTo =
      std::min(wordCount, static_cast<uint32_t>(m_words.size()));
  for (; m_shapedWordCount < upTo; ++m_shapedWordCount)
    shapeWordContent(fontContext, m_words[m_shapedWordCount]);
}

void Paragraph::ensureShaped(FontContext &fontContext) {
  ensureAnalyzed(fontContext);
  ensureShapedTo(fontContext, static_cast<uint32_t>(m_words.size()));
}

void Paragraph::analyze(FontContext &fontContext) {
  m_words.clear();
  if (m_text.empty())
    return;

  const UChar *text = reinterpret_cast<const UChar *>(m_text.data());
  const int32_t textLength = static_cast<int32_t>(m_text.size());
  UErrorCode status = U_ZERO_ERROR;

  // ── Line-break opportunities (UAX #14 via ICU) ─────────────────────────
  FontContext::Impl &implementation = fontContext.impl();
  if (!implementation.lineBreakIterator) {
    implementation.lineBreakIterator =
        ubrk_open(UBRK_LINE, "", text, textLength, &status);
  } else {
    ubrk_setText(implementation.lineBreakIterator, text, textLength, &status);
  }
  if (U_FAILURE(status) || !implementation.lineBreakIterator)
    return;

  // Scratch buffers persist across analyses (one FontContext == one thread
  // by contract, so thread_local matches the ownership model).
  static thread_local std::vector<int32_t> boundaries;
  boundaries.clear();
  for (int32_t boundary = ubrk_next(implementation.lineBreakIterator);
       boundary != UBRK_DONE;
       boundary = ubrk_next(implementation.lineBreakIterator))
    boundaries.push_back(boundary);
  if (boundaries.empty() || boundaries.back() != textLength)
    boundaries.push_back(textLength);

  // ── Script runs (also detects whether bidi is needed at all) ──────────
  bool hasRightToLeftText = false;
  static thread_local std::vector<ScriptRun> scriptRuns;
  itemizeScripts(m_text, hasRightToLeftText, scriptRuns);

  // ── Bidi levels (skipped entirely for LTR-only text) ──────────────────
  const UBiDiLevel *bidiLevels = nullptr;
  uint8_t uniformLevel = 0; // used when the whole paragraph is one direction
  if (hasRightToLeftText) {
    if (!implementation.bidirectionalAnalyzer)
      implementation.bidirectionalAnalyzer =
          ubidi_open(); // reused: setPara re-targets it per analyze
    if (implementation.bidirectionalAnalyzer) {
      status = U_ZERO_ERROR;
      ubidi_setPara(implementation.bidirectionalAnalyzer, text, textLength,
                    UBIDI_DEFAULT_LTR, nullptr, &status);
      if (U_SUCCESS(status)) {
        const UBiDiDirection direction =
            ubidi_getDirection(implementation.bidirectionalAnalyzer);
        if (direction == UBIDI_MIXED)
          bidiLevels =
              ubidi_getLevels(implementation.bidirectionalAnalyzer, &status);
        else if (direction == UBIDI_RTL)
          uniformLevel = 1;
      }
      if (U_FAILURE(status))
        bidiLevels = nullptr;
    }
  }

  // ── Words: one per break segment (segment/shape runs resolved lazily) ──
  m_words.reserve(boundaries.size());
  int placeholdersSeen = 0;
  int32_t segmentStart = 0;
  for (int32_t boundary : boundaries) {
    if (boundary <= segmentStart)
      continue;

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
      continue; // the segment was nothing but slots

    // Trailing whitespace (including any hard-break characters at the end).
    int32_t whitespaceStart = boundary;
    bool hardBreak = false;
    while (whitespaceStart > segmentStart) {
      const char16_t character = m_text[whitespaceStart - 1];
      if (isHardLineBreakCharacter(character)) {
        hardBreak = true;
        --whitespaceStart;
      } else if (u_isWhitespace(character)) {
        --whitespaceStart;
      } else {
        break;
      }
    }

    Word word;
    word.textBegin = static_cast<uint32_t>(segmentStart);
    word.whitespaceEnd = static_cast<uint32_t>(boundary);
    word.mandatoryBreakAfter = hardBreak;
    word.bidiLevel = bidiLevels ? bidiLevels[segmentStart] : uniformLevel;

    // A trailing soft hyphen (U+00AD) is a discretionary break: it never
    // shapes, but marks that a hyphen may be rendered if a breaker splits
    // here.
    if (whitespaceStart > segmentStart &&
        m_text[whitespaceStart - 1] == 0x00AD) {
      word.hyphenBreak = true;
      --whitespaceStart;
    }
    word.textEnd = static_cast<uint32_t>(whitespaceStart);

    // Tab-aware layouts (ParagraphLayoutOptions::tabStops) treat the glue
    // after this word as an advance-to-stop instead of measured whitespace.
    for (int32_t codeUnitIndex = whitespaceStart;
         codeUnitIndex < boundary && !word.tabAfter; ++codeUnitIndex)
      word.tabAfter = m_text[static_cast<size_t>(codeUnitIndex)] == u'\t';

    if (whitespaceStart > segmentStart) {
      UChar32 firstCodePoint;
      int32_t codePointEnd = segmentStart;
      U16_NEXT(text, codePointEnd, textLength, firstCodePoint);
      UErrorCode scriptStatus = U_ZERO_ERROR;
      word.ideographic =
          isIdeographicScript(uscript_getScript(firstCodePoint, &scriptStatus));
    }

    // Glyphs, widths, and glue come later: shapeWordContent() fills them in
    // lazily (see ensureShapedTo), so text past the layout frontier never
    // pays for HarfBuzz.
    m_words.push_back(std::move(word));
    segmentStart = boundary;
  }

  // Persist what lazy shaping needs: script runs (compact, ICU-free form)
  // and per-unit bidi levels (only when actually mixed).
  m_scriptRunEnds.clear();
  m_scriptRunEnds.reserve(scriptRuns.size());
  for (const ScriptRun &run : scriptRuns)
    m_scriptRunEnds.push_back({run.end, static_cast<int32_t>(run.script)});
  m_uniformBidirectionalLevel = uniformLevel;
  if (bidiLevels)
    m_bidirectionalLevels.assign(bidiLevels, bidiLevels + textLength);
  else
    m_bidirectionalLevels.clear();
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
void Paragraph::shapeWordContent(FontContext &fontContext, Word &word) {
  if (word.placeholderIndex >= 0)
    return; // slots carry their size from the placeholder record

  const UChar *text = reinterpret_cast<const UChar *>(m_text.data());
  const int32_t textLength = static_cast<int32_t>(m_text.size());

  auto styleIndexAt = [&](uint32_t position) -> uint32_t {
    while (m_shapingSpanCursor + 1 < m_spans.size() &&
           m_spans[m_shapingSpanCursor].end <= position)
      ++m_shapingSpanCursor;
    return m_shapingSpanCursor;
  };
  auto scriptAt = [&](uint32_t position) -> const ScriptRunEnd & {
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
    const StyleSpan &span = m_spans[styleIndex];
    const ScriptRunEnd &scriptRun =
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
    const char *languageTag = span.style.shaping.languageTag.empty()
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
    while (codeUnitOffset < segmentLimit) {
      const int32_t codePointStart = codeUnitOffset;
      UChar32 codePoint;
      U16_NEXT(text, codeUnitOffset, segmentLimit, codePoint);
      if (codePointInheritsTypeface(codePoint)) {
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
      sk_sp<SkTypeface> codePointTypeface =
          fontContext.resolveTypeface(primaryTypeface, codePoint, languageTag);
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
    const std::u16string_view segmentText =
        applyTextTransform(span.style.shaping,
                           std::u16string_view(m_text).substr(
                               static_cast<size_t>(segmentStart),
                               static_cast<size_t>(segmentEnd - segmentStart)),
                           segmentStart == static_cast<int32_t>(word.textBegin),
                           transformScratch);
    ShapedWordRef shapedWord =
        shapeWord(fontContext, span.style.shaping, resolvedTypeface,
                  segmentText,
                  harfBuzzScriptFor(static_cast<UScriptCode>(scriptRun.script)),
                  (bidiLevel & 1) != 0 && !shapeVertical, shapeVertical);
    if (verticalMode && segmentForm == SegmentForm::kTateChuYoko) {
      // 縦中横 occupies its font height along the column; advanceOffset lands
      // on the run's baseline inside that box so placement needs no metrics.
      const SkFont font =
          makeFont(resolvedTypeface, span.style.shaping.fontSize);
      SkFontMetrics fontMetrics;
      font.getMetrics(&fontMetrics);
      word.segments.push_back({shapedWord, styleIndex,
                               word.width - fontMetrics.fAscent, segmentForm});
      word.width += -fontMetrics.fAscent + fontMetrics.fDescent;
    } else {
      word.segments.push_back(
          {shapedWord, styleIndex, word.width, segmentForm});
      word.width += shapedWord->advance;
    }
    segmentStart = segmentEnd;
  }

  if (word.hyphenBreak) {
    // Shape the hyphen the breaker may render here, in the style of the
    // word's tail. Content-addressed like everything else: one entry per
    // style, shared by every hyphenatable word.
    const uint32_t styleIndex = word.segments.empty()
                                    ? styleIndexAt(word.textBegin)
                                    : word.segments.back().styleIndex;
    const StyleSpan &span = m_spans[styleIndex];
    sk_sp<SkTypeface> typeface = fontContext.variedTypeface(
        span.style.shaping.typeface, span.style.shaping.variations);
    word.hyphenGlyph =
        shapeWord(fontContext, span.style.shaping, typeface, u"-",
                  static_cast<ScriptTag>(HB_SCRIPT_COMMON), false);
  }

  // Trailing whitespace becomes justification glue. Hard-break characters
  // are zero-width; tabs measure as spaces (documented simplification).
  // The overwhelmingly common glue (" " in the same style as the previous
  // word) is memoized, skipping even the shape-cache probe.
  const int32_t whitespaceEnd = static_cast<int32_t>(word.whitespaceEnd);
  if (whitespaceStart < whitespaceEnd) {
    static thread_local std::u16string whitespaceScratch;
    whitespaceScratch.clear();
    bool needsScratch = false;
    for (int32_t codeUnitIndex = whitespaceStart; codeUnitIndex < whitespaceEnd;
         ++codeUnitIndex) {
      const char16_t character = m_text[static_cast<size_t>(codeUnitIndex)];
      if (isHardLineBreakCharacter(character) || character == u'\t') {
        needsScratch = true;
        break;
      }
    }
    std::u16string_view whitespace;
    if (needsScratch) {
      for (int32_t codeUnitIndex = whitespaceStart;
           codeUnitIndex < whitespaceEnd; ++codeUnitIndex) {
        const char16_t character = m_text[static_cast<size_t>(codeUnitIndex)];
        if (isHardLineBreakCharacter(character))
          continue;
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
        const StyleSpan &span = m_spans[styleIndex];
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

} // namespace sigil::weave
