/** @file
 * The paragraph controls: a block's own pitch and the leading kinds that
 * decide it, the one spacing rule, the four indents, the tab stops that
 * align their cell somewhere other than its start, the frame's
 * first-baseline rule and vertical distribution, and the hyphenation limits
 * a block owns against the ones the analysis owns.
 */

#include <gtest/gtest.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/kit/LineTables.h>
#include <sigilweave/layout/Beside.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// Ascending distinct baselines of the placed lines.
std::vector<float> baselines(const ParagraphLayout& layout) {
  std::vector<float> found;
  for (const PositionedRun& run : layout.runs) {
    if (run.transformed) continue;
    if (std::find(found.begin(), found.end(), run.origin.y()) == found.end())
      found.push_back(run.origin.y());
  }
  std::sort(found.begin(), found.end());
  return found;
}

/// Leftmost run origin on each line, ascending by line index.
std::vector<float> lineStarts(const ParagraphLayout& layout) {
  std::vector<std::pair<int, float>> byLine;
  for (const PositionedRun& run : layout.runs) {
    auto found = std::find_if(byLine.begin(), byLine.end(),
                              [&](const std::pair<int, float>& entry) {
                                return entry.first == run.lineIndex;
                              });
    if (found == byLine.end())
      byLine.emplace_back(run.lineIndex, run.origin.x());
    else
      found->second = std::min(found->second, run.origin.x());
  }
  std::sort(byLine.begin(), byLine.end());
  std::vector<float> starts;
  for (const auto& [line, start] : byLine) starts.push_back(start);
  return starts;
}

/// Two blocks, the second after a hard break.
Paragraph twoBlocks() {
  return makeParagraph(
      u8"The first block runs on for several words so that it wraps.\n"
      u8"The second block does the same and wraps as well.",
      16.0f);
}

}  // namespace

// ── Leading ───────────────────────────────────────────────────────────────

TEST(ParagraphStyle, FaceLeadingIsWhatAnUnstyledTextGets) {
  FontContext& fonts = sharedContext();
  Paragraph plain = makeParagraph(u8"one two three four five six seven eight");
  Paragraph styled = makeParagraph(u8"one two three four five six seven eight");
  BlockFlow flowA(SkRect::MakeWH(120, 400));
  BlockFlow flowB(SkRect::MakeWH(120, 400));

  const ParagraphLayout bare = layoutParagraph(fonts, plain, flowA);
  ParagraphLayoutOptions options;
  options.blocks = {ParagraphStyle{}};
  const ParagraphLayout withEmptyStyle =
      layoutParagraph(fonts, styled, flowB, options);

  EXPECT_EQ(baselines(bare), baselines(withEmptyStyle));
  EXPECT_FLOAT_EQ(bare.linePitch, withEmptyStyle.linePitch);
}

TEST(ParagraphStyle, MultipleLeadingOpensThePitch) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three four five six seven");
  BlockFlow flow(SkRect::MakeWH(120, 900));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.leading = Leading::multiple(2.0f);
  options.blocks = {style};
  const ParagraphLayout layout = layoutParagraph(fonts, paragraph, flow, options);

  const std::vector<float> lines = baselines(layout);
  ASSERT_GE(lines.size(), 3u);
  const float step = lines[1] - lines[0];
  EXPECT_NEAR(step, lines[2] - lines[1], 0.01f);
  // Twice the face's own height, and the extra opened ABOVE the first line.
  const Paragraph::Strut strut = paragraph.strutAt(fonts, 0);
  EXPECT_NEAR(step, strut.height * 2.0f, 0.01f);
  EXPECT_NEAR(lines[0], strut.ascent + strut.height, 0.01f);
}

TEST(ParagraphStyle, AbsoluteLeadingStatesThePitchOutright) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three four five six seven");
  BlockFlow flow(SkRect::MakeWH(120, 900));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.leading = Leading::absolute(40.0f);
  options.blocks = {style};
  const std::vector<float> lines =
      baselines(layoutParagraph(fonts, paragraph, flow, options));
  ASSERT_GE(lines.size(), 2u);
  EXPECT_NEAR(lines[1] - lines[0], 40.0f, 0.01f);
}

