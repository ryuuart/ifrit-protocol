/** @file
 * The paragraph controls: a block's own pitch and the leading kinds that
 * decide it, the one spacing rule, the four indents, the tab stops that
 * align their cell somewhere other than its start, the frame's
 * first-baseline rule and vertical distribution, and the hyphenation limits
 * a block owns against the ones the analysis owns.
 */

#include <gtest/gtest.h>
#include <sigilweave/kit/Hyphenation.h>

#include <algorithm>
#include <cmath>
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
