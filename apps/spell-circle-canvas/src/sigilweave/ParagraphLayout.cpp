#include "sigilweave/ParagraphLayout.h"
#include "ParagraphLayoutInternal.h"
#include "sigilweave/FontContext.h"
#include "sigilweave/Shaper.h"

#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkTextBlob.h>

#include <hb.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

namespace sigil::weave {

namespace detail {

// tabStopsActive() and glueAfter() are inline in ParagraphLayoutInternal.h:
// shared with KnuthPlass.cpp, hot in both breakers.

namespace {

constexpr float kFitEpsilon = 0.25f;
// After this many consecutive intervals rejected a word outright, it is
// force-placed (overflowing) rather than skipping arbitrarily far down the
// geometry — matches browser overflow behavior for unbreakably-wide content.
constexpr int kMaxIntervalSkips = 12;

enum class GapKind : uint8_t { kRigid, kSpace, kIdeographic };

/** Classifies the gap after one logical word for justification. */
GapKind gapKind(const std::vector<Word> &words, uint32_t wordIndex,
                const ParagraphLayoutOptions &options) {
  if (words[wordIndex].tabAfter && tabStopsActive(options))
    return GapKind::kRigid; // tab gaps never stretch or shrink
  if (words[wordIndex].spaceWidth > 0)
    return GapKind::kSpace;
  if (options.justification.expandIdeographicGaps &&
      (words[wordIndex].ideographic || words[wordIndex + 1].ideographic))
    return GapKind::kIdeographic;
  return GapKind::kRigid;
}

/** Returns a word's em size, including a safe default for placeholders. */
float wordFontSize(const Word &word) {
  return word.segments.empty() ? 16.0f : word.segments.front().shaped->fontSize;
}

// Snaps a unit tangent to one of `steps` directions (512 → 0.7° steps —
// invisible). Continuously varying per-glyph rotations would otherwise mint
// a fresh glyph-atlas strike every frame for every glyph on a moving path,
// turning animated curved text into a per-frame mask-rasterization storm.
// See ParagraphLayoutOptions::pathText.tangentRotationSteps.
SkVector quantizeTangent(SkVector tangent, int directionCount) {
  if (directionCount <= 0)
    return tangent;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  const float angle = std::atan2(tangent.fY, tangent.fX);
  int directionIndex =
      static_cast<int>(std::lround(angle / kTwoPi * directionCount)) %
      directionCount;
  if (directionIndex < 0)
    directionIndex += directionCount;
  const float snapped =
      static_cast<float>(directionIndex) * kTwoPi / directionCount;
  return {std::cos(snapped), std::sin(snapped)};
}

// Per-glyph RSXform blob for rotated straight intervals and path contours.
sk_sp<SkTextBlob> buildTransformedBlob(const ShapedWord &shapedWord,
                                       const LineInterval &interval,
                                       float penOffset, int rotationSteps) {
  if (shapedWord.glyphs.empty())
    return nullptr;
  SkTextBlobBuilder builder;
  const SkFont font =
      makeFont(shapedWord.typeface, shapedWord.fontSize, shapedWord.scaleX);
  const int glyphCount = static_cast<int>(shapedWord.glyphs.size());
  const auto &run = builder.allocRunRSXform(font, glyphCount);

  const float contourLength = interval.contour ? interval.contour->length() : 0;
  float penLocal = 0;
  for (int glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
    const float advance = shapedWord.advances[glyphIndex];
    // Offsets HarfBuzz applied on top of the pen position.
    const float glyphOffsetX = shapedWord.positions[glyphIndex].x() - penLocal;
    const float glyphOffsetY = shapedWord.positions[glyphIndex].y();

    SkPoint position;
    SkVector tangent;
    if (interval.contour) {
      float contourPosition =
          interval.contourStart +
          (penOffset + penLocal + advance * 0.5f) * interval.advanceScale;
      if (interval.contour->isClosed()) {
        // Closed contours wrap: text can march around the loop forever
        // (animate contourStart for an infinite marquee).
        contourPosition = std::fmod(contourPosition, contourLength);
        if (contourPosition < 0)
          contourPosition += contourLength;
      } else {
        contourPosition = std::clamp(contourPosition, 0.0f, contourLength);
      }
      if (!interval.contour->getPosTan(contourPosition, &position, &tangent)) {
        position = {0, 0};
        tangent = {1, 0};
      }
      // Rotation snaps; position stays exact.
      tangent = quantizeTangent(tangent, rotationSteps);
    } else {
      tangent = interval.direction;
      const float distance = penOffset + penLocal + advance * 0.5f;
      position = interval.origin +
                 SkVector{tangent.x() * distance, tangent.y() * distance};
      tangent = quantizeTangent(tangent, rotationSteps);
    }

    // Anchor the glyph's advance-center on the baseline point `pos`,
    // rotated to the local tangent. Center in glyph-local coordinates:
    const float glyphCenterX = advance * 0.5f - glyphOffsetX;
    const float glyphCenterY = -glyphOffsetY;
    run.glyphs[glyphIndex] = shapedWord.glyphs[glyphIndex];
    run.xforms()[glyphIndex] = {tangent.x(), tangent.y(),
                                position.x() - (tangent.x() * glyphCenterX -
                                                tangent.y() * glyphCenterY),
                                position.y() - (tangent.y() * glyphCenterX +
                                                tangent.x() * glyphCenterY)};
    penLocal += advance;
  }
  return builder.make();
}

/** Appends one shaped segment at its final straight or transformed position. */
void emitSegment(ParagraphLayout &result, const FlatInterval &flatInterval,
                 const WordSegment &segment, uint32_t wordIndex,
                 float penOffset, const ParagraphLayoutOptions &options) {
  const ShapedWord &shapedWord = *segment.shaped;
  if (shapedWord.glyphs.empty())
    return;
  PositionedRun run;
  run.shaped = segment.shaped;
  run.styleIndex = segment.styleIndex;
  run.wordIndex = wordIndex;
  run.lineIndex = flatInterval.sourceLineIndex;
  const bool straight = !flatInterval.interval.contour;
  const bool horizontal = straight &&
                          flatInterval.interval.direction.x() == 1 &&
                          flatInterval.interval.direction.y() == 0 &&
                          segment.form == SegmentForm::kFlow;
  const bool verticalColumn = straight &&
                              flatInterval.interval.direction.x() == 0 &&
                              flatInterval.interval.direction.y() == 1;
  if (horizontal) {
    run.blob = wordBlob(shapedWord);
    run.origin = flatInterval.interval.origin + SkVector{penOffset, 0};
  } else if (verticalColumn && segment.form == SegmentForm::kUpright) {
    // Vertical-shaped word: positions already stack down the column.
    run.blob = wordBlob(shapedWord);
    run.origin = flatInterval.interval.origin + SkVector{0, penOffset};
  } else if (verticalColumn && segment.form == SegmentForm::kTateChuYoko) {
    // Horizontal run set upright across the column, centred on its axis;
    // penX already points at the run's baseline (see Paragraph::analyze).
    run.blob = wordBlob(shapedWord);
    run.origin = flatInterval.interval.origin +
                 SkVector{-shapedWord.advance * 0.5f, penOffset};
  } else {
    // Rotated/curved: bake per-glyph transforms (kRotated Latin in a
    // vertical column rotates 90° clockwise here via the interval tangent).
    run.blob =
        buildTransformedBlob(shapedWord, flatInterval.interval, penOffset,
                             options.pathText.tangentRotationSteps);
    run.origin = {0, 0};
    run.transformed = true;
  }
  if (run.blob)
    result.runs.push_back(std::move(run));
}

// UAX #9 rule L2 over per-word levels: reverse maximal runs of every level
// >= each odd level, highest level first.
void visualOrder(const std::vector<Word> &words, uint32_t firstWordIndex,
                 uint32_t endWordIndex,
                 std::vector<uint32_t> &visualWordOrder) {
  visualWordOrder.clear();
  visualWordOrder.reserve(endWordIndex - firstWordIndex);
  uint8_t maximumLevel = 0;
  uint8_t minimumOddLevel = 255;
  for (uint32_t wordIndex = firstWordIndex; wordIndex < endWordIndex;
       ++wordIndex) {
    visualWordOrder.push_back(wordIndex);
    const uint8_t level = words[wordIndex].bidiLevel;
    maximumLevel = std::max(maximumLevel, level);
    if (level & 1)
      minimumOddLevel = std::min(minimumOddLevel, level);
  }
  for (uint8_t level = maximumLevel;
       level >= minimumOddLevel && minimumOddLevel != 255; --level) {
    size_t rangeStart = 0;
    while (rangeStart < visualWordOrder.size()) {
      if (words[visualWordOrder[rangeStart]].bidiLevel >= level) {
        size_t rangeEnd = rangeStart;
        while (rangeEnd < visualWordOrder.size() &&
               words[visualWordOrder[rangeEnd]].bidiLevel >= level)
          rangeEnd++;
        std::reverse(visualWordOrder.begin() + rangeStart,
                     visualWordOrder.begin() + rangeEnd);
        rangeStart = rangeEnd;
      } else {
        rangeStart++;
      }
    }
  }
}

} // namespace

float naturalWidth(const std::vector<Word> &words, uint32_t firstWordIndex,
                   uint32_t endWordIndex) {
  float width = 0;
  for (uint32_t wordIndex = firstWordIndex; wordIndex < endWordIndex;
       ++wordIndex) {
    width += words[wordIndex].width;
    if (wordIndex + 1 < endWordIndex)
      width += words[wordIndex].spaceWidth;
  }
  return width;
}

void placeWords(const std::vector<Word> &words, uint32_t firstWordIndex,
                uint32_t endWordIndex, const FlatInterval &flatInterval,
                TextAlignment alignment, bool lastLine, bool hyphenBreakTaken,
                const ParagraphLayoutOptions &options,
                ParagraphLayout &result) {
  if (firstWordIndex >= endWordIndex)
    return;

  const float hyphenWidth =
      hyphenBreakTaken ? words[endWordIndex - 1].hyphenGlyph->advance : 0.0f;

  // Visual reordering (no-op for pure-LTR lines). Computed up front because
  // tab resolution follows pen order, not logical order.
  static thread_local std::vector<uint32_t> visualWordOrder;
  visualOrder(words, firstWordIndex, endWordIndex, visualWordOrder);

  // Tab gaps pin the pen to absolute stops, so a tabbed line's width can
  // only be known by walking it. The same walk finds the last tab gap:
  // justification must ignore every gap at or before it — the following
  // stop would swallow any adjustment (and overshooting a stop would break
  // the column) — so only the gaps past the last tab absorb slack.
  int lastTabVisualIndex = -1; // visual index of the word before the gap
  float resolvedNaturalWidth = 0;
  if (tabStopsActive(options)) {
    float pen = 0;
    for (size_t visualIndex = 0; visualIndex < visualWordOrder.size();
         ++visualIndex) {
      const Word &word = words[visualWordOrder[visualIndex]];
      pen += word.width;
      if (visualIndex + 1 < visualWordOrder.size()) {
        if (word.tabAfter)
          lastTabVisualIndex = static_cast<int>(visualIndex);
        pen += glueAfter(word, pen, options);
      }
    }
    resolvedNaturalWidth = pen + hyphenWidth;
  }
  const bool hasTab = lastTabVisualIndex >= 0;

  // Gap census for justification (tabbed lines: only gaps past the last
  // tab), plus the measured glue behind the census for the shrink limit.
  int spaceGapCount = 0;
  int ideographicGapCount = 0;
  float stretchableGlue = 0;
  if (hasTab) {
    for (size_t visualIndex = static_cast<size_t>(lastTabVisualIndex) + 1;
         visualIndex + 1 < visualWordOrder.size(); ++visualIndex) {
      const uint32_t gapWordIndex = std::min(
          visualWordOrder[visualIndex], visualWordOrder[visualIndex + 1]);
      switch (gapKind(words, gapWordIndex, options)) {
      case GapKind::kSpace:
        spaceGapCount++;
        break;
      case GapKind::kIdeographic:
        ideographicGapCount++;
        break;
      case GapKind::kRigid:
        break;
      }
      stretchableGlue += words[gapWordIndex].spaceWidth;
    }
  } else {
    for (uint32_t wordIndex = firstWordIndex; wordIndex + 1 < endWordIndex;
         ++wordIndex) {
      switch (gapKind(words, wordIndex, options)) {
      case GapKind::kSpace:
        spaceGapCount++;
        break;
      case GapKind::kIdeographic:
        ideographicGapCount++;
        break;
      case GapKind::kRigid:
        break;
      }
      stretchableGlue += words[wordIndex].spaceWidth;
    }
  }

  const float naturalLineWidth =
      hasTab ? resolvedNaturalWidth
             : naturalWidth(words, firstWordIndex, endWordIndex) + hyphenWidth;
  const float extraWidth = flatInterval.interval.length - naturalLineWidth;

  TextAlignment resolvedAlignment = alignment;
  if (resolvedAlignment == TextAlignment::kJustify && lastLine &&
      !options.justification.justifyLastLine)
    resolvedAlignment = options.justification.lastLineAlignment;

  float startOffset = 0;
  float spaceAdjustment = 0;
  float ideographicAdjustment = 0;
  switch (resolvedAlignment) {
  case TextAlignment::kStart:
    break;
  case TextAlignment::kCenter:
    startOffset = std::max(0.0f, extraWidth * 0.5f);
    break;
  case TextAlignment::kEnd:
    startOffset = std::max(0.0f, extraWidth);
    break;
  case TextAlignment::kJustify: {
    if (extraWidth > 0 && (spaceGapCount + ideographicGapCount) > 0) {
      const float ideographicExpansionLimit =
          options.justification.maxIdeographicExpansion *
          wordFontSize(words[firstWordIndex]);
      const float equalGapAdjustment =
          extraWidth / static_cast<float>(spaceGapCount + ideographicGapCount);
      if (ideographicGapCount > 0 &&
          equalGapAdjustment > ideographicExpansionLimit) {
        ideographicAdjustment = ideographicExpansionLimit;
        spaceAdjustment =
            spaceGapCount > 0
                ? (extraWidth - ideographicAdjustment *
                                    static_cast<float>(ideographicGapCount)) /
                      static_cast<float>(spaceGapCount)
                : 0; // no spaces to absorb the rest: stay underfull
      } else {
        spaceAdjustment = ideographicAdjustment = equalGapAdjustment;
      }
    } else if (extraWidth < 0 && (spaceGapCount + ideographicGapCount) > 0) {
      // Shrink, but never beyond the shrink limits — a slightly overfull
      // line beats spaces collapsing to nothing. Ideographic gaps compress
      // a touch too, mirroring the breakers' shrink model (em * 0.03), so
      // a break the breaker deemed renderable never leaks past the measure.
      const float spaceShrinkLimit =
          spaceGapCount > 0
              ? stretchableGlue / static_cast<float>(spaceGapCount) *
                    options.justification.spaceShrink
              : 0;
      const float ideographicShrinkLimit =
          0.03f * wordFontSize(words[firstWordIndex]);
      const float capacity =
          spaceShrinkLimit * static_cast<float>(spaceGapCount) +
          ideographicShrinkLimit * static_cast<float>(ideographicGapCount);
      if (capacity > 0) {
        const float shrinkFraction = std::min(1.0f, -extraWidth / capacity);
        spaceAdjustment = -shrinkFraction * spaceShrinkLimit;
        ideographicAdjustment = -shrinkFraction * ideographicShrinkLimit;
      }
    }
    break;
  }
  }

  float penPosition = startOffset;
  for (size_t visualIndex = 0; visualIndex < visualWordOrder.size();
       ++visualIndex) {
    const uint32_t wordIndex = visualWordOrder[visualIndex];
    const Word &word = words[wordIndex];
    for (const WordSegment &segment : word.segments)
      emitSegment(result, flatInterval, segment, wordIndex,
                  penPosition + segment.advanceOffset, options);
    if (word.placeholderIndex >= 0 && !flatInterval.interval.contour) {
      // Inline slot: report where it landed (blob-less run; draw() and
      // drawBatched() skip it, placeholderRects() surfaces it).
      PositionedRun run;
      run.origin = flatInterval.interval.origin +
                   SkVector{flatInterval.interval.direction.x() * penPosition,
                            flatInterval.interval.direction.y() * penPosition};
      run.wordIndex = wordIndex;
      run.lineIndex = flatInterval.sourceLineIndex;
      run.placeholderIndex = word.placeholderIndex;
      result.runs.push_back(std::move(run));
    }
    penPosition += word.width;
    if (hyphenBreakTaken && wordIndex == endWordIndex - 1) {
      // Discretionary break taken: render the hyphen right after the word.
      const uint32_t styleIndex =
          word.segments.empty() ? 0 : word.segments.back().styleIndex;
      emitSegment(result, flatInterval,
                  WordSegment{word.hyphenGlyph, styleIndex, 0}, wordIndex,
                  penPosition, options);
      penPosition += hyphenWidth;
    }
    if (visualIndex + 1 < visualWordOrder.size()) {
      // Glue between visual neighbors; logical == visual for LTR text.
      // (glueAfter == spaceWidth unless the gap is a configured tab stop.)
      // Stops resolve in line-local coordinates — the alignment offset
      // shifts the resolved line as a whole, keeping the line's width the
      // width the breaker and the census computed for it.
      penPosition += glueAfter(word, penPosition - startOffset, options);
      if (static_cast<int>(visualIndex) > lastTabVisualIndex) {
        switch (gapKind(words,
                        std::min(wordIndex, visualWordOrder[visualIndex + 1]),
                        options)) {
        case GapKind::kSpace:
          penPosition += spaceAdjustment;
          break;
        case GapKind::kIdeographic:
          penPosition += ideographicAdjustment;
          break;
        case GapKind::kRigid:
          break;
        }
      }
    }
  }
}

// Overflow marker: trim the final placed line until the configured ellipsis
// fits, then append it as one more run (CSS text-overflow semantics).
// Straight horizontal intervals only — curved/vertical flows just overflow.
void applyEllipsis(FontContext &fontContext, Paragraph &paragraph,
                   IntervalSequence &intervalSequence,
                   const ParagraphLayoutOptions &options,
                   ParagraphLayout &result) {
  if (result.runs.empty())
    return;
  // Overflow means the breakers consumed every interval the geometry had,
  // so the final placed line sits on the last one.
  const FlatInterval *lastInterval = nullptr;
  for (size_t intervalIndex = 0; const FlatInterval *flatInterval =
                                     intervalSequence.intervalAt(intervalIndex);
       ++intervalIndex)
    lastInterval = flatInterval;
  if (!lastInterval || lastInterval->interval.contour ||
      lastInterval->interval.direction.x() != 1 ||
      lastInterval->interval.direction.y() != 0)
    return;

  // Shape the marker in the style of the line's tail (fallback-resolved on
  // its first codepoint; cache-shared like every other word).
  const int lineIndex = result.runs.back().lineIndex;
  const uint32_t styleIndex = result.runs.back().styleIndex;
  const uint32_t tailWord = result.runs.back().wordIndex;
  const StyleSpan &span = paragraph.spans()[styleIndex];
  UChar32 firstCodepoint;
  {
    size_t codeUnitIndex = 0;
    U16_NEXT(options.overflow.ellipsis.data(), codeUnitIndex,
             options.overflow.ellipsis.size(), firstCodepoint);
  }
  const char *languageTag = span.style.shaping.languageTag.empty()
                                ? nullptr
                                : span.style.shaping.languageTag.c_str();
  sk_sp<SkTypeface> typeface = fontContext.resolveTypeface(
      span.style.shaping.typeface, firstCodepoint, languageTag);
  if (!typeface)
    typeface = fontContext.defaultTypeface();
  ShapedWordRef marker = shapeWord(
      fontContext, span.style.shaping, typeface, options.overflow.ellipsis,
      static_cast<ScriptTag>(HB_SCRIPT_COMMON), false);
  if (!marker || marker->glyphs.empty())
    return;

  size_t lineBegin = result.runs.size();
  while (lineBegin > 0 && result.runs[lineBegin - 1].lineIndex == lineIndex)
    lineBegin--;
  auto runEndX = [&](const PositionedRun &run) {
    const float runWidth = run.shaped
                               ? run.shaped->advance
                               : (run.placeholderIndex >= 0
                                      ? paragraph
                                            .placeholders()[static_cast<size_t>(
                                                run.placeholderIndex)]
                                            .width
                                      : 0.0f);
    return run.origin.x() + runWidth;
  };

  // Drop whole trailing words until the marker fits inside the interval.
  const float limit = lastInterval->interval.origin.x() +
                      lastInterval->interval.length - marker->advance + 0.25f;
  while (result.runs.size() > lineBegin &&
         runEndX(result.runs.back()) > limit) {
    const uint32_t trailingWordIndex = result.runs.back().wordIndex;
    while (result.runs.size() > lineBegin &&
           result.runs.back().wordIndex == trailingWordIndex) {
      result.firstUnplacedWord =
          std::min(result.firstUnplacedWord, result.runs.back().wordIndex);
      result.runs.pop_back();
    }
  }

  PositionedRun run;
  run.shaped = marker;
  run.blob = wordBlob(*marker);
  run.styleIndex = styleIndex;
  run.wordIndex = tailWord;
  run.lineIndex = lineIndex;
  if (result.runs.size() > lineBegin)
    run.origin = {runEndX(result.runs.back()), result.runs.back().origin.y()};
  else
    run.origin = {lastInterval->interval.origin.x(),
                  lastInterval->interval.origin.y()};
  result.runs.push_back(std::move(run));
  result.ellipsized = true;
}

/// Clamps any FlowGeometry to its first `maxLines` lines
/// (OverflowOptions::maxLines): geometry "exhausts" at the limit, so the
/// existing overflow reporting and ellipsis machinery handle the rest with
/// zero breaker changes, for greedy and Knuth-Plass alike.
class LineLimitedGeometry final : public FlowGeometry {
public:
  LineLimitedGeometry(FlowGeometry &inner, int maxLines)
      : m_inner(inner), m_maxLines(maxLines) {}

  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &intervals) override {
    return index < m_maxLines &&
           m_inner.lineIntervals(index, lineHeight, ascent, intervals);
  }
  bool uniformIntervals() const override { return m_inner.uniformIntervals(); }

private:
  FlowGeometry &m_inner;
  int m_maxLines;
};

} // namespace detail