TEST(ParagraphStyle, GridLeadingLandsTwoBlocksOnOneRhythm) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(160, 900));
  ParagraphLayoutOptions options;
  ParagraphStyle grid;
  grid.leading = Leading::grid(24.0f);
  options.blocks = {grid, grid};
  const std::vector<float> lines =
      baselines(layoutParagraph(fonts, paragraph, flow, options));
  ASSERT_GE(lines.size(), 4u);
  for (size_t index = 1; index < lines.size(); ++index) {
    const float step = lines[index] - lines[index - 1];
    EXPECT_NEAR(std::fmod(step + 0.01f, 24.0f), 0.01f, 0.05f)
        << "step " << step << " is not a whole number of grid lines";
  }
}

// ── Spacing ───────────────────────────────────────────────────────────────

TEST(ParagraphStyle, TheGapIsTheLargerOfAfterAndBefore) {
  FontContext& fonts = sharedContext();
  ParagraphLayoutOptions options;
  ParagraphStyle first;
  first.spaceAfter = 30.0f;
  ParagraphStyle second;
  second.spaceBefore = 12.0f;
  options.blocks = {first, second};

  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(160, 900));
  const ParagraphLayout spaced =
      layoutParagraph(fonts, paragraph, flow, options);

  Paragraph plain = twoBlocks();
  BlockFlow plainFlow(SkRect::MakeWH(160, 900));
  const ParagraphLayout bare = layoutParagraph(fonts, plain, plainFlow);

  const std::vector<float> spacedLines = baselines(spaced);
  const std::vector<float> bareLines = baselines(bare);
  ASSERT_EQ(spacedLines.size(), bareLines.size());
  ASSERT_GE(spacedLines.size(), 4u);
  // Nothing before the block boundary has moved, and everything after it has
  // moved down by the LARGER of the two, which is the block before's
  // spaceAfter rather than the sum or the block after's spaceBefore.
  size_t moved = 0;
  while (moved < spacedLines.size() &&
         std::abs(spacedLines[moved] - bareLines[moved]) < 0.01f)
    ++moved;
  ASSERT_LT(moved, spacedLines.size());
  EXPECT_GT(moved, 0u);
  for (size_t index = moved; index < spacedLines.size(); ++index)
    EXPECT_NEAR(spacedLines[index] - bareLines[index], 30.0f, 0.01f);
}

TEST(ParagraphStyle, SpaceBeforeIsNotSuppressedAtTheHeadOfTheFlow) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three");
  BlockFlow flow(SkRect::MakeWH(300, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.spaceBefore = 20.0f;
  options.blocks = {style};
  const std::vector<float> lines =
      baselines(layoutParagraph(fonts, paragraph, flow, options));
  ASSERT_FALSE(lines.empty());
  const Paragraph::Strut strut = paragraph.strutAt(fonts, 0);
  EXPECT_NEAR(lines[0], 20.0f + strut.ascent, 0.01f);
}

// ── Indents ───────────────────────────────────────────────────────────────

TEST(ParagraphStyle, FirstLineIndentShortensOnlyTheFirstLine) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three four five six seven");
  BlockFlow flow(SkRect::MakeWH(140, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.indent.firstLine = 24.0f;
  options.blocks = {style};
  const std::vector<float> starts =
      lineStarts(layoutParagraph(fonts, paragraph, flow, options));
  ASSERT_GE(starts.size(), 2u);
  EXPECT_NEAR(starts[0], 24.0f, 0.01f);
  EXPECT_NEAR(starts[1], 0.0f, 0.01f);
}

TEST(ParagraphStyle, StartIndentMovesEveryLine) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three four five six seven");
  BlockFlow flow(SkRect::MakeWH(140, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.indent.start = 18.0f;
  options.blocks = {style};
  const std::vector<float> starts =
      lineStarts(layoutParagraph(fonts, paragraph, flow, options));
  ASSERT_GE(starts.size(), 2u);
  for (const float start : starts) EXPECT_NEAR(start, 18.0f, 0.01f);
}

TEST(ParagraphStyle, HangingIndentPullsTheFirstLineOut) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three four five six seven");
  BlockFlow flow(SkRect::MakeWH(160, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.indent.start = 30.0f;
  style.indent.firstLine = -30.0f;
  options.blocks = {style};
  const std::vector<float> starts =
      lineStarts(layoutParagraph(fonts, paragraph, flow, options));
  ASSERT_GE(starts.size(), 2u);
  EXPECT_NEAR(starts[0], 0.0f, 0.01f);
  EXPECT_NEAR(starts[1], 30.0f, 0.01f);
}

TEST(ParagraphStyle, EndIndentShortensTheMeasure) {
  FontContext& fonts = sharedContext();
  Paragraph wide = makeParagraph(u8"one two three four five six seven eight");
  Paragraph narrow = makeParagraph(u8"one two three four five six seven eight");
  BlockFlow wideFlow(SkRect::MakeWH(200, 600));
  BlockFlow narrowFlow(SkRect::MakeWH(200, 600));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.indent.end = 90.0f;
  options.blocks = {style};
  EXPECT_GT(baselines(layoutParagraph(fonts, narrow, narrowFlow, options)).size(),
            baselines(layoutParagraph(fonts, wide, wideFlow)).size());
}

// ── Per-block overrides ───────────────────────────────────────────────────

TEST(ParagraphStyle, AlignmentIsPerBlock) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(400, 600));
  ParagraphLayoutOptions options;
  ParagraphStyle centred;
  centred.alignment = TextAlignment::kCenter;
  options.blocks = {ParagraphStyle{}, centred};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  const std::vector<float> starts = lineStarts(layout);
  ASSERT_GE(starts.size(), 2u);
  EXPECT_NEAR(starts.front(), 0.0f, 0.01f);
  EXPECT_GT(starts.back(), 1.0f);
}

// ── Tab stops ─────────────────────────────────────────────────────────────

TEST(TabStops, EndAlignedStopPinsTheCellsEnd) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"Chapter\t12");
  BlockFlow flow(SkRect::MakeWH(400, 200));
  ParagraphLayoutOptions options;
  options.tabStops.stops = {TabStop{300.0f, TabStop::Align::kEnd}};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  ASSERT_GE(layout.runs.size(), 2u);
  const PositionedRun& figures = layout.runs.back();
  EXPECT_NEAR(runEnd(paragraph, figures), 300.0f, 0.5f);
}

