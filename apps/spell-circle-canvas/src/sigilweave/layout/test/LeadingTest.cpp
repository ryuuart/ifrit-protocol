/** @file
 * A block's own pitch and what opens it: the four leading kinds, the one
 * rule that decides the gap between two blocks, the four indents, an
 * alignment set per block, half-leading, and a span lifted off the
 * baseline it sits on.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

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
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

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
  EXPECT_GT(
      baselines(layoutParagraph(fonts, narrow, narrowFlow, options)).size(),
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
