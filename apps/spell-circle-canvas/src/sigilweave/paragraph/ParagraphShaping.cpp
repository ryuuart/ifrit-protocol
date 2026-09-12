/** @file
 * Shaping one word's content: the case mapping a style asks for, the
 * upright, rotated and tate-chu-yoko forms a vertical setting puts a
 * segment in, and the run of segments a word is shaped into.
 */

#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/paragraph/Paragraph.h"
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
    ShapedWordReference shapedWord =
        shapeWord(fontContext, span.style.shaping, resolvedTypeface,
                  segmentText, unicode::shaperScript(scriptRun.script),
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
                  unicode::shaperScript(unicode::kCommonScript), false);
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
        ShapedWordReference shapedWhitespace = shapeWord(
            fontContext, span.style.shaping, whitespaceTypeface, whitespace,
            unicode::shaperScript(unicode::kCommonScript), false,
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