TEST(TabStops, CharacterAlignedStopPinsTheDecimalPoint) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"Total\t1284.50");
  BlockFlow flow(SkRect::MakeWH(400, 200));
  ParagraphLayoutOptions options;
  options.tabStops.stops = {TabStop{280.0f, TabStop::Align::kCharacter, u'.'}};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  ASSERT_GE(layout.runs.size(), 2u);
  // The figures start left of the stop and reach past it: the point sits on
  // it, so neither edge does.
  const PositionedRun& figures = layout.runs.back();
  EXPECT_LT(figures.origin.x(), 280.0f);
  EXPECT_GT(runEnd(paragraph, figures), 280.0f);
}

TEST(TabStops, LeaderFillsTheGapItOpened) {
  FontContext& fonts = sharedContext();
  Paragraph plain = makeParagraph(u8"Chapter\t12");
  Paragraph led = makeParagraph(u8"Chapter\t12");
  BlockFlow flowA(SkRect::MakeWH(400, 200));
  BlockFlow flowB(SkRect::MakeWH(400, 200));
  ParagraphLayoutOptions bare;
  bare.tabStops.stops = {TabStop{300.0f}};
  ParagraphLayoutOptions dotted = bare;
  dotted.tabStops.stops.front().leader = u".";
  const size_t bareRuns =
      layoutParagraph(fonts, plain, flowA, bare).runs.size();
  const size_t dottedRuns =
      layoutParagraph(fonts, led, flowB, dotted).runs.size();
  EXPECT_GT(dottedRuns, bareRuns + 5);
}

// ── The frame ─────────────────────────────────────────────────────────────

TEST(FrameSeating, FixedFirstBaselineMovesTheWholePassage) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(u8"one two three four five six seven");
  BlockFlow flow(SkRect::MakeWH(120, 600));
  ParagraphLayoutOptions options;
  options.frame.firstBaseline = FrameOptions::FirstBaseline::kFixed;
  options.frame.firstBaselineOffset = 60.0f;
  const std::vector<float> lines =
      baselines(layoutParagraph(fonts, paragraph, flow, options));
  ASSERT_GE(lines.size(), 2u);
  EXPECT_NEAR(lines[0], 60.0f, 0.01f);
  const Paragraph::Strut strut = paragraph.strutAt(fonts, 0);
  EXPECT_NEAR(lines[1] - lines[0], strut.height, 0.01f);
}

