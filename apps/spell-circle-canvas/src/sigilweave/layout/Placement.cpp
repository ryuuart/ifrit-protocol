/** @file
 * WHERE THE WORDS OF ONE LINE LAND: a word's segments emitted as runs, the
 * bidi reordering that puts them in visual order, the mojikumi a Japanese
 * setting spaces them by, the tab leader set across the gap a stop opened,
 * and the alignment that seats the line inside its interval.
 */

#include <hb.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkTextBlob.h>
#include <unicode/ubidi.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <optional>
#include <vector>

#include "Blobs.h"
#include "ParagraphLayoutInternal.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

namespace detail {

namespace {

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
    if (!segment.shaped->advances.empty())
      return segment.shaped->advances.front();
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

/** Appends one shaped segment at its final straight or transformed position. */
void emitSegment(ParagraphLayout& result, const FlatInterval& flatInterval,
                 const WordSegment& segment, uint32_t wordIndex,
                 float penOffset, const ParagraphLayoutOptions& options,
                 const GlyphFit& fit = {}, float baselineShift = 0) {
  const ShapedWord& shapedWord = *segment.shaped;
  if (shapedWord.glyphs.empty()) return;
  // THE RUN IS SETTLED BEFORE IT IS APPENDED, and then written straight
  // into the vector's own slot: building one beside the vector and handing
  // it over is a second set of stores and a second pass over the same
  // bytes, and the blob handle it carries is reference-counted.
  sk_sp<SkTextBlob> blob;
  SkPoint origin = {0, 0};
  bool transformed = false;
  float advance = shapedWord.advance;
  GlyphFit runFit;
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
    blob =
        fit.plain() ? wordBlob(shapedWord) : buildFittedBlob(shapedWord, fit);
    if (!fit.plain()) {
      runFit = fit;
      advance = advanceUnder(fit, shapedWord);
    }
    // A BASELINE SHIFT lifts the span off its line's baseline and changes
    // nothing else: the advances are the face's own, so the pen is where
    // it was and the shaped run is the shared one.
    origin = flatInterval.interval.origin + SkVector{penOffset, -baselineShift};
  } else if (verticalColumn && segment.form == SegmentForm::kUpright) {
    // Vertical-shaped word: positions already stack down the column.
    blob = wordBlob(shapedWord);
    origin = flatInterval.interval.origin + SkVector{0, penOffset};
  } else if (verticalColumn && segment.form == SegmentForm::kTateChuYoko) {
    // Horizontal run set upright across the column, centred on its axis;
    // penX already points at the run's baseline (see Paragraph::analyze).
    blob = wordBlob(shapedWord);
    origin = flatInterval.interval.origin +
             SkVector{-shapedWord.advance * 0.5f, penOffset};
  } else {
    // Rotated/curved: bake per-glyph transforms (kRotated Latin in a
    // vertical column rotates 90° clockwise here via the interval tangent).
    blob = buildTransformedBlob(shapedWord, flatInterval.interval, penOffset,
                                options.pathText.tangentRotationSteps);
    transformed = true;
  }
  if (!blob) return;
  PositionedRun& run = result.runs.emplace_back();
  run.blob = std::move(blob);
  run.shaped = segment.shaped.get();
  run.origin = origin;
  run.styleIndex = segment.styleIndex;
  run.wordIndex = wordIndex;
  run.lineIndex = flatInterval.sourceLineIndex;
  run.transformed = transformed;
  run.intervalIndex = flatInterval.index;
  run.penOffset = penOffset;
  run.advance = advance;
  run.fit = runFit;
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
    // every line of a left-to-right text. Sized once and then filled, so
    // the fill is a straight write over a block this vector already owns.
    visualWordOrder.resize(static_cast<size_t>(count));
    std::iota(visualWordOrder.begin(), visualWordOrder.end(), firstWordIndex);
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
    const size_t found = text.find(alignOn, word.textBegin) < word.textEnd
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
                float gapStart, float gapEnd) {
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
  const ShapedWordReference leader =
      shapeWord(fontContext, span.style.shaping, typeface, stop.leader,
                static_cast<ScriptTag>(HB_SCRIPT_COMMON), false, false);
  if (!leader || leader->glyphs.empty() || leader->advance <= 0) return;
  // The leader is shaped HERE and lives nowhere in the paragraph, so the
  // layout keeps the handle its runs borrow from.
  LayoutAccess::retain(result, leader);
  const ShapedWord* const leaderWord = leader.get();
  const auto repeats =
      static_cast<int>(std::floor((gapEnd - gapStart) / leader->advance));
  float pen = gapEnd - static_cast<float>(repeats) * leader->advance;
  for (int repeat = 0; repeat < repeats; ++repeat) {
    PositionedRun run;
    run.blob = wordBlob(*leader);
    run.shaped = leaderWord;
    run.advance = leader->advance;
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

  // HOW THE LINE IS SEATED IN ITS INTERVAL, settled before anything is
  // measured for the seating: what a line has to be walked for — the width
  // it came to, the gaps that could take up slack — is what an aligned or
  // justified line reads, and a line set from its start reads none of it.
  TextAlignment resolvedAlignment = alignment;
  if (resolvedAlignment == TextAlignment::kJustify && lastLine &&
      !options.justification.justifyLastLine)
    resolvedAlignment = options.justification.lastLineAlignment;
  const bool justifying = resolvedAlignment == TextAlignment::kJustify;
  const bool seatedByWidth = justifying ||
                             resolvedAlignment == TextAlignment::kCenter ||
                             resolvedAlignment == TextAlignment::kEnd;

  // WIDTH OF THE CELL EACH TAB OPENS: the text from the tab to the next tab,
  // or to the end of the line. Only a stop that aligns its cell somewhere
  // other than its start reads it, but it costs one backward walk and the
  // walk is over words already measured.
  static thread_local std::vector<float> cellAfter;
  if (hasTab) {
    cellAfter.assign(visualWordOrder.size(), 0.0f);
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
  // Only a justified line spends a gap, and this walks every word it holds.
  int spaceGapCount = 0;
  int ideographicGapCount = 0;
  float stretchableGlue = 0;
  if (!justifying) {
    // No gap moves: the census would answer a question nobody asks.
  } else if (hasTab) {
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

  // WHAT THE LINE CAME TO, which only a line seated by its width reads —
  // and which is another walk over every word the line holds.
  const float naturalLineWidth =
      !seatedByWidth ? 0.0f
      : hasTab
          ? resolvedNaturalWidth
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

  const float extraWidthNatural =
      flatInterval.interval.length - naturalLineWidth + hangAtStart + hangAtEnd;
  const float extraWidth = extraWidthNatural;

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
  const bool extendedJustify = justifying && spendsPastGaps;
  const float wordSpacingDelta =
      extendedJustify ? justification.wordSpacing - 1.0f : 0.0f;

  // WHAT THE LETTER AND GLYPH PASSES ARE MEASURED IN, needed before the
  // gaps are fitted because each pass's DESIRED value widens the line
  // ahead of them exactly as the desired word spacing does: a letter
  // spacing that only got what the gaps could not spend would never reach
  // a line the gaps fit on their own, which is every line of an ordinary
  // measure.
  //
  // The two quantities are the ones the pen walk actually spends the fit
  // on: one letter space after EVERY glyph the line holds, and the scale
  // over the shaped advance those glyphs came to. Anything else — a word's
  // own gap to the next, an inline slot's box — is not the fit's to move,
  // so pricing the passes against it would leave the line short of the
  // measure by exactly what was mispriced.
  const float em = wordFontSize(words[firstWordIndex]);
  float lineGlyphs = 0;
  float lineShapedWidth = 0;
  if (extendedJustify)
    for (uint32_t wordIndex = firstWordIndex; wordIndex < endWordIndex;
         ++wordIndex)
      for (const WordSegment& segment : words[wordIndex].segments()) {
        lineGlyphs += static_cast<float>(segment.shaped->glyphs.size());
        lineShapedWidth += segment.shaped->advance;
      }
  const float desiredLetterSpacing =
      extendedJustify ? justification.letterSpacing * em : 0.0f;
  const float desiredGlyphWidening =
      extendedJustify ? (justification.glyphScale - 1.0f) * lineShapedWidth
                      : 0.0f;
  // WHETHER THE GAPS ARE BOUNDED AT ALL. They open to their stretch limit
  // only where a later pass can spend what they may not; with both of
  // those shut — their limits equal to what they were asked for — a bound
  // on the gaps would leave a hole at the right margin that nothing in the
  // line is allowed to close, and a hole is worse than a wide gap. So a
  // caller who asks only for a rule about lone-word lines, or only for a
  // wider gap to aim at, still gets every other line filled.
  const bool laterPassesHaveRoom =
      extendedJustify &&
      (justification.letterSpacingMinimum * em < desiredLetterSpacing ||
       justification.letterSpacingMaximum * em > desiredLetterSpacing ||
       justification.glyphScaleMinimum < justification.glyphScale ||
       justification.glyphScaleMaximum > justification.glyphScale);

  float startOffset = 0;
  float spaceAdjustment = 0;
  float ideographicAdjustment = 0;
  GlyphFit fit;
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
      // Every pass's desired value widens the line before anything is
      // fitted: they are what the gaps, the letters and the glyphs are
      // AIMED at, and each elasticity is measured from there.
      const float extraWidth =
          extraWidthNatural - wordSpacingDelta * stretchableGlue -
          desiredLetterSpacing * lineGlyphs - desiredGlyphWidening;
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
        // THE GAPS MAY ONLY OPEN TO THEIR STRETCH LIMIT, measured from the
        // width they are aimed at. What they may not take is what the
        // letter and glyph passes are for, and this is the only thing that
        // ever leaves them anything: a line whose gaps could take
        // everything leaves the two passes past them nothing to do.
        if (laterPassesHaveRoom && spaceGapCount > 0) {
          const float spaceStretchLimit = stretchableGlue *
                                          options.justification.wordSpacing /
                                          static_cast<float>(spaceGapCount) *
                                          options.justification.spaceStretch;
          spaceAdjustment = std::min(spaceAdjustment, spaceStretchLimit);
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
      // measured from its own DESIRED value, which widened the line above
      // and is therefore already paid for here, and bounded by its own two
      // limits.
      float residual =
          extraWidth -
          (spaceAdjustment * static_cast<float>(spaceGapCount) +
           ideographicAdjustment * static_cast<float>(ideographicGapCount));

      float letterSpacing = desiredLetterSpacing;
      const bool loneWord = spaceGapCount + ideographicGapCount == 0;
      if (lineGlyphs > 0 && loneWord &&
          justification.singleWord ==
              JustificationOptions::SingleWord::kJustify) {
        // A LINE HOLDING ONE WORD has no gaps at all: asked to justify, it
        // spends the whole measure between its letters and no limit could
        // mean anything, because there is nothing else to spend it on.
        letterSpacing += residual / lineGlyphs;
        residual = 0;
        startOffset = 0;
      } else if (lineGlyphs > 0) {
        const float wanted = letterSpacing + residual / lineGlyphs;
        const float bounded = std::clamp(
            wanted,
            std::min(letterSpacing, justification.letterSpacingMinimum * em),
            std::max(letterSpacing, justification.letterSpacingMaximum * em));
        residual -= (bounded - letterSpacing) * lineGlyphs;
        letterSpacing = bounded;
      }

      float glyphScale = justification.glyphScale;
      if (lineShapedWidth > 0) {
        const float wanted = glyphScale + residual / lineShapedWidth;
        const float bounded = std::clamp(
            wanted, std::min(glyphScale, justification.glyphScaleMinimum),
            std::max(glyphScale, justification.glyphScaleMaximum));
        residual -= (bounded - glyphScale) * lineShapedWidth;
        glyphScale = bounded;
      }

      // WHAT NO PASS COULD SPEND GOES BACK TO THE GAPS. The bound on them
      // rests on ONE claim — that what they may not take, a later pass
      // takes — and a pass standing at its own limit does not take it.
      // Held at their limit anyway, the gaps leave a hole at the right
      // margin with nothing in the line allowed to close it, which is the
      // one thing the bound exists to prevent; so the claim's failure
      // lifts the bound rather than being paid for. A line whose passes
      // all ran out is then set exactly as the gaps alone would have set
      // it, which is what a line with no later pass gets.
      //
      // The IDEOGRAPHIC gaps take none of it: their ceiling is a rule
      // about how far a full-width gap may open and not a claim about
      // another pass, so it stands whatever the passes did — and a line
      // with no space gaps to absorb the rest stays underfull, exactly as
      // it does when the first pass cannot place it.
      if (residual > 0 && spaceGapCount > 0)
        spaceAdjustment += residual / static_cast<float>(spaceGapCount);

      fit = {letterSpacing, glyphScale};
      break;
    }
  }

  // WHETHER THE GAPS MOVE AT ALL. Nothing was added to any of them on a
  // line the fit left alone, so the pen walk below need not ask what kind
  // of gap each one is — which is a question about two words per gap.
  const bool gapsMove = wordSpacingDelta != 0 || spaceAdjustment != 0 ||
                        ideographicAdjustment != 0;

  // The hang itself: the line starts one hang back, so the character that
  // may hang sits outside the measure and the letters after it square on
  // it. A justified line spent the extra room the hang opened, so its
  // interior is already correct.
  float penPosition = startOffset - hangAtStart;
  const std::vector<StyleSpan>& spans = paragraph.spans();
  const auto shiftOf = [&](const WordSegment& segment) {
    return segment.styleIndex < spans.size()
               ? spans[segment.styleIndex].style.paint.baselineShift
               : 0.0f;
  };
  for (size_t visualIndex = 0; visualIndex < visualWordOrder.size();
       ++visualIndex) {
    const uint32_t wordIndex = visualWordOrder[visualIndex];
    const Word& word = words[wordIndex];
    float wordAdvance = word.width;
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
        local += advanceUnder(fit, *segment.shaped);
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
                       wordIndex, *resolved.stop, gapStart, penPosition);
          continue;
        }
      }
      penPosition += glueAfter(word, penPosition - startOffset, options) +
                     roomAfter(wordIndex);
      if (gapsMove && static_cast<int>(visualIndex) > lastTabVisualIndex) {
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

}  // namespace detail

}  // namespace sigil::weave