ParagraphLayout layoutParagraph(FontContext &fontContext, Paragraph &paragraph,
                                FlowGeometry &geometry,
                                const ParagraphLayoutOptions &options) {
  using namespace detail;

  LineLimitedGeometry clampedGeometry(geometry, options.overflow.maxLines);
  FlowGeometry &effectiveGeometry =
      options.overflow.maxLines > 0 ? static_cast<FlowGeometry &>(clampedGeometry)
                                    : geometry;

  // Segmentation only; the breakers pull HarfBuzz shaping just ahead of
  // their own frontier, so text past the geometry never shapes at all.
  paragraph.ensureAnalyzed(fontContext);

  const Paragraph::Strut strut = paragraph.strut(fontContext);
  const float lineHeight = options.lineMetrics.height > 0
                               ? options.lineMetrics.height
                               : strut.height;
  const float ascent = options.lineMetrics.ascent > 0
                           ? options.lineMetrics.ascent
                           : strut.ascent;

  ParagraphLayout result;
  const std::vector<Word> &words = paragraph.words();
  if (words.empty())
    return result;

  IntervalSequence intervalSequence(
      effectiveGeometry, lineHeight, ascent,
      options.lineBreakStrategy == LineBreakStrategy::kKnuthPlass
          ? options.knuthPlass.minimumIntervalWidth
          : 0.0f);

  if (options.lineBreakStrategy == LineBreakStrategy::kKnuthPlass) {
    ParagraphLayout result =
        knuthPlassLayout(fontContext, paragraph, intervalSequence, options);
    if (!options.overflow.ellipsis.empty() && result.overflowed())
      applyEllipsis(fontContext, paragraph, intervalSequence, options, result);
    return result;
  }

  // ── Greedy breaker ───────────────────────────────────────────────────
  size_t intervalIndex = 0;
  const FlatInterval *flatInterval = intervalSequence.intervalAt(intervalIndex);
  uint32_t firstWordIndex = 0;
  uint32_t wordIndex = 0;
  float penPosition = 0;
  int skippedIntervalCount = 0;
  int lastLineUsed = -1;
  // Widest interval passed over during the current skip run — the fallback
  // landing spot if the geometry runs out while a wide word keeps skipping.
  size_t widestSkippedIntervalIndex = SIZE_MAX;
  float widestSkippedIntervalLength = -1;

  auto flushLine = [&](uint32_t endWordIndex, bool isLast) {
    if (flatInterval && firstWordIndex < endWordIndex) {
      placeWords(words, firstWordIndex, endWordIndex, *flatInterval,
                 options.alignment, isLast,
                 hyphenTakenAt(words, endWordIndex, isLast, options), options,
                 result);
      lastLineUsed = std::max(lastLineUsed, flatInterval->sourceLineIndex);
    }
    firstWordIndex = endWordIndex;
    penPosition = 0;
  };

  while (wordIndex < static_cast<uint32_t>(words.size())) {
    if (!flatInterval) {
      result.firstUnplacedWord = wordIndex;
      break;
    }
    // Shape just ahead of the greedy frontier so overflowing tails remain
    // completely untouched by HarfBuzz.
    paragraph.ensureShapedTo(fontContext, wordIndex + 1);
    const Word &word = words[wordIndex];
    const float glue =
        wordIndex > firstWordIndex
            ? glueAfter(words[wordIndex - 1], penPosition, options)
            : 0;
    // Soft-hyphen words reserve room for the hyphen so a break taken right
    // after them always fits.
    const float hyphenReserve =
        (options.hyphenation.enabled && word.hyphenBreak && word.hyphenGlyph)
            ? word.hyphenGlyph->advance
            : 0;
    const bool fits = penPosition + glue + word.width + hyphenReserve <=
                      flatInterval->interval.length + kFitEpsilon;
    const bool intervalEmpty = (wordIndex == firstWordIndex);

    if (fits || (intervalEmpty && skippedIntervalCount >= kMaxIntervalSkips)) {
      penPosition += glue + word.width;
      wordIndex++;
      skippedIntervalCount = 0;
      widestSkippedIntervalIndex = SIZE_MAX;
      widestSkippedIntervalLength = -1;
      if (word.mandatoryBreakAfter) {
        const int lineIndex = flatInterval->sourceLineIndex;
        flushLine(wordIndex, /*isLast=*/true);
        lastLineUsed = std::max(lastLineUsed, lineIndex);
        do {
          flatInterval = intervalSequence.intervalAt(++intervalIndex);
        } while (flatInterval && flatInterval->sourceLineIndex ==
                                     lineIndex); // Hard break skips a line.
      }
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

    flushLine(wordIndex, /*isLast=*/false);
    flatInterval = intervalSequence.intervalAt(++intervalIndex);
  }

  flushLine(wordIndex, /*isLast=*/true);
  result.lineCount = lastLineUsed + 1;
  if (!options.overflow.ellipsis.empty() && result.overflowed())
    applyEllipsis(fontContext, paragraph, intervalSequence, options, result);
  return result;
}

ParagraphLayout layoutSingleLine(FontContext &fontContext, Paragraph &paragraph,
                                 SkPoint baselineOrigin,
                                 const PathTextOptions &pathText) {
  const float availableWidth = paragraph.naturalWidth(fontContext) + 1.0f;
  LineSetFlow singleLineFlow({{{baselineOrigin, {1, 0}, availableWidth}}});
  ParagraphLayoutOptions options;
  options.pathText = pathText;
  return layoutParagraph(fontContext, paragraph, singleLineFlow, options);
}

namespace {

const PaintStyle &resolvePaint(const std::vector<StyleSpan> &spans,
                               uint32_t styleIndex,
                               const PaintStyle *overridePaint) {
  static const PaintStyle kFallback;
  if (overridePaint)
    return *overridePaint;
  return styleIndex < spans.size() ? spans[styleIndex].style.paint : kFallback;
}

template <typename DrawPass>
void drawPaintLayers(const PaintStyle &style, DrawPass &&drawPass) {
  for (const PaintLayer &layer : style.underlays)
    if (!layer.paint.nothingToDraw())
      drawPass(layer.paint, layer.offset);
  if (!style.foreground.nothingToDraw())
    drawPass(style.foreground, SkVector{0, 0});
  for (const PaintLayer &layer : style.overlays)
    if (!layer.paint.nothingToDraw())
      drawPass(layer.paint, layer.offset);
}

/// A run that can carry a decoration band: straight horizontal glyphs.
bool decorableRun(const PositionedRun &run) {
  return !run.transformed && run.shaped && !run.shaped->vertical &&
         run.placeholderIndex < 0 && run.blob;
}

// Emits every decoration rect for a layout through `emitRect(SkRect,
// const SkPaint &)` — the paint already resolved by decorationBandPaint(),
// so callers just draw. Shared by draw() (immediate) and drawBatched()
// (deferred past the glyph buckets so strikethroughs land above the
// batched glyphs).
//
// Decorations span the *decorated range*, not individual words: contiguous
// runs on one line sharing a style (and metrics identity) merge into one
// group whose band also covers the glue between words — CSS behavior, where
// an underlined sentence is one continuous line. Skip-ink intercepts still
// come from each run's own blob; the inter-word gaps have no ink, so they
// stay covered.
/// Which decoration kinds an emission pass covers: highlights paint
/// beneath every glyph pass, everything else above them.
enum class DecorationPhase : uint8_t { kBelowGlyphs, kAboveGlyphs };

bool decorationInPhase(const Decoration &decoration, DecorationPhase phase) {
  const bool isHighlight = decoration.kind == Decoration::Kind::kHighlight;
  return phase == DecorationPhase::kBelowGlyphs ? isHighlight : !isHighlight;
}

template <typename EmitRect>
void forEachDecorationRect(const std::vector<PositionedRun> &runs,
                           const std::vector<StyleSpan> &spans,
                           const PaintStyle *overridePaint,
                           DecorationPhase phase, EmitRect &&emitRect) {
  for (size_t groupStart = 0; groupStart < runs.size();) {
    const PositionedRun &first = runs[groupStart];
    if (!decorableRun(first)) {
      ++groupStart;
      continue;
    }
    const PaintStyle &style =
        resolvePaint(spans, first.styleIndex, overridePaint);
    if (style.decorations.empty()) {
      ++groupStart;
      continue;
    }

    // Extend the group while runs stay visually contiguous left-to-right on
    // the same line with identical style and metrics identity (same
    // typeface + size ⇒ same band geometry). Bidi-reordered or
    // fallback-split neighbors simply start a new group.
    size_t groupEnd = groupStart + 1;
    while (groupEnd < runs.size()) {
      const PositionedRun &candidate = runs[groupEnd];
      const PositionedRun &previous = runs[groupEnd - 1];
      if (!decorableRun(candidate) ||
          candidate.lineIndex != first.lineIndex ||
          candidate.styleIndex != first.styleIndex ||
          candidate.shaped->typeface.get() != first.shaped->typeface.get() ||
          candidate.shaped->fontSize != first.shaped->fontSize ||
          candidate.origin.y() != first.origin.y() ||
          candidate.origin.x() < previous.origin.x())
        break;
      ++groupEnd;
    }

    const float groupStartX = first.origin.x();
    const float groupEndX = runs[groupEnd - 1].origin.x() +
                            runs[groupEnd - 1].shaped->advance;
    const SkFont font = makeFont(first.shaped->typeface,
                                 first.shaped->fontSize);
    SkFontMetrics metrics;
    font.getMetrics(&metrics);

    for (const Decoration &decoration : style.decorations) {
      if (!decorationInPhase(decoration, phase))
        continue;
      const detail::ResolvedDecorationBand band =
          detail::resolveDecorationBand(decoration, metrics,
                                        style.foreground.getColor());
      const SkPaint bandPaint = detail::decorationBandPaint(decoration, band);
      const float top = first.origin.y() + band.position;
      if (decoration.span == Decoration::Span::kPerWord) {
        // One band per word run: reuse the single-run segment geometry so
        // per-word skip-ink behaves exactly like the spanning form's.
        for (size_t runIndex = groupStart; runIndex < groupEnd; ++runIndex)
          for (const auto &[startX, endX] : detail::decorationSegments(
                   runs[runIndex], decoration, band))
            emitRect(SkRect::MakeLTRB(startX, top, endX,
                                      top + band.thickness),
                     bandPaint);
        continue;
      }
      const bool skipInk = decoration.skipInk &&
                           decoration.kind == Decoration::Kind::kUnderline;
      if (!skipInk) {
        emitRect(SkRect::MakeLTRB(groupStartX, top, groupEndX,
                                  top + band.thickness),
                 bandPaint);
        continue;
      }
      // One continuous band minus every member run's ink intercepts. Glue
      // between runs carries no ink, so it never interrupts the band.
      const SkScalar bounds[2] = {band.position,
                                  band.position + band.thickness};
      const float standoff = band.thickness;
      static thread_local std::vector<SkScalar> interceptScratch;
      float cursor = groupStartX;
      for (size_t runIndex = groupStart; runIndex < groupEnd; ++runIndex) {
        const PositionedRun &run = runs[runIndex];
        const int interceptCount = run.blob->getIntercepts(bounds, nullptr);
        if (interceptCount < 2)
          continue;
        interceptScratch.resize(static_cast<size_t>(interceptCount));
        run.blob->getIntercepts(bounds, interceptScratch.data());
        for (int pair = 0; pair + 1 < interceptCount; pair += 2) {
          const float inkStart = run.origin.x() +
                                 interceptScratch[static_cast<size_t>(pair)] -
                                 standoff;
          const float inkEnd =
              run.origin.x() +
              interceptScratch[static_cast<size_t>(pair + 1)] + standoff;
          if (inkStart > cursor)
            emitRect(SkRect::MakeLTRB(cursor, top,
                                      std::min(inkStart, groupEndX),
                                      top + band.thickness),
                     bandPaint);
          cursor = std::max(cursor, inkEnd);
        }
      }
      if (cursor < groupEndX)
        emitRect(SkRect::MakeLTRB(cursor, top, groupEndX,
                                  top + band.thickness),
                 bandPaint);
    }
    groupStart = groupEnd;
  }
}

} // namespace

namespace detail {

ResolvedDecorationBand resolveDecorationBand(const Decoration &decoration,
                                             const SkFontMetrics &metrics,
                                             SkColor foregroundColor) {
  ResolvedDecorationBand band;
  if (decoration.color != SK_ColorTRANSPARENT) {
    band.color = decoration.color;
  } else if (decoration.kind == Decoration::Kind::kHighlight) {
    // An opaque foreground-colored highlight would hide the text it sits
    // behind; default to a ~25%-alpha tint of the foreground instead.
    band.color = SkColorSetA(foregroundColor, 0x40);
  } else {
    band.color = foregroundColor;
  }

  band.thickness = decoration.thickness;
  if (band.thickness <= 0) {
    if (decoration.kind == Decoration::Kind::kHighlight) {
      band.thickness = -metrics.fAscent + metrics.fDescent; // full text height
    } else {
      SkScalar metricThickness = 0;
      const bool hasMetric =
          decoration.kind == Decoration::Kind::kStrikethrough
              ? metrics.hasStrikeoutThickness(&metricThickness)
              : metrics.hasUnderlineThickness(&metricThickness);
      band.thickness =
          hasMetric && metricThickness > 0 ? metricThickness : 1.0f;
    }
  }
  band.thickness = std::max(band.thickness, 1.0f);

  if (decoration.offset != 0) {
    band.position = decoration.offset;
    return band;
  }
  switch (decoration.kind) {
  case Decoration::Kind::kUnderline: {
    SkScalar underlinePosition = 0; // metric = distance baseline → band top
    band.position = metrics.hasUnderlinePosition(&underlinePosition)
                        ? underlinePosition
                        : band.thickness;
    break;
  }
  case Decoration::Kind::kStrikethrough: {
    SkScalar strikeoutPosition = 0; // metric = baseline → band bottom (< 0)
    if (metrics.hasStrikeoutPosition(&strikeoutPosition))
      band.position = strikeoutPosition;
    else
      band.position = -metrics.fXHeight * 0.5f - band.thickness * 0.5f;
    break;
  }
  case Decoration::Kind::kOverline:
  case Decoration::Kind::kHighlight:
    band.position = metrics.fAscent; // negative: the ascent line
    break;
  }
  return band;
}

SkPaint decorationBandPaint(const Decoration &decoration,
                            const ResolvedDecorationBand &band) {
  if (decoration.paint)
    return *decoration.paint;
  SkPaint fill;
  fill.setAntiAlias(true);
  fill.setColor(band.color);
  return fill;
}

std::vector<std::pair<float, float>>
decorationSegments(const PositionedRun &run, const Decoration &decoration,
                   const ResolvedDecorationBand &band) {
  std::vector<std::pair<float, float>> segments;
  if (run.transformed || !run.shaped || run.shaped->vertical ||
      run.placeholderIndex >= 0)
    return segments;
  const float startX = run.origin.x();
  const float endX = startX + run.shaped->advance;
  if (endX <= startX)
    return segments;

  const bool skipInk = decoration.skipInk &&
                       decoration.kind == Decoration::Kind::kUnderline &&
                       run.blob;
  if (!skipInk) {
    segments.emplace_back(startX, endX);
    return segments;
  }

  // Blob-local intercepts of glyph ink with the band; grown by one
  // thickness so the line stands off the descender instead of touching it.
  const SkScalar bounds[2] = {band.position, band.position + band.thickness};
  const int interceptCount = run.blob->getIntercepts(bounds, nullptr);
  if (interceptCount < 2) {
    segments.emplace_back(startX, endX);
    return segments;
  }
  std::vector<SkScalar> intercepts(static_cast<size_t>(interceptCount));
  run.blob->getIntercepts(bounds, intercepts.data());

  const float standoff = band.thickness;
  float cursor = startX;
  for (int interceptIndex = 0; interceptIndex + 1 < interceptCount;
       interceptIndex += 2) {
    const float inkStart =
        startX + intercepts[static_cast<size_t>(interceptIndex)] - standoff;
    const float inkEnd =
        startX + intercepts[static_cast<size_t>(interceptIndex + 1)] +
        standoff;
    if (inkStart > cursor)
      segments.emplace_back(cursor, std::min(inkStart, endX));
    cursor = std::max(cursor, inkEnd);
  }
  if (cursor < endX)
    segments.emplace_back(cursor, endX);
  return segments;
}

} // namespace detail

void ParagraphLayout::draw(SkCanvas *canvas, const Paragraph &paragraph,
                           const PaintStyle *overridePaint) const {
  const std::vector<StyleSpan> &spans = paragraph.spans();
  const auto drawRect = [&](SkRect rect, const SkPaint &paint) {
    canvas->drawRect(rect, paint);
  };

  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kBelowGlyphs, drawRect);
  for (const PositionedRun &run : runs) {
    if (!run.blob)
      continue;
    const PaintStyle &style =
        resolvePaint(spans, run.styleIndex, overridePaint);

    drawPaintLayers(style, [&](const SkPaint &paint, SkVector offset) {
      canvas->drawTextBlob(run.blob.get(), run.origin.x() + offset.x(),
                           run.origin.y() + offset.y(), paint);
    });
  }
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kAboveGlyphs, drawRect);
}