TEST(FrameSeating, CentredDistributionHalvesTheLeftoverRoom) {
  FontContext& fonts = sharedContext();
  Paragraph plain = makeParagraph(u8"one two three four");
  Paragraph centred = makeParagraph(u8"one two three four");
  BlockFlow flowA(SkRect::MakeWH(120, 400));
  BlockFlow flowB(SkRect::MakeWH(120, 400));
  const std::vector<float> bare =
      baselines(layoutParagraph(fonts, plain, flowA));
  ParagraphLayoutOptions options;
  options.frame.extent = 400.0f;
  options.frame.distribute = FrameOptions::Distribute::kCenter;
  const std::vector<float> moved =
      baselines(layoutParagraph(fonts, centred, flowB, options));
  ASSERT_EQ(bare.size(), moved.size());
  ASSERT_FALSE(bare.empty());
  const float shift = moved.front() - bare.front();
  EXPECT_GT(shift, 100.0f);
  for (size_t index = 0; index < bare.size(); ++index)
    EXPECT_NEAR(moved[index] - bare[index], shift, 0.01f);
}

// ── Hyphenation ───────────────────────────────────────────────────────────

TEST(Hyphenation, PatternsOpenBreaksInsideWords) {
  static const kit::PatternHyphenator hyphenator("en", kit::patterns::english());
  EXPECT_GT(hyphenator.patternCount(), 100u);
  std::vector<uint32_t> points;
  hyphenator.breakPoints(u"hyphenation", "en-US", points);
  EXPECT_FALSE(points.empty());
  for (const uint32_t offset : points) {
    EXPECT_GT(offset, 0u);
    EXPECT_LT(offset, 11u);
  }
}

TEST(Hyphenation, ATableDeclinesALanguageItDoesNotAnswerFor) {
  static const kit::PatternHyphenator hyphenator("en", kit::patterns::english());
  std::vector<uint32_t> points;
  hyphenator.breakPoints(u"hyphenation", "de-DE", points);
  EXPECT_TRUE(points.empty());
  hyphenator.breakPoints(u"hyphenation", "", points);
  EXPECT_TRUE(points.empty());
}

TEST(Hyphenation, AnExceptionOverridesThePatterns) {
  static const kit::PatternHyphenator hyphenator("en", kit::patterns::english());
  std::vector<uint32_t> points;
  hyphenator.breakPoints(u"present", "en", points);
  EXPECT_TRUE(points.empty());  // "present" is listed unbroken
}

TEST(Hyphenation, PatternBreaksReachTheWordList) {
  FontContext& fonts = sharedContext();
  static const kit::PatternHyphenator hyphenator("en", kit::patterns::english());
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  Paragraph paragraph;
  paragraph.appendText(u8"hyphenation", style);
  BlockFlow flow(SkRect::MakeWH(40, 400));
  ParagraphLayoutOptions options;
  options.hyphenation.patterns = &hyphenator;
  layoutParagraph(fonts, paragraph, flow, options);
  int opportunities = 0;
  for (const Word& word : paragraph.words())
    if (word.hyphenBreak) ++opportunities;
  EXPECT_GT(opportunities, 0);
}

TEST(Hyphenation, TheMinimumWordLengthIsSettledInTheAnalysis) {
  FontContext& fonts = sharedContext();
  static const kit::PatternHyphenator hyphenator("en", kit::patterns::english());
  TextStyle style = basicStyle(16.0f);
  style.shaping.languageTag = "en-US";
  Paragraph paragraph;
  paragraph.appendText(u8"hyphenation", style);
  BlockFlow flow(SkRect::MakeWH(40, 400));
  ParagraphLayoutOptions options;
  options.hyphenation.patterns = &hyphenator;
  options.hyphenation.limits.minimumWordLength = 40;
  layoutParagraph(fonts, paragraph, flow, options);
  for (const Word& word : paragraph.words()) EXPECT_FALSE(word.hyphenBreak);
}

// ── The line's edges ──────────────────────────────────────────────────────

