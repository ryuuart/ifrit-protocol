/** @file
 * The greedy breaker and the placement both breakers share: words fitted
 * interval by interval with a widest-skipped fallback for a word that fits
 * nowhere, then reordered per bidi level and positioned with the requested
 * alignment, as shared blobs on straight lines or baked RSXform blobs on
 * rotated and contour intervals; the ellipsis trim; the single-line entry
 * point; and layoutParagraph itself.
 */

#include <hb.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkTextBlob.h>
#include <unicode/ubidi.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

#include "ParagraphLayoutInternal.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"

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

/** The advance of a word's FIRST glyph — what a character hanging back
 *  past the line's start is a fraction of. */
float leadingAdvanceOf(const Word& word) {
  for (const WordSegment& segment : word.segments())
    if (!segment.shaped->advances.empty()) return segment.shaped->advances.front();
  return 0.0f;
}

/** The advance of a word's LAST glyph — what a character hanging past the
 *  line's end is a fraction of. */
float trailingAdvanceOf(const Word& word) {
  const std::span<const WordSegment> segments = word.segments();
  for (size_t index = segments.size(); index-- > 0;)
    if (!segments[index].shaped->advances.empty())
      return segments[index].shaped->advances.back();
  return 0.0f;
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

/** HOW A JUSTIFIED LINE SPENDS WHAT ITS WORD GAPS COULD NOT: extra advance
 *  between the glyphs, and a horizontal scale on the glyphs themselves. The
 *  default is neither, which is a shared word blob and the only path a text
 *  that never asks for the other two ever takes. */
struct LineFit {
  float letterSpacing = 0;  ///< px added after each glyph
  float glyphScale = 1.0f;  ///< horizontal scale on the glyphs
  [[nodiscard]] bool plain() const {
    return letterSpacing == 0 && glyphScale == 1.0f;
  }
  /** The advance `word` takes under this fit. */
  [[nodiscard]] float advanceOf(const ShapedWord& word) const {
    return word.advance * glyphScale +
           letterSpacing * static_cast<float>(word.glyphs.size());
  }
};

/** Per-glyph positioned blob for a run a justified line respaced or scaled:
 *  the shared blob bakes one set of positions and this line needs another. */
sk_sp<SkTextBlob> buildFittedBlob(const ShapedWord& shapedWord,
                                  const LineFit& fit) {
  SkTextBlobBuilder builder;
  const SkFont font = makeFont(shapedWord.typeface, shapedWord.fontSize,
                               shapedWord.scaleX * fit.glyphScale,
                               shapedWord.aliased);
  const int glyphCount = static_cast<int>(shapedWord.glyphs.size());
  const auto& run = builder.allocRunPos(font, glyphCount);
  for (int glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
    run.glyphs[glyphIndex] = shapedWord.glyphs[glyphIndex];
    run.points()[glyphIndex] = {
        shapedWord.positions[glyphIndex].x() * fit.glyphScale +
            fit.letterSpacing * static_cast<float>(glyphIndex),
        shapedWord.positions[glyphIndex].y()};
  }
  return builder.make();
}

/** Appends one shaped segment at its final straight or transformed position. */
void emitSegment(ParagraphLayout& result, const FlatInterval& flatInterval,
                 const WordSegment& segment, uint32_t wordIndex,
                 float penOffset, const ParagraphLayoutOptions& options,
                 const LineFit& fit = {}, float baselineShift = 0) {
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
    // Respacing and scaling are a STRAIGHT HORIZONTAL answer: a column and
    // a curve place per glyph already, and a second per-glyph rule on top
    // of those would be two placements arguing over one run.
    run.blob = fit.plain() ? wordBlob(shapedWord)
                           : buildFittedBlob(shapedWord, fit);
    // A BASELINE SHIFT lifts the span off its line's baseline and changes
    // nothing else: the advances are the face's own, so the pen is where
    // it was and the shaped run is the shared one.
    run.origin =
        flatInterval.interval.origin + SkVector{penOffset, -baselineShift};
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

// UAX #9 rule L2 over per-word levels, which is a reordering ICU performs
// on a level array: the levels of the words on the line go in, and the
// answer is the logical index each visual position holds.
void visualOrder(const std::vector<Word>& words, uint32_t firstWordIndex,
                 uint32_t endWordIndex,
                 std::vector<uint32_t>& visualWordOrder) {
  visualWordOrder.clear();
  if (endWordIndex <= firstWordIndex) return;
  const int32_t count = static_cast<int32_t>(endWordIndex - firstWordIndex);
  static thread_local std::vector<UBiDiLevel> levels;
  static thread_local std::vector<int32_t> indexMap;
  levels.clear();
  levels.reserve(static_cast<size_t>(count));
  bool anyRightToLeft = false;
  for (uint32_t wordIndex = firstWordIndex; wordIndex < endWordIndex;
       ++wordIndex) {
    levels.push_back(static_cast<UBiDiLevel>(words[wordIndex].bidiLevel));
    anyRightToLeft |= (words[wordIndex].bidiLevel & 1u) != 0;
  }
  if (!anyRightToLeft) {
    // Rule L2 reverses runs at odd levels and there are none: the visual
    // order IS the logical order, which is the whole of the answer for
    // every line of a left-to-right text.
    for (uint32_t wordIndex = firstWordIndex; wordIndex < endWordIndex;
         ++wordIndex)
      visualWordOrder.push_back(wordIndex);
    return;
  }
  indexMap.resize(static_cast<size_t>(count));
  ubidi_reorderVisual(levels.data(), count, indexMap.data());
  visualWordOrder.reserve(static_cast<size_t>(count));
  for (const int32_t logical : indexMap)
    visualWordOrder.push_back(firstWordIndex + static_cast<uint32_t>(logical));
}

}  // namespace

void resolveMojikumi(const Paragraph& paragraph,
                     const ParagraphLayoutOptions& options,
                     std::vector<float>& room) {
  room.clear();
  const bool tsume = options.tsume != 0;
  if (options.mojikumi.empty() && !tsume) return;
  const std::vector<Word>& words = paragraph.words();
  const std::u16string_view text = paragraph.text();
  room.assign(words.size(), 0.0f);
  // The em the fraction is of is the SPAN's, not the shaped word's: the
  // room a gap needs is settled before anything is shaped, so a text that
  // overflows its frame pays for the gaps it sets and not for the rest.
  size_t spanCursor = 0;
  const auto emAt = [&](uint32_t offset) {
    const std::vector<StyleSpan>& spans = paragraph.spans();
    if (spans.empty()) return 0.0f;
    while (spanCursor + 1 < spans.size() && spans[spanCursor].end <= offset)
      ++spanCursor;
    return spans[spanCursor].style.shaping.fontSize;
  };
  // The class of the character either side of each gap. A gap between two
  // words is where two full-width characters meet across a break
  // opportunity, which in a text set in full-width characters is nearly
  // every gap it has.
  const auto classAt = [&](uint32_t offset) {
    if (offset >= text.size()) return MojikumiClass::kOther;
    const MojikumiClass named = options.mojikumi.classOf(text[offset]);
    if (named != MojikumiClass::kOther) return named;
    size_t cursor = offset;
    return unicode::isFullWidth(unicode::decodeAt(text, cursor))
               ? MojikumiClass::kIdeograph
               : MojikumiClass::kOther;
  };
  for (size_t index = 0; index + 1 < words.size(); ++index) {
    const Word& word = words[index];
    // Only a gap with nothing in it is the table's to set: a word that
    // ends in whitespace has its own glue, and the face set it.
    if (word.whitespaceEnd != word.textEnd || word.textEnd == 0) continue;
    const MojikumiClass before = classAt(word.textEnd - 1);
    const MojikumiClass after = classAt(words[index + 1].textBegin);
    if (before == MojikumiClass::kOther || after == MojikumiClass::kOther)
      continue;
    float fraction =
        options.mojikumi
            .room[static_cast<size_t>(before)][static_cast<size_t>(after)];
    // TSUME closes the gap after a full-width character the table gives no
    // class of its own — the plain ones, whose side bearings are the
    // face's own even spacing rather than a punctuation mark's air.
    if (tsume && before == MojikumiClass::kIdeograph &&
        after == MojikumiClass::kIdeograph)
      fraction -= options.tsume;
    if (fraction == 0) continue;
    room[index] = fraction * emAt(word.textBegin);
  }
}

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

/** How much of the cell a tab opened stands BEFORE the first `alignOn` in
 *  it — what a character-aligned stop pulls the cell back by. A cell with
 *  no such character aligns on its end, which is what a table of figures
 *  wants for a row that holds a whole number. */
float widthBeforeAlignCharacter(const Paragraph& paragraph,
                                const std::vector<Word>& words,
                                const std::vector<uint32_t>& visualWordOrder,
                                size_t tabVisualIndex, char16_t alignOn,
                                float cellWidth) {
  const std::u16string& text = paragraph.text();
  float before = 0;
  for (size_t visualIndex = tabVisualIndex + 1;
       visualIndex < visualWordOrder.size(); ++visualIndex) {
    const Word& word = words[visualWordOrder[visualIndex]];
    const size_t found =
        text.find(alignOn, word.textBegin) < word.textEnd
            ? text.find(alignOn, word.textBegin)
            : std::u16string::npos;
    if (found != std::u16string::npos) {
      const auto target = static_cast<uint32_t>(found);
      for (const WordSegment& segment : word.segments()) {
        const ShapedWord& shaped = *segment.shaped;
        for (size_t glyphIndex = 0; glyphIndex < shaped.glyphs.size();
             ++glyphIndex) {
          if (segment.textBegin + shaped.clusters[glyphIndex] >= target)
            return before;
          before += shaped.advances[glyphIndex];
        }
      }
      return before;
    }
    before += word.width;
    if (word.tabAfter) break;  // the cell ends at the next tab
    if (visualIndex + 1 < visualWordOrder.size()) before += word.spaceWidth;
  }
  return cellWidth;
}

/** Sets a stop's leader across the gap it opened: the string repeated as
 *  many whole times as fit, BUTTED AGAINST THE STOP so the run of dots
 *  meets the figure it leads to and the ragged end falls where the eye
 *  starts rather than where it lands. Set in the style of the text before
 *  the tab, and only along a straight horizontal line — a leader down a
 *  column is a different convention and this is not it. */
void emitLeader(FontContext& fontContext, const Paragraph& paragraph,
                ParagraphLayout& result, const FlatInterval& flatInterval,
                const Word& word, uint32_t wordIndex, const TabStop& stop,
                float gapStart, float gapEnd,
                const ParagraphLayoutOptions& options) {
  static_cast<void>(options);
  if (gapEnd - gapStart <= 0 || flatInterval.interval.contour.valid()) return;
  if (flatInterval.interval.direction.x() != 1 ||
      flatInterval.interval.direction.y() != 0)
    return;
  const uint32_t styleIndex =
      word.segments().empty() ? 0 : word.segments().back().styleIndex;
  if (styleIndex >= paragraph.spans().size()) return;
  const StyleSpan& span = paragraph.spans()[styleIndex];
  UChar32 firstCodepoint = 0;
  {
    size_t codeUnitIndex = 0;
    U16_NEXT(stop.leader.data(), codeUnitIndex, stop.leader.size(),
             firstCodepoint);
  }
  const char* languageTag = span.style.shaping.languageTag.empty()
                                ? nullptr
                                : span.style.shaping.languageTag.c_str();
  sk_sp<SkTypeface> typeface = fontContext.resolveTypeface(
      span.style.shaping.typeface, firstCodepoint, languageTag);
  if (!typeface) typeface = fontContext.defaultTypeface();
  const ShapedWordRef leader =
      shapeWord(fontContext, span.style.shaping, typeface, stop.leader,
                static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, false);
  if (!leader || leader->glyphs.empty() || leader->advance <= 0) return;
  const auto repeats =
      static_cast<int>(std::floor((gapEnd - gapStart) / leader->advance));
  float pen = gapEnd - static_cast<float>(repeats) * leader->advance;
  for (int repeat = 0; repeat < repeats; ++repeat) {
    PositionedRun run;
    run.blob = wordBlob(*leader);
    run.shaped = leader;
    run.styleIndex = styleIndex;
    // A leader belongs to the WORD BEFORE ITS TAB: it is set in that word's
    // style, on that word's line, and everything that reads a run back —
    // the line bands, the choreography walk — asks a run which word it
    // came from.
    run.wordIndex = wordIndex;
    run.lineIndex = flatInterval.sourceLineIndex;
    run.intervalIndex = flatInterval.index;
    run.penOffset = pen;
    run.origin = flatInterval.interval.origin + SkVector{pen, 0};
    if (run.blob) result.runs.push_back(std::move(run));
    pen += leader->advance;
  }
}

void placeWords(FontContext& fontContext, const Paragraph& paragraph,
                uint32_t firstWordIndex, uint32_t endWordIndex,
                const FlatInterval& flatInterval, TextAlignment alignment,
                bool lastLine, bool hyphenBreakTaken,
                const ParagraphLayoutOptions& options, ParagraphLayout& result,
                std::span<const float> mojikumiAfter) {
  const std::vector<Word>& words = paragraph.words();
  if (firstWordIndex >= endWordIndex) return;
  // The room the mojikumi table and tsume put after a word, which the
  // breakers fitted this line against and placement must spend. A layout
  // that asked for neither answers the question once, here.
  const bool spacedByTable = !mojikumiAfter.empty();
  const auto roomAfter = [&](uint32_t wordIndex) {
    return spacedByTable && wordIndex < mojikumiAfter.size()
               ? mojikumiAfter[wordIndex]
               : 0.0f;
  };

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
        pen += glueAfter(word, pen, options) +
               roomAfter(visualWordOrder[visualIndex]);
      }
    }
    resolvedNaturalWidth = pen + hyphenWidth;
  }
  const bool hasTab = lastTabVisualIndex >= 0;

  // WIDTH OF THE CELL EACH TAB OPENS: the text from the tab to the next tab,
  // or to the end of the line. Only a stop that aligns its cell somewhere
  // other than its start reads it, but it costs one backward walk and the
  // walk is over words already measured.
  static thread_local std::vector<float> cellAfter;
  cellAfter.assign(visualWordOrder.size(), 0.0f);
  if (hasTab) {
    float accumulated = 0;
    for (size_t visualIndex = visualWordOrder.size(); visualIndex-- > 0;) {
      const Word& cellWord = words[visualWordOrder[visualIndex]];
      cellAfter[visualIndex] = accumulated;
      accumulated = cellWord.tabAfter
                        ? cellWord.width
                        : cellWord.width +
                              (visualIndex + 1 < visualWordOrder.size()
                                   ? cellWord.spaceWidth
                                   : 0.0f) +
                              accumulated;
    }
  }

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
  // OPTICAL MARGIN ALIGNMENT: a line that opens on a quote or closes on a
  // comma reads as indented and as short, because the eye squares a margin
  // on the mass of the type rather than on its advances. A hanging table
  // says how much of such a character may stand OUTSIDE the measure, as a
  // fraction of its own advance — so the rule scales with the type — and
  // the line is then fitted as though the measure were that much wider.
  // Down a column the same rule is burasagari.
  float hangAtStart = 0;
  float hangAtEnd = 0;
  if (!options.hanging.empty() && !flatInterval.interval.contour.valid()) {
    const Word& first = words[firstWordIndex];
    const Word& last = words[endWordIndex - 1];
    if (first.textEnd > first.textBegin)
      if (const HangingEdge* edge =
              options.hanging.find(paragraph.text()[first.textBegin]))
        hangAtStart = edge->atStart * leadingAdvanceOf(first);
    if (last.textEnd > last.textBegin)
      if (const HangingEdge* edge =
              options.hanging.find(paragraph.text()[last.textEnd - 1]))
        hangAtEnd = edge->atEnd * trailingAdvanceOf(last);
  }

  const float extraWidthNatural = flatInterval.interval.length -
                                  naturalLineWidth + hangAtStart + hangAtEnd;
  const float extraWidth = extraWidthNatural;

  TextAlignment resolvedAlignment = alignment;
  if (resolvedAlignment == TextAlignment::kJustify && lastLine &&
      !options.justification.justifyLastLine)
    resolvedAlignment = options.justification.lastLineAlignment;

  // The three passes past the word gaps are asked for or they are not, and
  // a line that does not ask for them takes the shared-blob path it always
  // took. Every field here is at the value that means "leave it alone".
  const JustificationOptions& justification = options.justification;
  const bool spendsPastGaps =
      justification.wordSpacing != 1.0f || justification.letterSpacing != 0 ||
      justification.letterSpacingMinimum != 0 ||
      justification.letterSpacingMaximum != 0 ||
      justification.glyphScale != 1.0f ||
      justification.glyphScaleMinimum != 1.0f ||
      justification.glyphScaleMaximum != 1.0f ||
      justification.singleWord == JustificationOptions::SingleWord::kJustify;
  const bool extendedJustify =
      resolvedAlignment == TextAlignment::kJustify && spendsPastGaps;
  const float wordSpacingDelta =
      extendedJustify ? justification.wordSpacing - 1.0f : 0.0f;

  float startOffset = 0;
  float spaceAdjustment = 0;
  float ideographicAdjustment = 0;
  LineFit fit;
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
      // The desired word spacing widens the line before anything is fitted:
      // it is what the gaps are AIMED at, and the elasticity is measured
      // from there.
      const float extraWidth =
          extraWidthNatural - wordSpacingDelta * stretchableGlue;
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
                ? stretchableGlue * options.justification.wordSpacing /
                      static_cast<float>(spaceGapCount) *
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
      if (!extendedJustify) break;

      // WHAT THE GAPS COULD NOT SPEND, spent in the two passes past them,
      // in order and each on what the one before it left: letter spacing
      // between the glyphs, then a horizontal scale on the glyphs. Each is
      // measured from its own DESIRED value, which widened the line before
      // any of this, and bounded by its own two limits.
      int lineGlyphCount = 0;
      float lineContentWidth = 0;
      for (uint32_t wordIndex = firstWordIndex; wordIndex < endWordIndex;
           ++wordIndex) {
        lineContentWidth += words[wordIndex].width;
        for (const WordSegment& segment : words[wordIndex].segments())
          lineGlyphCount += static_cast<int>(segment.shaped->glyphs.size());
      }
      const float em = wordFontSize(words[firstWordIndex]);
      const auto glyphGaps = static_cast<float>(std::max(lineGlyphCount - 1, 0));
      float residual =
          extraWidth -
          (spaceAdjustment * static_cast<float>(spaceGapCount) +
           ideographicAdjustment * static_cast<float>(ideographicGapCount));

      float letterSpacing = justification.letterSpacing * em;
      residual -= letterSpacing * glyphGaps;
      const bool loneWord = spaceGapCount + ideographicGapCount == 0;
      if (glyphGaps > 0 && loneWord &&
          justification.singleWord ==
              JustificationOptions::SingleWord::kJustify) {
        // A LINE HOLDING ONE WORD has no gaps at all: asked to justify, it
        // spends the whole measure between its letters and no limit could
        // mean anything, because there is nothing else to spend it on.
        letterSpacing += residual / glyphGaps;
        residual = 0;
        startOffset = 0;
      } else if (glyphGaps > 0) {
        const float wanted = letterSpacing + residual / glyphGaps;
        const float bounded =
            std::clamp(wanted,
                       std::min(letterSpacing,
                                justification.letterSpacingMinimum * em),
                       std::max(letterSpacing,
                                justification.letterSpacingMaximum * em));
        residual -= (bounded - letterSpacing) * glyphGaps;
        letterSpacing = bounded;
      }

      float glyphScale = justification.glyphScale;
      residual -= (glyphScale - 1.0f) * lineContentWidth;
      if (lineContentWidth > 0) {
        const float wanted = glyphScale + residual / lineContentWidth;
        const float bounded = std::clamp(
            wanted, std::min(glyphScale, justification.glyphScaleMinimum),
            std::max(glyphScale, justification.glyphScaleMaximum));
        glyphScale = bounded;
      }
      fit = {letterSpacing, glyphScale};
      break;
    }
  }

  // The hang itself: the line starts one hang back, so the character that
  // may hang sits outside the measure and the letters after it square on
  // it. A justified line spent the extra room the hang opened, so its
  // interior is already correct.
  float penPosition = startOffset - hangAtStart;
  for (size_t visualIndex = 0; visualIndex < visualWordOrder.size();
       ++visualIndex) {
    const uint32_t wordIndex = visualWordOrder[visualIndex];
    const Word& word = words[wordIndex];
    float wordAdvance = word.width;
    const auto shiftOf = [&](const WordSegment& segment) {
      return segment.styleIndex < paragraph.spans().size()
                 ? paragraph.spans()[segment.styleIndex].style.paint.baselineShift
                 : 0.0f;
    };
    if (fit.plain()) {
      for (const WordSegment& segment : word.segments())
        emitSegment(result, flatInterval, segment, wordIndex,
                    penPosition + segment.advanceOffset, options, {},
                    shiftOf(segment));
    } else {
      // Under a fit the segments' own offsets no longer hold: each one is
      // as wide as the fit makes it, so the word's pen is walked here and
      // its advance is what that walk reached.
      float local = 0;
      for (const WordSegment& segment : word.segments()) {
        emitSegment(result, flatInterval, segment, wordIndex,
                    penPosition + local, options, fit, shiftOf(segment));
        local += fit.advanceOf(*segment.shaped);
      }
      if (!word.segments().empty()) wordAdvance = local;
    }
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
    penPosition += wordAdvance;
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
      if (word.tabAfter && tabStopsActive(options)) {
        // THE CELL THE TAB OPENS decides where the pen lands: a stop that
        // starts its cell puts the pen on the stop, and one that ends,
        // centres or pins a character in it pulls the pen back by as much
        // of the cell as stands before that point. The pen never moves
        // backwards — a cell too wide for its stop simply runs on.
        ResolvedTabStop resolved;
        if (tabStopAhead(penPosition - startOffset, options, resolved)) {
          const float gapStart = penPosition;
          const float stopPen = resolved.position + startOffset;
          float target = stopPen;
          if (resolved.stop) {
            switch (resolved.stop->align) {
              case TabStop::Align::kStart:
                break;
              case TabStop::Align::kEnd:
                target = stopPen - cellAfter[visualIndex];
                break;
              case TabStop::Align::kCenter:
                target = stopPen - cellAfter[visualIndex] * 0.5f;
                break;
              case TabStop::Align::kCharacter:
                target = stopPen - widthBeforeAlignCharacter(
                                       paragraph, words, visualWordOrder,
                                       visualIndex, resolved.stop->alignOn,
                                       cellAfter[visualIndex]);
                break;
            }
          }
          penPosition = std::max(penPosition, target);
          if (resolved.stop && !resolved.stop->leader.empty())
            emitLeader(fontContext, paragraph, result, flatInterval, word,
                       wordIndex, *resolved.stop, gapStart, penPosition,
                       options);
          continue;
        }
      }
      penPosition += glueAfter(word, penPosition - startOffset, options) +
                     roomAfter(wordIndex);
      if (static_cast<int>(visualIndex) > lastTabVisualIndex) {
        switch (gapKind(words,
                        std::min(wordIndex, visualWordOrder[visualIndex + 1]),
                        options)) {
          case GapKind::kSpace:
            penPosition += wordSpacingDelta * word.spaceWidth + spaceAdjustment;
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
// fits, then append it as one more run (CSS text-overflow semantics). A
// line's marker lands at its end and a column's at its foot; a contour
// interval takes none, because there is no end to a loop.
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
  if (!lastInterval || lastInterval->interval.contour.valid()) return;
  const SkVector direction = lastInterval->interval.direction;
  const bool alongColumn = direction.x() == 0 && direction.y() == 1;
  if (!alongColumn && (direction.x() != 1 || direction.y() != 0)) return;

  // Shape the marker in the style of the line's tail (fallback-resolved on
  // its first codepoint; cache-shared like every other word) — and, down a
  // column, in that tail's FORM. THE MARKER STANDS FOR THE TEXT THAT WAS
  // CUT, so it is set the way that text was set: a column of upright
  // glyphs ends in an upright marker, which TTB shaping gives the face's
  // own `vert` form when it has one, and a rotated Latin run ends in a
  // marker turned with the column exactly as the letters before it are.
  const int lineIndex = result.runs.back().lineIndex;
  const uint32_t styleIndex = result.runs.back().styleIndex;
  const uint32_t tailWord = result.runs.back().wordIndex;
  const bool uprightMarker = alongColumn && !result.runs.back().transformed;
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
      static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, uprightMarker);
  if (!marker || marker->glyphs.empty()) return;

  size_t lineBegin = result.runs.size();
  while (lineBegin > 0 && result.runs[lineBegin - 1].lineIndex == lineIndex)
    lineBegin--;
  // How far along the interval a run reaches, in the pen's own direction —
  // the one measurement the trim is made of, and the only thing about it
  // that the writing mode changes.
  auto runEnd = [&](const PositionedRun& run) {
    const float runWidth = run.shaped
                               ? run.shaped->advance
                               : (run.placeholderIndex >= 0
                                      ? paragraph
                                            .placeholders()[static_cast<size_t>(
                                                run.placeholderIndex)]
                                            .width
                                      : 0.0f);
    if (!alongColumn) return run.origin.x() + runWidth;
    // A ROTATED run's placement is baked into its blob and its origin is
    // the canvas origin, so only its pen offset says where down the column
    // it sits; its horizontal advance IS its travel down the column.
    if (run.transformed)
      return lastInterval->interval.origin.y() + run.penOffset + runWidth;
    // A TATE-CHU-YOKO run stands across the column and consumes its font
    // height, not the advance of however many digits it holds; its origin
    // is the baseline it stands on, so its foot is one descent below.
    if (run.shaped && !run.shaped->vertical) {
      SkFontMetrics metrics;
      makeFont(run.shaped->typeface, run.shaped->fontSize, run.shaped->scaleX,
               run.shaped->aliased)
          .getMetrics(&metrics);
      return run.origin.y() + metrics.fDescent;
    }
    return run.origin.y() + runWidth;
  };

  // Drop whole trailing words until the marker fits inside the interval.
  const float intervalStart = alongColumn ? lastInterval->interval.origin.y()
                                          : lastInterval->interval.origin.x();
  const float limit =
      intervalStart + lastInterval->interval.length - marker->advance + 0.25f;
  while (result.runs.size() > lineBegin && runEnd(result.runs.back()) > limit) {
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
  run.styleIndex = styleIndex;
  run.wordIndex = tailWord;
  run.lineIndex = lineIndex;
  const bool afterARun = result.runs.size() > lineBegin;
  const float markerPen =
      afterARun ? runEnd(result.runs.back()) : intervalStart;
  // The marker names the interval it landed on and where along it, like
  // any other run. A COLUMN's metrics are read through that pair — a
  // column has no baseline to measure from the way a line has one — so a
  // marker that named neither would fall outside the column it ends.
  run.intervalIndex = lastInterval->index;
  run.penOffset = markerPen - intervalStart;
  if (!alongColumn) {
    run.blob = wordBlob(*marker);
    run.origin = {markerPen, afterARun ? result.runs.back().origin.y()
                                       : lastInterval->interval.origin.y()};
  } else if (uprightMarker) {
    run.blob = wordBlob(*marker);
    run.origin = {lastInterval->interval.origin.x(), markerPen};
  } else {
    run.blob = buildTransformedBlob(*marker, lastInterval->interval,
                                    markerPen - intervalStart,
                                    options.pathText.tangentRotationSteps);
    run.transformed = true;
  }
  if (!run.blob) return;
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

  using FlowGeometry::lineIntervals;
  bool lineIntervals(const LineRequest& request,
                     std::vector<LineInterval>& intervals) override {
    return request.index < m_maxLines &&
           m_inner.lineIntervals(request, intervals);
  }
  bool uniformIntervals() const override { return m_inner.uniformIntervals(); }

 private:
  FlowGeometry& m_inner;
  int m_maxLines;
};

namespace {

/** The BLOCKS of a text — the words between mandatory breaks — each with
 *  its style resolved against the layout's own, its pitch resolved from its
 *  own first span, and the air before it resolved by the one spacing rule:
 *  THE GAP IS THE LARGER of the block before's `spaceAfter` and this
 *  block's `spaceBefore`, everywhere, the head of the flow included. */
std::vector<detail::Block> resolveBlocks(
    FontContext& fontContext, const Paragraph& paragraph,
    const ParagraphLayoutOptions& options) {
  const std::vector<Word>& words = paragraph.words();
  std::vector<detail::Block> blocks;
  uint32_t first = 0;
  for (uint32_t wordIndex = 0; wordIndex < words.size(); ++wordIndex)
    if (words[wordIndex].mandatoryBreakAfter) {
      blocks.push_back(
          detail::Block{static_cast<int>(blocks.size()), first, wordIndex + 1});
      first = wordIndex + 1;
    }
  if (first < words.size() || blocks.empty())
    blocks.push_back(detail::Block{static_cast<int>(blocks.size()), first,
                                   static_cast<uint32_t>(words.size())});

  const ParagraphStyle unstyled;
  float previousSpaceAfter = 0;
  for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    detail::Block& block = blocks[blockIndex];
    const ParagraphStyle& style = blockIndex < options.blocks.size()
                                      ? options.blocks[blockIndex]
                                      : unstyled;
    block.style = style;
    block.options = options;
    if (style.alignment) block.options.alignment = *style.alignment;
    if (style.justification) block.options.justification = *style.justification;
    if (style.hyphenation) block.options.hyphenation = *style.hyphenation;
    if (style.tabStops) block.options.tabStops = *style.tabStops;

    const uint32_t textOffset =
        block.firstWord < words.size() ? words[block.firstWord].textBegin : 0;
    const Paragraph::Strut strut = paragraph.strutAt(fontContext, textOffset);
    // A stated line metric overrides what the FACE reports; a stated leading
    // overrides the pitch outright, and the extra it opens goes above the
    // line, which is where leading has always gone.
    const float faceHeight =
        options.lineMetrics.height > 0 ? options.lineMetrics.height
                                       : strut.height;
    const float faceAscent =
        options.lineMetrics.ascent > 0 ? options.lineMetrics.ascent
                                       : strut.ascent;
    float pitch = faceHeight;
    float gridStep = 0;
    switch (style.leading.kind) {
      case Leading::Kind::kFace:
        break;
      case Leading::Kind::kMultiple:
        pitch = faceHeight * style.leading.value;
        break;
      case Leading::Kind::kAbsolute:
        pitch = style.leading.value;
        break;
      case Leading::Kind::kGrid:
        gridStep = style.leading.value;
        pitch = gridStep > 0 ? std::ceil(faceHeight / gridStep) * gridStep
                             : faceHeight;
        break;
    }
    // The reserved band is a layout input: it opens the pitch before
    // anything is broken, and `before` carries the baseline down inside the
    // band so the type stays where a reader expects it.
    const float reservedBefore =
        options.reserved.before + style.reserved.before;
    const float reservedAfter = options.reserved.after + style.reserved.after;
    block.pitch = pitch + reservedBefore + reservedAfter;
    // WHERE THE LEADING GOES. All of it above the line is the setting
    // convention; half above and half below is the web's, and a passage
    // that must sit optically centred in its own band wants that one.
    const float opened = pitch - faceHeight;
    block.ascent = (style.leading.kind == Leading::Kind::kFace
                        ? faceAscent
                        : faceAscent + opened * (style.halfLeading ? 0.5f : 1.0f)) +
                   reservedBefore;
    block.gridStep = gridStep;
    block.lead = blockIndex == 0
                     ? style.spaceBefore
                     : std::max(previousSpaceAfter, style.spaceBefore);
    previousSpaceAfter = style.spaceAfter;
  }
  return blocks;
}

/** The interval a block's LAST line is placed in: the last-line indent adds
 *  to the near end, exactly as the first-line one does on the first. */
detail::FlatInterval withLastLineIndent(const detail::FlatInterval& flat,
                                        float indent) {
  if (indent == 0 || flat.interval.contour.valid()) return flat;
  detail::FlatInterval shortened = flat;
  shortened.interval.origin +=
      SkVector{flat.interval.direction.x() * indent,
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
  const HyphenationOptions& hyphenation = block.options.hyphenation;
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
  const float zone = block.options.hyphenation.zone;
  if (zone <= 0 || block.options.alignment == TextAlignment::kJustify)
    return true;
  const uint32_t whole = wholeWordStart(words, endWordIndex);
  if (whole <= lineStart) return true;  // the word is the whole line
  return measure - naturalWidth(words, lineStart, whole) > zone;
}

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
  const ParagraphLayoutOptions& options = block.options;
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
    const bool fits =
        penPosition + glue + word.width + hyphenReserve <= measure + kFitEpsilon;
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

/** WHAT ONE BLOCK PUT IN THIS FRAME — the record the keeps are settled
 *  against once the fill has stopped. */
struct PlacedBlock {
  const detail::Block* block = nullptr;
  size_t firstRun = 0;  ///< half-open range in ParagraphLayout::runs
  size_t endRun = 0;
  int lines = 0;  ///< distinct flow lines the block's runs landed on
};

/** How many lines the words in `[first, end)` would take at `measure`,
 *  counted no further than `cap`.
 *
 *  It is a greedy fit at ONE measure and nothing else. The measure is the
 *  NEXT frame's where the caller stated one
 *  (`ParagraphLayoutOptions::nextMeasure`) and the measure this frame's
 *  last line was set in otherwise, which is the same number for a chain of
 *  equal frames and the honest fallback when nobody holds the chain. The
 *  FIT stays greedy even for a block the optimizing breaker set, so a
 *  remainder whose hyphens or demerits would have bought it a line comes
 *  out a line long. Counting stops at `cap` because every caller only asks
 *  whether the remainder REACHES a number, and stopping there is what
 *  keeps the tail of a long block unshaped. */
int remainderLines(FontContext& fontContext, Paragraph& paragraph,
                   uint32_t first, uint32_t end, float measure, int cap) {
  if (first >= end || measure <= 0 || cap <= 0) return 0;
  const std::vector<Word>& words = paragraph.words();
  int lines = 1;
  float pen = 0;
  for (uint32_t wordIndex = first; wordIndex < end && wordIndex < words.size();
       ++wordIndex) {
    paragraph.ensureShapedTo(fontContext, wordIndex + 1);
    const Word& word = words[wordIndex];
    const float glue = wordIndex > first ? words[wordIndex - 1].spaceWidth : 0;
    if (pen > 0 && pen + glue + word.width > measure + kFitEpsilon) {
      if (++lines >= cap) return cap;
      pen = word.width;
    } else {
      pen += glue + word.width;
    }
  }
  return lines;
}

/** ENFORCES THE KEEPS AT THE FRAME BOUNDARY: lines the block may not leave
 *  behind are taken out of this fill and reported as overflow, so the next
 *  frame of the chain gets them.
 *
 *  A keep is a statement about a BOUNDARY — a widow stands at the head of
 *  the next frame, an orphan at the foot of this one, a kept-together pair
 *  straddles the join — so it is settled where the boundary is, once the
 *  fill has stopped, by moving lines forward. Nothing is weighed against
 *  spacing and no break is re-decided, which is why both breakers obey
 *  these identically: retracting a line is not a break decision.
 *
 *  A KEEP NEVER EMPTIES A FRAME. A retraction that would leave the fill
 *  with nothing is dropped, because the text it moved would arrive at the
 *  next frame in exactly the state that emptied this one and the chain
 *  would never advance.
 *
 *  Returns the depth the retracted lines had occupied, which the frame's
 *  vertical distribution must not spend. */
float enforceKeeps(FontContext& fontContext, Paragraph& paragraph,
                   const std::vector<PlacedBlock>& placed,
                   float remainderMeasure, ParagraphLayout& result) {
  if (!result.overflowed() || placed.empty()) return 0;

  float depthFreed = 0;
  // Retracts every run from `runIndex` on, reporting the first word of
  // them as where this frame ran out.
  const auto retractFrom = [&](size_t runIndex) {
    if (runIndex == 0 || runIndex >= result.runs.size()) return false;
    result.firstUnplacedWord =
        std::min(result.firstUnplacedWord, result.runs[runIndex].wordIndex);
    result.runs.erase(result.runs.begin() + (long)runIndex, result.runs.end());
    result.ellipsized = false;
    return true;
  };
  // The run each of a block's last `count` lines begins at.
  const auto runStartingLastLines = [&](const PlacedBlock& entry, int count) {
    size_t runIndex = entry.endRun;
    int lines = 0;
    while (runIndex > entry.firstRun && lines < count) {
      const int line = result.runs[runIndex - 1].lineIndex;
      while (runIndex > entry.firstRun &&
             result.runs[runIndex - 1].lineIndex == line)
        --runIndex;
      ++lines;
    }
    return runIndex;
  };

  size_t entryIndex = placed.size() - 1;
  const PlacedBlock& last = placed[entryIndex];
  const KeepOptions& keep = last.block->style.keep;
  const bool split = result.firstUnplacedWord < last.block->endWord &&
                     result.firstUnplacedWord > last.block->firstWord;

  int retractLines = 0;
  bool wholeBlock = false;
  if (split) {
    if (keep.allLinesTogether) {
      wholeBlock = true;
    } else if (keep.orphanLines > 0 && last.lines < keep.orphanLines) {
      wholeBlock = true;
    } else if (keep.widowLines > 0) {
      const int carried =
          remainderLines(fontContext, paragraph, result.firstUnplacedWord,
                         last.block->endWord, remainderMeasure,
                         keep.widowLines);
      retractLines = keep.widowLines - carried;
      // Every line pulled back out of this frame is a line the next frame
      // gains, so what the block keeps here must still satisfy its own
      // orphan rule; when it cannot, the block goes over whole.
      if (retractLines > 0 &&
          last.lines - retractLines < std::max(keep.orphanLines, 1))
        wholeBlock = true;
    }
  } else if (keep.withNext && last.block->endWord <= result.firstUnplacedWord) {
    // The block ended in this frame and the one after it begins in the
    // next: the pair the caller asked to keep together is exactly the pair
    // the boundary fell between.
    wholeBlock = true;
  }

  // Whole-block retractions cascade backwards: a block that leaves takes
  // the frame's boundary with it, and the block before it that asked to
  // keep with the next now sits against that boundary.
  while (wholeBlock || retractLines > 0) {
    const PlacedBlock& entry = placed[entryIndex];
    const int lines =
        wholeBlock ? entry.lines : std::min(retractLines, entry.lines);
    if (!retractFrom(wholeBlock ? entry.firstRun
                                : runStartingLastLines(entry, lines)))
      break;
    depthFreed += entry.block->pitch * static_cast<float>(lines);
    if (!wholeBlock) break;
    depthFreed += entry.block->lead;
    if (entryIndex == 0) break;
    --entryIndex;
    if (!placed[entryIndex].block->style.keep.withNext) break;
  }
  return depthFreed;
}

/** WHERE THE FLOW'S FIRST BAND BEGINS, from the first-baseline rule: the
 *  rule names where baseline 0 sits below the flow's near edge, and every
 *  later baseline follows at its own block's pitch, so seating the passage
 *  is one number applied once. */
float firstBandStart(const FrameOptions& frame, const Paragraph::Strut& strut,
                     float ascent, float pitch) {
  float baseline = 0;
  switch (frame.firstBaseline) {
    case FrameOptions::FirstBaseline::kAscent:
      baseline = ascent + frame.firstBaselineOffset;
      break;
    case FrameOptions::FirstBaseline::kCapHeight:
      baseline = strut.capHeight + frame.firstBaselineOffset;
      break;
    case FrameOptions::FirstBaseline::kXHeight:
      baseline = strut.xHeight + frame.firstBaselineOffset;
      break;
    case FrameOptions::FirstBaseline::kLeading:
      baseline = pitch + frame.firstBaselineOffset;
      break;
    case FrameOptions::FirstBaseline::kFixed:
      baseline = frame.firstBaselineOffset;
      break;
  }
  return baseline - ascent;
}

/** Spends the room a frame has left over on the lines it holds: nothing,
 *  half above, all above, or spread between them as extra leading.
 *
 *  A pure translation along the stacking axis, applied to the placed runs
 *  and to the intervals they report landing on, so a caller re-placing a
 *  run reads the geometry the glyphs are actually on. A run whose glyphs
 *  were baked per-glyph — a path or a rotated column — carries its
 *  placement inside its blob and cannot be moved, so a flow holding one is
 *  left where it stands. */
void distributeInFrame(const FrameOptions& frame, float usedDepth,
                       int lineCount, ParagraphLayout& layout) {
  if (frame.extent <= 0 || layout.runs.empty() || layout.intervals.empty())
    return;
  if (frame.distribute == FrameOptions::Distribute::kStart) return;
  const float leftover = frame.extent - usedDepth;
  if (leftover <= 0) return;
  for (const PositionedRun& run : layout.runs)
    if (run.transformed) return;
  const SkVector direction = layout.intervals.front().direction;
  SkVector stack{0, 1};  // lines stack down the page
  if (direction.x() == 0 && direction.y() == 1)
    stack = {-1, 0};  // columns advance right to left
  else if (direction.x() != 1 || direction.y() != 0)
    return;  // neither a line nor a column: nothing to distribute along

  float shift = 0;
  float perLine = 0;
  switch (frame.distribute) {
    case FrameOptions::Distribute::kStart:
      return;
    case FrameOptions::Distribute::kCenter:
      shift = leftover * 0.5f;
      break;
    case FrameOptions::Distribute::kEnd:
      shift = leftover;
      break;
    case FrameOptions::Distribute::kJustify:
      if (lineCount < 2) return;
      perLine = leftover / static_cast<float>(lineCount - 1);
      if (frame.maximumInterlineSpacing > 0)
        perLine = std::min(perLine, frame.maximumInterlineSpacing);
      break;
  }
  const auto move = [&](SkPoint& point, int lineIndex) {
    const float distance =
        shift + perLine * static_cast<float>(std::max(lineIndex, 0));
    point += SkVector{stack.x() * distance, stack.y() * distance};
  };
  for (PositionedRun& run : layout.runs) move(run.origin, run.lineIndex);
  for (size_t index = 0; index < layout.intervals.size(); ++index)
    move(layout.intervals[index].origin, static_cast<int>(index));
}

}  // namespace

}  // namespace detail


ParagraphLayout layoutParagraph(FontContext& fontContext, Paragraph& paragraph,
                                FlowGeometry& geometry,
                                const ParagraphLayoutOptions& options,
                                uint32_t firstWord) {
  using namespace detail;

  LineLimitedGeometry clampedGeometry(geometry, options.overflow.maxLines);
  FlowGeometry& effectiveGeometry =
      options.overflow.maxLines > 0
          ? static_cast<FlowGeometry&>(clampedGeometry)
          : geometry;

  // Whether a soft hyphen is a break opportunity is decided during
  // segmentation, so the option reaches the paragraph before it analyzes;
  // disabled, the two halves fuse into one unbreakable word. Where else
  // inside a word a break may fall is the same kind of fact and reaches it
  // the same way. Setting either to what the paragraph already holds is
  // free.
  paragraph.setSoftHyphenBreaks(options.hyphenation.enabled);
  paragraph.setHyphenator(options.hyphenation.patterns,
                          options.hyphenation.limits);
  paragraph.setKinsoku(options.kinsoku);

  // Segmentation only; the breakers pull HarfBuzz shaping just ahead of
  // their own frontier, so text past the geometry never shapes at all.
  paragraph.ensureAnalyzed(fontContext);

  ParagraphLayout result;
  const std::vector<Word>& words = paragraph.words();
  if (words.empty()) return result;

  // The room a mojikumi table and tsume put after each word, resolved once
  // for the whole text and read by both breakers and by placement. Empty,
  // and free, for a layout that asked for neither.
  static thread_local std::vector<float> mojikumiRoom;
  resolveMojikumi(paragraph, options, mojikumiRoom);

  std::vector<Block> blocks = resolveBlocks(fontContext, paragraph, options);
  for (Block& block : blocks) block.mojikumiAfter = mojikumiRoom;
  // RESUMING: the blocks are numbered from the start of the text, so a
  // frame in the middle of a chain reads the same style for the same
  // block. What changes is where the fill begins — the blocks already
  // placed are dropped and the one the cursor sits in starts at the
  // cursor.
  if (firstWord > 0) {
    size_t firstBlock = 0;
    while (firstBlock < blocks.size() && blocks[firstBlock].endWord <= firstWord)
      ++firstBlock;
    if (firstBlock >= blocks.size()) return result;
    blocks.erase(blocks.begin(), blocks.begin() + (long)firstBlock);
    blocks.front().firstWord = std::max(blocks.front().firstWord, firstWord);
    // A block resumed part-way opens no air of its own: the gap it asked
    // for was spent where it began, in the frame before this one.
    blocks.front().lead = 0;
  }
  const Paragraph::Strut strut = paragraph.strutAt(
      fontContext,
      blocks.front().firstWord < words.size()
          ? words[blocks.front().firstWord].textBegin
          : 0);

  IntervalSequence intervalSequence(
      effectiveGeometry, blocks.front().pitch, blocks.front().ascent,
      options.lineBreakStrategy == LineBreakStrategy::kKnuthPlass
          ? options.knuthPlass.minimumIntervalWidth
          : 0.0f);
  intervalSequence.seatFirstBand(firstBandStart(
      options.frame, strut, blocks.front().ascent, blocks.front().pitch));

  // The geometry a caller needs to re-place a transformed run at draw time:
  // the intervals the layout actually consumed, in the numbering the runs
  // report, plus the snapping the placement used. Recorded on the way out,
  // because "which interval" is only meaningful next to the interval list
  // it indexes.
  const auto recordGeometry = [&](ParagraphLayout& layout) {
    layout.tangentRotationSteps = options.pathText.tangentRotationSteps;
    layout.linePitch = blocks.front().pitch;
    layout.intervals.reserve(intervalSequence.flattened().size());
    for (const FlatInterval& flat : intervalSequence.flattened())
      layout.intervals.push_back(flat.interval);
  };

  const bool optimizing =
      options.lineBreakStrategy == LineBreakStrategy::kKnuthPlass;
  size_t nextInterval = 0;
  int lastLineUsed = -1;
  std::vector<PlacedBlock> placedBlocks;
  // The cheapened copies a degraded frame is set from — a block that
  // degrades is set from ITS copy, and everything downstream reads the
  // setting the lines were actually made under. A deque because a record
  // points at one: it allocates nothing until a block degrades, and never
  // moves what it holds.
  std::deque<Block> cheapBlocks;
  float lastMeasure = 0;
  for (size_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
    const Block& block = blocks[blockIndex];
    if (block.firstWord >= block.endWord) continue;
    // A block that must start a frame ends one it did not start: the fill
    // stops here and the block arrives at the head of the next.
    if (block.style.keep.startInNextFrame && !result.runs.empty()) {
      result.firstUnplacedWord = block.firstWord;
      break;
    }
    intervalSequence.openBlock(block.index, block.pitch, block.ascent,
                               block.lead, block.gridStep, block.style.indent);
    intervalSequence.setUniformBlocks(block.style.indent.firstLine == 0 &&
                                      block.style.indent.lastLine == 0);
    uint32_t overflowWord = ~0u;
    size_t lastIntervalUsed = SIZE_MAX;
    const size_t firstRun = result.runs.size();
    bool outOfBudget = false;
    // A block whose lines this thread has already decided the ends of, for
    // these words at this measure under this setting, is placed from that
    // decision: deciding is the expensive half of composing a paragraph and
    // a moving text asks for the same decision frame after frame.
    const BreakList* kept = nullptr;
    if (optimizing && options.live && intervalSequence.uniform()) {
      const FlatInterval* first = intervalSequence.intervalAt(nextInterval);
      if (first)
        kept = breakStore().find(
            BreakKey{paragraph.identity(), paragraph.wordRevision(),
                     block.firstWord, block.endWord,
                     quantisedMeasure(first->interval.length),
                     breakSetting(block)});
    }
    if (kept) {
      ++result.reusedBlocks;
      placeBreaks(fontContext, paragraph, intervalSequence, block, *kept,
                  result, lastIntervalUsed, overflowWord);
    } else if (optimizing)
      knuthPlassBlock(fontContext, paragraph, intervalSequence, block,
                      nextInterval, result, lastIntervalUsed, overflowWord,
                      outOfBudget);
    // WHAT A DEGRADE ACTUALLY DROPS. The composer ran out of budget on
    // this block, so the frame is set greedily rather than late — and
    // greedily means the whole setting, not the breaker alone. The
    // controls that cost a frame something go with it: the hyphens (a
    // break the greedy fitter would have to reserve room for and then
    // weigh), the justification passes past the word gaps (letter spacing
    // and glyph scaling, both a second and third fitting of every line),
    // and the widow rule (the one keep that has to count lines the frame
    // cannot see, which means shaping past its own end). The keeps that
    // cost nothing — orphans, keep-with-next, all-lines-together — are
    // enforced as they always are. Everything is back the next frame the
    // budget is met.
    const Block* setFrom = &block;
    if (outOfBudget) {
      ++result.degradedBlocks;
      overflowWord = ~0u;
      cheapBlocks.push_back(block);
      Block& cheap = cheapBlocks.back();
      cheap.options.hyphenation.enabled = false;
      JustificationOptions& justification = cheap.options.justification;
      justification.letterSpacingMinimum = justification.letterSpacing;
      justification.letterSpacingMaximum = justification.letterSpacing;
      justification.glyphScaleMinimum = justification.glyphScale;
      justification.glyphScaleMaximum = justification.glyphScale;
      cheap.style.keep.widowLines = 0;
      setFrom = &cheap;
    }
    if ((!optimizing && !kept) || outOfBudget)
      lastIntervalUsed =
          greedyBlock(fontContext, paragraph, intervalSequence, *setFrom,
                      nextInterval, result, overflowWord);
    if (result.runs.size() > firstRun) {
      PlacedBlock entry{setFrom, firstRun, result.runs.size(), 1};
      for (size_t runIndex = firstRun + 1; runIndex < result.runs.size();
           ++runIndex)
        if (result.runs[runIndex].lineIndex !=
            result.runs[runIndex - 1].lineIndex)
          ++entry.lines;
      placedBlocks.push_back(entry);
    }
    if (lastIntervalUsed != SIZE_MAX) {
      const FlatInterval* used = intervalSequence.intervalAt(lastIntervalUsed);
      if (used) {
        lastLineUsed = std::max(lastLineUsed, used->sourceLineIndex);
        lastMeasure = used->interval.length;
      }
      // A block never shares a band with the one before it: whatever is
      // left of the line this block ended on belongs to no one.
      nextInterval = intervalSequence.pastSourceLine(lastIntervalUsed);
    }
    if (overflowWord != ~0u ||
        (lastIntervalUsed == SIZE_MAX && !intervalSequence.intervalAt(
                                             nextInterval))) {
      result.firstUnplacedWord =
          overflowWord != ~0u ? overflowWord : block.firstWord;
      break;
    }
  }

  const float depthFreed =
      enforceKeeps(fontContext, paragraph, placedBlocks,
                   options.nextMeasure > 0 ? options.nextMeasure : lastMeasure,
                   result);
  result.lineCount = lastLineUsed + 1;
  if (depthFreed > 0) {
    int highestLine = -1;
    for (const PositionedRun& run : result.runs)
      highestLine = std::max(highestLine, run.lineIndex);
    result.lineCount = std::min(result.lineCount, highestLine + 1);
  }
  if (!options.overflow.ellipsis.empty() && result.overflowed())
    applyEllipsis(fontContext, paragraph, intervalSequence, options, result);
  recordGeometry(result);
  distributeInFrame(options.frame, intervalSequence.bandCursor() - depthFreed,
                    result.lineCount, result);
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