std::vector<LineMetrics>
ParagraphLayout::lineMetrics(const Paragraph &paragraph) const {
  std::vector<LineMetrics> lines;
  // Memoized per font change: lines overwhelmingly share one (typeface,
  // size), so metric resolution runs once per style stretch, not per run.
  const SkTypeface *lastTypeface = nullptr;
  float lastFontSize = 0;
  SkFontMetrics fontMetrics{};

  for (const PositionedRun &run : runs) {
    if (run.transformed || (run.shaped && run.shaped->vertical))
      continue;

    float runAscent = 0;
    float runDescent = 0;
    float runLeft = run.origin.x();
    float runRight = runLeft;
    if (run.shaped) {
      if (run.shaped->typeface.get() != lastTypeface ||
          run.shaped->fontSize != lastFontSize) {
        lastTypeface = run.shaped->typeface.get();
        lastFontSize = run.shaped->fontSize;
        makeFont(run.shaped->typeface, run.shaped->fontSize)
            .getMetrics(&fontMetrics);
      }
      runAscent = -fontMetrics.fAscent;
      runDescent = fontMetrics.fDescent;
      runRight += run.shaped->advance;
    } else if (run.placeholderIndex >= 0) {
      const Placeholder &placeholder =
          paragraph.placeholders()[static_cast<size_t>(run.placeholderIndex)];
      runAscent = std::max(0.0f, placeholder.height - placeholder.baselineDrop);
      runDescent = std::max(0.0f, placeholder.baselineDrop);
      runRight += placeholder.width;
    } else {
      continue;
    }

    // Runs arrive line by line, so the active line is nearly always the
    // last entry; fall back to a linear probe for safety (ellipsis trims
    // and bidi never break this in practice, but the contract shouldn't
    // depend on emission order).
    LineMetrics *line = nullptr;
    if (!lines.empty() && lines.back().lineIndex == run.lineIndex) {
      line = &lines.back();
    } else {
      for (LineMetrics &candidate : lines)
        if (candidate.lineIndex == run.lineIndex) {
          line = &candidate;
          break;
        }
    }
    const Word &word = paragraph.words()[run.wordIndex];
    if (!line) {
      lines.push_back({run.lineIndex, run.origin.y(), runAscent, runDescent,
                       runLeft, runRight, word.textBegin, word.whitespaceEnd});
      continue;
    }
    line->ascent = std::max(line->ascent, runAscent);
    line->descent = std::max(line->descent, runDescent);
    line->left = std::min(line->left, runLeft);
    line->right = std::max(line->right, runRight);
    line->textBegin = std::min(line->textBegin, word.textBegin);
    line->textEnd = std::max(line->textEnd, word.whitespaceEnd);
  }

  std::sort(lines.begin(), lines.end(),
            [](const LineMetrics &left, const LineMetrics &right) {
              return left.lineIndex < right.lineIndex;
            });
  return lines;
}