TEST(LineEdges, HangingPunctuationPullsALineBackPastItsStart) {
  FontContext& fonts = sharedContext();
  Paragraph plain = makeParagraph(u8"“quoted opening words here”");
  Paragraph hung = makeParagraph(u8"“quoted opening words here”");
  BlockFlow flowA(SkRect::MakeWH(400, 200));
  BlockFlow flowB(SkRect::MakeWH(400, 200));
  const std::vector<float> square = lineStarts(layoutParagraph(fonts, plain, flowA));
  ParagraphLayoutOptions options;
  options.hanging = kit::hanging::latin();
  const std::vector<float> optical =
      lineStarts(layoutParagraph(fonts, hung, flowB, options));
  ASSERT_FALSE(square.empty());
  ASSERT_EQ(square.size(), optical.size());
  // The quote stands OUTSIDE the measure, so the line begins left of zero.
  EXPECT_LT(optical.front(), square.front() - 1.0f);
}

TEST(LineEdges, ATextWithNoTableIsSquaredOnItsAdvances) {
  FontContext& fonts = sharedContext();
  Paragraph plain = makeParagraph(u8"“quoted opening words here”");
  BlockFlow flow(SkRect::MakeWH(400, 200));
  const std::vector<float> starts = lineStarts(layoutParagraph(fonts, plain, flow));
  ASSERT_FALSE(starts.empty());
  EXPECT_NEAR(starts.front(), 0.0f, 0.01f);
}

TEST(LineEdges, KinsokuNeverOpensALineWithAProhibitedCharacter) {
  FontContext& fonts = sharedContext();
  // A comma that would otherwise begin a column: the prohibition drops the
  // boundary before it, so the character before comes down with it.
  const std::u8string passage =
      u8"これは本文です、そして"
      u8"続きます。";
  Paragraph bare = makeParagraph(passage, 20.0f);
  Paragraph ruled = makeParagraph(passage, 20.0f);
  ParagraphLayoutOptions options;
  options.kinsoku = kit::kinsoku::japanese();
  BlockFlow flowA(SkRect::MakeWH(126, 300));
  BlockFlow flowB(SkRect::MakeWH(126, 300));
  layoutParagraph(fonts, bare, flowA);
  layoutParagraph(fonts, ruled, flowB, options);
  // The prohibited characters never open a Word under the table, because
  // the boundary that would have opened one was never opened.
  const KinsokuTable table = kit::kinsoku::japanese();
  for (const Word& word : ruled.words())
    if (word.textEnd > word.textBegin && word.textBegin > 0)
      EXPECT_EQ(table.notLineStart.find(ruled.text()[word.textBegin]),
                std::u16string::npos);
}

TEST(LineEdges, KinsokuDropsTheBoundaryBeforeAProhibitedCharacter) {
  FontContext& fonts = sharedContext();
  // Two ideographs with a UAX #14 boundary between them, and a table that
  // forbids the second from opening a line: the boundary goes, and the two
  // become one unbreakable word.
  const std::u8string passage =
      u8"\xe6\x9c\xac\xe6\x96\x87\xe6\x9c\xac\xe6\x96\x87";
  Paragraph bare = makeParagraph(passage, 20.0f);
  Paragraph ruled = makeParagraph(passage, 20.0f);
  ParagraphLayoutOptions options;
  options.kinsoku.notLineStart = u"\u6587";
  BlockFlow flowA(SkRect::MakeWH(126, 300));
  BlockFlow flowB(SkRect::MakeWH(126, 300));
  layoutParagraph(fonts, bare, flowA);
  layoutParagraph(fonts, ruled, flowB, options);
  EXPECT_LT(ruled.words().size(), bare.words().size());
  for (const Word& word : ruled.words())
    if (word.textBegin > 0)
      EXPECT_NE(ruled.text()[word.textBegin], u'\u6587');
}

TEST(ParagraphStyle, HalfLeadingPutsHalfTheOpenedRoomUnderTheLine) {
  FontContext& fonts = sharedContext();
  Paragraph above = makeParagraph(u8"one two three four five six seven");
  Paragraph split = makeParagraph(u8"one two three four five six seven");
  BlockFlow flowA(SkRect::MakeWH(120, 900));
  BlockFlow flowB(SkRect::MakeWH(120, 900));
  ParagraphStyle style;
  style.leading = Leading::multiple(2.0f);
  ParagraphLayoutOptions allAbove;
  allAbove.blocks = {style};
  ParagraphStyle halved = style;
  halved.halfLeading = true;
  ParagraphLayoutOptions halfOptions;
  halfOptions.blocks = {halved};
  const std::vector<float> high =
      baselines(layoutParagraph(fonts, above, flowA, allAbove));
  const std::vector<float> centred =
      baselines(layoutParagraph(fonts, split, flowB, halfOptions));
  ASSERT_GE(high.size(), 2u);
  ASSERT_EQ(high.size(), centred.size());
  const Paragraph::Strut strut = above.strutAt(fonts, 0);
  // The pitch is the same; only where the type sits inside it moves, by
  // exactly half the room the leading opened.
  EXPECT_NEAR(high[1] - high[0], centred[1] - centred[0], 0.01f);
  EXPECT_NEAR(high[0] - centred[0], strut.height * 0.5f, 0.01f);
}

