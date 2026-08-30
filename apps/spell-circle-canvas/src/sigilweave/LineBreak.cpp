#include <hb.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkTextBlob.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ParagraphLayoutInternal.h"
#include "sigilweave/FontContext.h"
#include "sigilweave/ParagraphLayout.h"
#include "sigilweave/Shaper.h"

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
GapKind gapKind(const std::vector<Word>& words, uint32_t wordIndex,
                const ParagraphLayoutOptions& options) {
  if (words[wordIndex].tabAfter && tabStopsActive(options))
    return GapKind::kRigid;  // tab gaps never stretch or shrink
  if (words[wordIndex].spaceWidth > 0) return GapKind::kSpace;
  if (options.justification.expandIdeographicGaps &&
      (words[wordIndex].ideographic || words[wordIndex + 1].ideographic))
    return GapKind::kIdeographic;
  return GapKind::kRigid;
}

/** Returns a word's em size, including a safe default for placeholders. */
float wordFontSize(const Word& word) {
  return word.segments().empty() ? 16.0f
                                 : word.segments().front().shaped->fontSize;
}

// Per-glyph RSXform blob for rotated straight intervals and path contours.
sk_sp<SkTextBlob> buildTransformedBlob(const ShapedWord& shapedWord,
                                       const LineInterval& interval,
                                       float penOffset, int rotationSteps) {
  if (shapedWord.glyphs.empty()) return nullptr;
  SkTextBlobBuilder builder;
  const SkFont font = makeFont(shapedWord.typeface, shapedWord.fontSize,
                               shapedWord.scaleX, shapedWord.aliased);
  const int glyphCount = static_cast<int>(shapedWord.glyphs.size());
  const auto& run = builder.allocRunRSXform(font, glyphCount);

  float penLocal = 0;
  for (int glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
    const float advance = shapedWord.advances[glyphIndex];
    // Offsets HarfBuzz applied on top of the pen position.
    const float glyphOffsetX = shapedWord.positions[glyphIndex].x() - penLocal;
    const float glyphOffsetY = shapedWord.positions[glyphIndex].y();

    // The interval owns the pen→placement mapping, and it is the SAME
    // function a caller re-placing these glyphs at draw time reads, so the
    // baked blob and a live re-placement can never disagree.
    SkPoint position;
    SkVector tangent;
    interval.placeAt(penOffset + penLocal + advance * 0.5f, 0.0f, rotationSteps,
                     &position, &tangent);

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
void emitSegment(ParagraphLayout& result, const FlatInterval& flatInterval,
                 const WordSegment& segment, uint32_t wordIndex,
                 float penOffset, const ParagraphLayoutOptions& options) {
  const ShapedWord& shapedWord = *segment.shaped;
  if (shapedWord.glyphs.empty()) return;
  PositionedRun run;
  run.shaped = segment.shaped;
  run.styleIndex = segment.styleIndex;
  run.wordIndex = wordIndex;
  run.lineIndex = flatInterval.sourceLineIndex;
  run.intervalIndex = flatInterval.index;
  run.penOffset = penOffset;
  const bool straight = !flatInterval.interval.contour.valid();
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
  if (run.blob) result.runs.push_back(std::move(run));
}

// UAX #9 rule L2 over per-word levels: reverse maximal runs of every level
// >= each odd level, highest level first.
void visualOrder(const std::vector<Word>& words, uint32_t firstWordIndex,
                 uint32_t endWordIndex,
                 std::vector<uint32_t>& visualWordOrder) {
  visualWordOrder.clear();
  visualWordOrder.reserve(endWordIndex - firstWordIndex);
  uint8_t maximumLevel = 0;
  uint8_t minimumOddLevel = 255;
  for (uint32_t wordIndex = firstWordIndex; wordIndex < endWordIndex;
       ++wordIndex) {
    visualWordOrder.push_back(wordIndex);
    const uint8_t level = words[wordIndex].bidiLevel;
    maximumLevel = std::max(maximumLevel, level);
    if (level & 1) minimumOddLevel = std::min(minimumOddLevel, level);
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

}  // namespace

float naturalWidth(const std::vector<Word>& words, uint32_t firstWordIndex,
                   uint32_t endWordIndex) {
  float width = 0;
  for (uint32_t wordIndex = firstWordIndex; wordIndex < endWordIndex;
       ++wordIndex) {
    width += words[wordIndex].width;
    if (wordIndex + 1 < endWordIndex) width += words[wordIndex].spaceWidth;
  }
  return width;
}

void placeWords(const std::vector<Word>& words, uint32_t firstWordIndex,
                uint32_t endWordIndex, const FlatInterval& flatInterval,
                TextAlignment alignment, bool lastLine, bool hyphenBreakTaken,
                const ParagraphLayoutOptions& options,
                ParagraphLayout& result) {
  if (firstWordIndex >= endWordIndex) return;

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
  int lastTabVisualIndex = -1;  // visual index of the word before the gap
  float resolvedNaturalWidth = 0;
  if (tabStopsActive(options)) {
    float pen = 0;
    for (size_t visualIndex = 0; visualIndex < visualWordOrder.size();
         ++visualIndex) {
      const Word& word = words[visualWordOrder[visualIndex]];
      pen += word.width;
      if (visualIndex + 1 < visualWordOrder.size()) {
        if (word.tabAfter) lastTabVisualIndex = static_cast<int>(visualIndex);
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
      const uint32_t gapWordIndex = std::min(visualWordOrder[visualIndex],
                                             visualWordOrder[visualIndex + 1]);
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
            extraWidth /
            static_cast<float>(spaceGapCount + ideographicGapCount);
        if (ideographicGapCount > 0 &&
            equalGapAdjustment > ideographicExpansionLimit) {
          ideographicAdjustment = ideographicExpansionLimit;
          spaceAdjustment =
              spaceGapCount > 0
                  ? (extraWidth - ideographicAdjustment *
                                      static_cast<float>(ideographicGapCount)) /
                        static_cast<float>(spaceGapCount)
                  : 0;  // no spaces to absorb the rest: stay underfull
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
    const Word& word = words[wordIndex];
    for (const WordSegment& segment : word.segments())
      emitSegment(result, flatInterval, segment, wordIndex,
                  penPosition + segment.advanceOffset, options);
    if (word.placeholderIndex >= 0 && !flatInterval.interval.contour.valid()) {
      // Inline slot: report where it landed (blob-less run; draw() and
      // drawBatched() skip it, placeholderRects() surfaces it).
      PositionedRun run;
      run.origin = flatInterval.interval.origin +
                   SkVector{flatInterval.interval.direction.x() * penPosition,
                            flatInterval.interval.direction.y() * penPosition};
      run.wordIndex = wordIndex;
      run.lineIndex = flatInterval.sourceLineIndex;
      run.intervalIndex = flatInterval.index;
      run.penOffset = penPosition;
      run.placeholderIndex = word.placeholderIndex;
      result.runs.push_back(std::move(run));
    }
    penPosition += word.width;
    if (hyphenBreakTaken && wordIndex == endWordIndex - 1) {
      // Discretionary break taken: render the hyphen right after the word.
      const uint32_t styleIndex =
          word.segments().empty() ? 0 : word.segments().back().styleIndex;
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
void applyEllipsis(FontContext& fontContext, Paragraph& paragraph,
                   IntervalSequence& intervalSequence,
                   const ParagraphLayoutOptions& options,
                   ParagraphLayout& result) {
  if (result.runs.empty()) return;
  // Overflow means the breakers consumed every interval the geometry had,
  // so the final placed line sits on the last one.
  const FlatInterval* lastInterval = nullptr;
  for (size_t intervalIndex = 0; const FlatInterval* flatInterval =
                                     intervalSequence.intervalAt(intervalIndex);
       ++intervalIndex)
    lastInterval = flatInterval;
  if (!lastInterval || lastInterval->interval.contour.valid() ||
      lastInterval->interval.direction.x() != 1 ||
      lastInterval->interval.direction.y() != 0)
    return;

  // Shape the marker in the style of the line's tail (fallback-resolved on
  // its first codepoint; cache-shared like every other word).
  const int lineIndex = result.runs.back().lineIndex;
  const uint32_t styleIndex = result.runs.back().styleIndex;
  const uint32_t tailWord = result.runs.back().wordIndex;
  const StyleSpan& span = paragraph.spans()[styleIndex];
  UChar32 firstCodepoint;
  {
    size_t codeUnitIndex = 0;
    U16_NEXT(options.overflow.ellipsis.data(), codeUnitIndex,
             options.overflow.ellipsis.size(), firstCodepoint);
  }
  const char* languageTag = span.style.shaping.languageTag.empty()
                                ? nullptr
                                : span.style.shaping.languageTag.c_str();
  sk_sp<SkTypeface> typeface = fontContext.resolveTypeface(
      span.style.shaping.typeface, firstCodepoint, languageTag);
  if (!typeface) typeface = fontContext.defaultTypeface();
  ShapedWordRef marker = shapeWord(
      fontContext, span.style.shaping, typeface, options.overflow.ellipsis,
      static_cast<ScriptTag>(HB_SCRIPT_COMMON), false);
  if (!marker || marker->glyphs.empty()) return;

  size_t lineBegin = result.runs.size();
  while (lineBegin > 0 && result.runs[lineBegin - 1].lineIndex == lineIndex)
    lineBegin--;
  auto runEndX = [&](const PositionedRun& run) {
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
  LineLimitedGeometry(FlowGeometry& inner, int maxLines)
      : m_inner(inner), m_maxLines(maxLines) {}

  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval>& intervals) override {
    return index < m_maxLines &&
           m_inner.lineIntervals(index, lineHeight, ascent, intervals);
  }
  bool uniformIntervals() const override { return m_inner.uniformIntervals(); }

 private:
  FlowGeometry& m_inner;
  int m_maxLines;
};

}  // namespace detail

ParagraphLayout layoutParagraph(FontContext& fontContext, Paragraph& paragraph,
                                FlowGeometry& geometry,
                                const ParagraphLayoutOptions& options) {
  using namespace detail;

  LineLimitedGeometry clampedGeometry(geometry, options.overflow.maxLines);
  FlowGeometry& effectiveGeometry =
      options.overflow.maxLines > 0
          ? static_cast<FlowGeometry&>(clampedGeometry)
          : geometry;

  // Whether a soft hyphen is a break opportunity is decided during
  // segmentation, so the option reaches the paragraph before it analyzes;
  // disabled, the two halves fuse into one unbreakable word. Setting it to
  // what the paragraph already holds is free.
  paragraph.setSoftHyphenBreaks(options.hyphenation.enabled);

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
  const std::vector<Word>& words = paragraph.words();
  if (words.empty()) return result;

  IntervalSequence intervalSequence(
      effectiveGeometry, lineHeight, ascent,
      options.lineBreakStrategy == LineBreakStrategy::kKnuthPlass
          ? options.knuthPlass.minimumIntervalWidth
          : 0.0f);

  // The geometry a caller needs to re-place a transformed run at draw time:
  // the intervals the layout actually consumed, in the numbering the runs
  // report, plus the snapping the placement used. Recorded on the way out
  // of every breaker, because "which interval" is only meaningful next to
  // the interval list it indexes.
  const auto recordGeometry = [&](ParagraphLayout& layout) {
    layout.tangentRotationSteps = options.pathText.tangentRotationSteps;
    layout.linePitch = lineHeight;
    layout.intervals.reserve(intervalSequence.flattened().size());
    for (const FlatInterval& flat : intervalSequence.flattened())
      layout.intervals.push_back(flat.interval);
  };

  if (options.lineBreakStrategy == LineBreakStrategy::kKnuthPlass) {
    ParagraphLayout result =
        knuthPlassLayout(fontContext, paragraph, intervalSequence, options);
    if (!options.overflow.ellipsis.empty() && result.overflowed())
      applyEllipsis(fontContext, paragraph, intervalSequence, options, result);
    recordGeometry(result);
    return result;
  }

  // ── Greedy breaker ───────────────────────────────────────────────────
  size_t intervalIndex = 0;
  const FlatInterval* flatInterval = intervalSequence.intervalAt(intervalIndex);
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
    const Word& word = words[wordIndex];
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
                                     lineIndex);  // Hard break skips a line.
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
  recordGeometry(result);
  return result;
}

ParagraphLayout layoutSingleLine(FontContext& fontContext, Paragraph& paragraph,
                                 SkPoint baselineOrigin,
                                 const PathTextOptions& pathText) {
  const float availableWidth = paragraph.naturalWidth(fontContext) + 1.0f;
  LineSetFlow singleLineFlow({{{baselineOrigin, {1, 0}, availableWidth}}});
  ParagraphLayoutOptions options;
  options.pathText = pathText;
  return layoutParagraph(fontContext, paragraph, singleLineFlow, options);
}

}  // namespace sigil::weave