std::vector<ParagraphLayout::PlacedPlaceholder>
ParagraphLayout::placeholderRects(const Paragraph &paragraph) const {
  std::vector<PlacedPlaceholder> placedPlaceholders;
  for (const PositionedRun &run : runs) {
    if (run.placeholderIndex < 0)
      continue;
    const Placeholder &placeholder =
        paragraph.placeholders()[static_cast<size_t>(run.placeholderIndex)];
    placedPlaceholders.push_back(
        {run.placeholderIndex,
         SkRect::MakeXYWH(run.origin.x(),
                          run.origin.y() - placeholder.height +
                              placeholder.baselineDrop,
                          placeholder.width, placeholder.height),
         run.lineIndex});
  }
  return placedPlaceholders;
}

void ParagraphLayout::drawBatched(SkCanvas *canvas, const Paragraph &paragraph,
                                  const PaintStyle *overridePaint) const {
  const std::vector<StyleSpan> &spans = paragraph.spans();

  // Buckets keyed by (typeface, font size, resolved paint). A frame's worth of
  // horizontal runs collapses into one drawGlyphs call per bucket and layer;
  // scratch storage persists across frames (styles copied by value — span
  // pointers would dangle between calls).
  struct Bucket {
    sk_sp<SkTypeface> typeface;
    float fontSize = 0;
    float scaleX = 1.0f;
    PaintStyle style;
    std::vector<SkGlyphID> glyphs;
    std::vector<SkPoint> positions;
  };
  static thread_local std::vector<Bucket> buckets;
  if (buckets.size() > 64)
    buckets.clear(); // release pathological one-frame style cardinality
  size_t activeBucketCount = 0;

  // Decoration rects accumulate during the run walk and flush after the
  // glyph buckets, so strikethroughs/overlines land above the batched text.
  // Paints are copied by value: the resolved band paint lives on the
  // emitter's stack.
  struct DecorationRect {
    SkRect rect;
    SkPaint paint;
  };
  static thread_local std::vector<DecorationRect> decorationRects;
  decorationRects.clear();

  // Highlights go straight to the canvas: every glyph pass draws after
  // them. The above-glyph decorations accumulate and flush past the
  // buckets so strikethroughs land above the batched text.
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kBelowGlyphs,
                        [&](SkRect rect, const SkPaint &paint) {
                          canvas->drawRect(rect, paint);
                        });
  forEachDecorationRect(runs, spans, overridePaint,
                        DecorationPhase::kAboveGlyphs,
                        [&](SkRect rect, const SkPaint &paint) {
                          decorationRects.push_back({rect, paint});
                        });

  for (const PositionedRun &run : runs) {
    if (!run.blob)
      continue;
    const PaintStyle &style =
        resolvePaint(spans, run.styleIndex, overridePaint);

    if (run.transformed || !run.shaped) {
      // Positions are baked into the blob; draw every configured pass
      // directly. Arbitrary SkPaint effects remain available on this path.
      drawPaintLayers(style, [&](const SkPaint &paint, SkVector offset) {
        canvas->drawTextBlob(run.blob.get(), run.origin.x() + offset.x(),
                             run.origin.y() + offset.y(), paint);
      });
      continue;
    }

    const ShapedWord &shapedWord = *run.shaped;
    Bucket *bucket = nullptr;
    for (Bucket &candidate :
         std::span<Bucket>(buckets.data(), activeBucketCount))
      if (candidate.typeface.get() == shapedWord.typeface.get() &&
          candidate.fontSize == shapedWord.fontSize &&
          candidate.scaleX == shapedWord.scaleX &&
          candidate.style == style) {
        bucket = &candidate;
        break;
      }
    if (!bucket) {
      if (activeBucketCount == buckets.size())
        buckets.push_back({});
      bucket = &buckets[activeBucketCount++];
      bucket->typeface = shapedWord.typeface;
      bucket->fontSize = shapedWord.fontSize;
      bucket->scaleX = shapedWord.scaleX;
      bucket->style = style;
      bucket->glyphs.clear();
      bucket->positions.clear();
    }
    for (size_t glyphIndex = 0; glyphIndex < shapedWord.glyphs.size();
         ++glyphIndex) {
      bucket->glyphs.push_back(shapedWord.glyphs[glyphIndex]);
      bucket->positions.push_back(run.origin +
                                  shapedWord.positions[glyphIndex]);
    }
  }

  for (const Bucket &bucket :
       std::span<const Bucket>(buckets.data(), activeBucketCount)) {
    if (bucket.glyphs.empty())
      continue;
    const SkFont font =
        makeFont(bucket.typeface, bucket.fontSize, bucket.scaleX);
    const SkSpan<const SkGlyphID> glyphs(bucket.glyphs.data(),
                                         bucket.glyphs.size());
    const SkSpan<const SkPoint> positions(bucket.positions.data(),
                                          bucket.positions.size());
    drawPaintLayers(bucket.style, [&](const SkPaint &paint, SkVector offset) {
      canvas->drawGlyphs(glyphs, positions, {offset.x(), offset.y()}, font,
                         paint);
    });
  }

  for (const DecorationRect &decorationRect : decorationRects)
    canvas->drawRect(decorationRect.rect, decorationRect.paint);
  decorationRects.clear(); // paints hold shader refs; don't pin past the frame
}

} // namespace sigil::weave