TEST(ParagraphStyle, ABaselineShiftLiftsASpanAndCostsNoReshape) {
  FontContext& fonts = sharedContext();
  TextStyle base = basicStyle(16.0f);
  TextStyle lifted = base;
  lifted.paint.baselineShift = 6.0f;
  Paragraph paragraph;
  paragraph.appendText(u8"level ", base);
  paragraph.appendText(u8"lifted", lifted);
  BlockFlow flow(SkRect::MakeWH(400, 200));
  const ParagraphLayout layout = layoutParagraph(fonts, paragraph, flow);
  ASSERT_GE(layout.runs.size(), 2u);
  float levelBaseline = 0;
  float liftedBaseline = 0;
  for (const PositionedRun& run : layout.runs) {
    const uint32_t begin = paragraph.words()[run.wordIndex].textBegin;
    if (begin == 0) levelBaseline = run.origin.y();
    if (begin >= 6) liftedBaseline = run.origin.y();
  }
  EXPECT_NEAR(levelBaseline - liftedBaseline, 6.0f, 0.01f);
  // The advances are the face's own either way, so the two spans share
  // every shaped entry a shift-free text would have produced.
  EXPECT_TRUE(allGlyphsResolved(paragraph));
}

// ── A reading set beside a base ───────────────────────────────────────────

TEST(Beside, TheBandAReadingNeedsIsItsOwnStrutPlusTheGap) {
  FontContext& fonts = sharedContext();
  const float small = bandBeside(fonts, basicStyle(8.0f), 0.0f);
  const float large = bandBeside(fonts, basicStyle(24.0f), 0.0f);
  EXPECT_GT(large, small * 2.0f);
  EXPECT_NEAR(bandBeside(fonts, basicStyle(8.0f), 5.0f), small + 5.0f, 0.01f);
}

TEST(Beside, AReadingStandsCentredOnItsBaseAndClearOfIt) {
  FontContext& fonts = sharedContext();
  Paragraph reading = makeParagraph(u8"note", 9.0f);
  const SkRect base = SkRect::MakeXYWH(100, 200, 60, 20);
  const ParagraphLayout above =
      layoutBeside(fonts, reading, {.base = base,
                                    .writingMode = WritingMode::kHorizontal,
                                    .side = Beside::Side::Before,
                                    .gap = 3.0f});
  ASSERT_FALSE(above.runs.empty());
  const float width = reading.naturalWidth(fonts);
  EXPECT_NEAR(above.runs.front().origin.x(), base.centerX() - width * 0.5f,
              0.5f);
  EXPECT_NEAR(above.runs.front().origin.y(), base.top() - 3.0f, 0.5f);
}

TEST(Beside, AColumnReadsItsFurnitureOnTheRight) {
  FontContext& fonts = sharedContext();
  Paragraph reading = makeParagraph(u8"\xe3\x81\xbb", 9.0f);
  const SkRect base = SkRect::MakeXYWH(100, 200, 24, 60);
  const ParagraphLayout beside =
      layoutBeside(fonts, reading, {.base = base,
                                    .writingMode = WritingMode::kVerticalRL,
                                    .side = Beside::Side::Before,
                                    .gap = 2.0f});
  ASSERT_FALSE(beside.runs.empty());
  EXPECT_GT(beside.runs.front().origin.x(), base.right());
}

TEST(Beside, ABrokenBaseSharesItsReadingByAdvance) {
  // Half the advance either side takes half the reading; all of it on one
  // side takes all of it; and no advance at all leaves the reading whole
  // rather than empty.
  EXPECT_EQ(shareOfReading(u"abcd", 1.0f, 1.0f), u"ab");
  EXPECT_EQ(shareOfReading(u"abcd", 3.0f, 1.0f), u"abc");
  EXPECT_EQ(shareOfReading(u"abcd", 1.0f, 0.0f), u"abcd");
  EXPECT_EQ(shareOfReading(u"abcd", 0.0f, 0.0f), u"abcd");
}

// ── Keeps at the frame boundary ───────────────────────────────────────────

namespace {

/// Where the second block of twoBlocks() begins, in UTF-16 units.
constexpr uint32_t kSecondBlockText =
    sizeof("The first block runs on for several words so that it wraps.") - 1;

/// How many distinct lines the words at or past `textBegin` landed on.
int linesFrom(const ParagraphLayout& layout, const Paragraph& paragraph,
              uint32_t textBegin) {
  std::vector<int> lines;
  for (const PositionedRun& run : layout.runs) {
    if (paragraph.words()[run.wordIndex].textBegin < textBegin) continue;
    if (std::find(lines.begin(), lines.end(), run.lineIndex) == lines.end())
      lines.push_back(run.lineIndex);
  }
  return static_cast<int>(lines.size());
}

/// The first word of the block starting at `textBegin`.
uint32_t firstWordAt(const Paragraph& paragraph, uint32_t textBegin) {
  for (uint32_t index = 0; index < paragraph.words().size(); ++index)
    if (paragraph.words()[index].textBegin >= textBegin) return index;
  return 0;
}

/// A frame `lines` deep: the geometry holds the whole text and the clamp
/// says where the frame ends.
ParagraphLayoutOptions framedTo(int lines) {
  ParagraphLayoutOptions options;
  options.overflow.maxLines = lines;
  return options;
}

}  // namespace

TEST(Keeps, AnOrphanTooShortToStandMovesItsWholeBlockOver) {
  FontContext& fonts = sharedContext();
  Paragraph measured = twoBlocks();
  BlockFlow open(SkRect::MakeWH(150, 600));
  const ParagraphLayout whole = layoutParagraph(fonts, measured, open);
  const int firstBlockLines =
      whole.lineCount - linesFrom(whole, measured, kSecondBlockText);
  ASSERT_GT(firstBlockLines, 0);

  // A frame with room for one line of the second block: without a keep it
  // takes that line, with an orphan rule of two it takes none.
  Paragraph loose = twoBlocks();
  BlockFlow flowA(SkRect::MakeWH(150, 600));
  const ParagraphLayout unkept =
      layoutParagraph(fonts, loose, flowA, framedTo(firstBlockLines + 1));
  EXPECT_EQ(linesFrom(unkept, loose, kSecondBlockText), 1);

  Paragraph kept = twoBlocks();
  BlockFlow flowB(SkRect::MakeWH(150, 600));
  ParagraphLayoutOptions options = framedTo(firstBlockLines + 1);
  ParagraphStyle second;
  second.keep.orphanLines = 2;
  options.blocks = {ParagraphStyle{}, second};
  const ParagraphLayout layout =
      layoutParagraph(fonts, kept, flowB, options);
  EXPECT_EQ(linesFrom(layout, kept, kSecondBlockText), 0);
  EXPECT_EQ(layout.firstUnplacedWord, firstWordAt(kept, kSecondBlockText));
}

TEST(Keeps, BothBreakersEnforceTheSameKeepBecauseNoBreakIsReDecided) {
  FontContext& fonts = sharedContext();
  Paragraph measured = twoBlocks();
  BlockFlow open(SkRect::MakeWH(150, 600));
  const int firstBlockLines =
      layoutParagraph(fonts, measured, open).lineCount -
      linesFrom(layoutParagraph(fonts, measured, open), measured,
                kSecondBlockText);

  ParagraphStyle second;
  second.keep.orphanLines = 2;
  const auto fillWith = [&](LineBreakStrategy strategy) {
    Paragraph paragraph = twoBlocks();
    BlockFlow flow(SkRect::MakeWH(150, 600));
    ParagraphLayoutOptions options = framedTo(firstBlockLines + 1);
    options.lineBreakStrategy = strategy;
    options.blocks = {ParagraphStyle{}, second};
    return layoutParagraph(fonts, paragraph, flow, options).firstUnplacedWord;
  };
  EXPECT_EQ(fillWith(LineBreakStrategy::kGreedy),
            fillWith(LineBreakStrategy::kKnuthPlass));
}

TEST(Keeps, WithNextTakesTheBlockThatEndedAtTheBoundaryWithIt) {
  FontContext& fonts = sharedContext();
  // Three blocks: a body, a heading that asks to keep with what follows,
  // and the body under it.
  const std::u8string text =
      u8"An opening block that runs on for enough words to wrap twice.\n"
      u8"A heading.\n"
      u8"The block the heading introduces, which also wraps.";
  constexpr uint32_t kHeadingText =
      sizeof("An opening block that runs on for enough words to wrap twice.") -
      1;
  constexpr uint32_t kThirdBlockText =
      kHeadingText + sizeof("A heading.") - 1;

  Paragraph measured = makeParagraph(text, 16.0f);
  BlockFlow open(SkRect::MakeWH(150, 600));
  const ParagraphLayout whole = layoutParagraph(fonts, measured, open);
  const int throughHeading =
      whole.lineCount - linesFrom(whole, measured, kThirdBlockText);
  ASSERT_GT(throughHeading, 1);

  const auto fillWith = [&](bool withNext) {
    Paragraph paragraph = makeParagraph(text, 16.0f);
    BlockFlow flow(SkRect::MakeWH(150, 600));
    ParagraphLayoutOptions options = framedTo(throughHeading);
    ParagraphStyle heading;
    heading.keep.withNext = withNext;
    options.blocks = {ParagraphStyle{}, heading};
    return layoutParagraph(fonts, paragraph, flow, options);
  };
  // Without the keep the heading sits alone at the foot of the frame.
  EXPECT_GT(linesFrom(fillWith(false), measured, kHeadingText), 0);
  // With it, the heading goes over to stand above its own block.
  const ParagraphLayout kept = fillWith(true);
  EXPECT_EQ(linesFrom(kept, measured, kHeadingText), 0);
  EXPECT_EQ(kept.firstUnplacedWord, firstWordAt(measured, kHeadingText));
  EXPECT_FALSE(kept.runs.empty());
}

TEST(Keeps, StartInNextFrameEndsTheFillBeforeTheBlock) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(150, 600));
  ParagraphLayoutOptions options;
  ParagraphStyle second;
  second.keep.startInNextFrame = true;
  options.blocks = {ParagraphStyle{}, second};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_EQ(linesFrom(layout, paragraph, kSecondBlockText), 0);
  EXPECT_EQ(layout.firstUnplacedWord, firstWordAt(paragraph, kSecondBlockText));
}

TEST(Keeps, AKeepNeverEmptiesAFrame) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"A single block whose lines cannot go anywhere but here.", 16.0f);
  BlockFlow flow(SkRect::MakeWH(150, 600));
  ParagraphLayoutOptions options = framedTo(1);
  ParagraphStyle only;
  only.keep.orphanLines = 4;
  only.keep.allLinesTogether = true;
  options.blocks = {only};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  // Retracting the one line it placed would hand the next frame exactly
  // what emptied this one, so the fill stands.
  EXPECT_FALSE(layout.runs.empty());
  EXPECT_TRUE(layout.overflowed());
}

TEST(Keeps, AWidowIsCountedInTheLinesTheNextFrameWouldGet) {
  FontContext& fonts = sharedContext();
  Paragraph measured = makeParagraph(
      u8"One block long enough to fill several lines of a narrow frame and "
      u8"leave a short remainder behind it.",
      16.0f);
  BlockFlow open(SkRect::MakeWH(150, 600));
  const int total = layoutParagraph(fonts, measured, open).lineCount;
  ASSERT_GT(total, 3);

  // A frame one line short of the whole block carries one line over; a
  // widow rule of two pulls a second line back out of the frame.
  const auto placedLines = [&](int widowLines) {
    Paragraph paragraph = makeParagraph(
        u8"One block long enough to fill several lines of a narrow frame and "
        u8"leave a short remainder behind it.",
        16.0f);
    BlockFlow flow(SkRect::MakeWH(150, 600));
    ParagraphLayoutOptions options = framedTo(total - 1);
    ParagraphStyle style;
    style.keep.widowLines = widowLines;
    options.blocks = {style};
    return layoutParagraph(fonts, paragraph, flow, options).lineCount;
  };
  EXPECT_EQ(placedLines(0), total - 1);
  EXPECT_EQ(placedLines(2), total - 2);
}
