/** @file
 * A block's own pitch and what opens it: the four leading kinds, the face's
 * own height a caller can ask for by itself, the one rule that decides the
 * gap between two blocks, the four indents, an alignment set per block,
 * half-leading, and a span lifted off the baseline it sits on.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "sigilweave/fonts/Shaper.h"
#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// The line the second block of `twoBlocks()` opens on: the line of the
/// first run whose word starts past the hard break.
int secondBlockLine(const Paragraph& paragraph, const ParagraphLayout& layout) {
  const uint32_t secondBlock =
      static_cast<uint32_t>(paragraph.text().find(u'\n')) + 1;
  for (const PositionedRun& run : layout.runs)
    if (paragraph.words()[run.wordIndex].textBegin >= secondBlock)
      return run.lineIndex;
  return -1;
}

/// The baseline of each line, ascending by line index.
std::vector<float> baselinesByLine(const ParagraphLayout& layout) {
  std::vector<float> byLine;
  for (const PositionedRun& run : layout.runs) {
    if (run.lineIndex < 0) continue;
    if (byLine.size() <= static_cast<size_t>(run.lineIndex))
      byLine.resize(static_cast<size_t>(run.lineIndex) + 1, 0.0f);
    byLine[static_cast<size_t>(run.lineIndex)] = run.origin.y();
  }
  return byLine;
}

}  // namespace

// ── Leading ───────────────────────────────────────────────────────────────

TEST(ParagraphStyle, FaceLeadingIsWhatAnUnstyledTextGets) {
  FontContext& fonts = sigil::test::fonts();
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

TEST(ParagraphStyle, LineHeightOfAStyleIsTheHeightFaceLeadingSetsThePitchTo) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"one two three four five six seven");
  BlockFlow flow(SkRect::MakeWH(120, 900));
  const std::vector<float> lines =
      baselines(layoutParagraph(fonts, paragraph, flow));
  ASSERT_GE(lines.size(), 2u);

  // What a caller resolving a length in `lh` asks for is the one number the
  // block's own pitch is measured from, not a second derivation of it.
  const float height = lineHeightOf(basicStyle(16.0f), fonts);
  EXPECT_FLOAT_EQ(height, paragraph.strutAt(fonts, 0).height);
  EXPECT_NEAR(height, lines[1] - lines[0], 0.01f);
}

TEST(ParagraphStyle, MultipleLeadingOpensThePitch) {
  FontContext& fonts = sigil::test::fonts();
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
  FontContext& fonts = sigil::test::fonts();
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

// ── Between two blocks ────────────────────────────────────────────────────

// WHAT STANDS BETWEEN TWO BLOCKS IS THE BLOCKS' OWN, whichever breaker sets
// them: the optimizing breaker reads past a block's last line before the
// next block opens, and the band it read is set again under the next
// block's pitch, air and grid.
class BetweenBlocks : public BrokenBothWays {};

TEST_P(BetweenBlocks, GridLeadingLandsTwoBlocksOnOneRhythm) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(160, 900));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  ParagraphStyle grid;
  grid.leading = Leading::grid(24.0f);
  ParagraphStyle gridWithAir = grid;
  gridWithAir.spaceBefore = 7.0f;
  options.blocks = {grid, gridWithAir};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  ASSERT_GE(secondBlockLine(paragraph, layout), 2)
      << "the first block did not wrap";
  const std::vector<float> lines = baselines(layout);
  ASSERT_GE(lines.size(), 4u);
  for (size_t index = 1; index < lines.size(); ++index) {
    const float step = lines[index] - lines[index - 1];
    EXPECT_NEAR(std::fmod(step + 0.01f, 24.0f), 0.01f, 0.05f)
        << "step " << step << " is not a whole number of grid lines";
  }
}

TEST_P(BetweenBlocks, TheGapIsTheLargerOfAfterAndBefore) {
  FontContext& fonts = sigil::test::fonts();
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  ParagraphStyle first;
  first.spaceAfter = 30.0f;
  ParagraphStyle second;
  second.spaceBefore = 12.0f;
  options.blocks = {first, second};

  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(160, 900));
  const ParagraphLayout spaced =
      layoutParagraph(fonts, paragraph, flow, options);

  ParagraphLayoutOptions plainOptions;
  plainOptions.lineBreakStrategy = breaker();
  Paragraph plain = twoBlocks();
  BlockFlow plainFlow(SkRect::MakeWH(160, 900));
  const ParagraphLayout bare =
      layoutParagraph(fonts, plain, plainFlow, plainOptions);

  const std::vector<float> spacedLines = baselinesByLine(spaced);
  const std::vector<float> bareLines = baselinesByLine(bare);
  const int opening = secondBlockLine(paragraph, spaced);
  ASSERT_EQ(spacedLines.size(), bareLines.size());
  ASSERT_GE(opening, 2) << "the first block did not wrap";
  ASSERT_GE(spacedLines.size(), static_cast<size_t>(opening) + 2)
      << "the second block did not wrap";
  // Nothing before the block boundary has moved, and everything after it
  // has moved down by the LARGER of the two, which is the block before's
  // spaceAfter rather than the sum or the block after's spaceBefore.
  for (size_t line = 0; line < spacedLines.size(); ++line)
    EXPECT_NEAR(spacedLines[line] - bareLines[line],
                line < static_cast<size_t>(opening) ? 0.0f : 30.0f, 0.01f)
        << "line " << line;
}

TEST_P(BetweenBlocks, EachBlockStacksAtItsOwnPitch) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(160, 900));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  ParagraphStyle tight;
  tight.leading = Leading::absolute(20.0f);
  ParagraphStyle open;
  open.leading = Leading::absolute(40.0f);
  options.blocks = {tight, open};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  const std::vector<float> lines = baselinesByLine(layout);
  const int opening = secondBlockLine(paragraph, layout);
  ASSERT_GE(opening, 2) << "the first block did not wrap";
  ASSERT_GE(lines.size(), static_cast<size_t>(opening) + 2)
      << "the second block did not wrap";
  // Each step within a block is that block's pitch; the step onto the
  // second block's first line is the second block's own band.
  for (size_t line = 1; line < lines.size(); ++line)
    EXPECT_NEAR(lines[line] - lines[line - 1],
                line < static_cast<size_t>(opening) ? 20.0f : 40.0f, 0.01f)
        << "line " << line;
}

INSTANTIATE_TEST_SUITE_P(Breakers, BetweenBlocks, bothBreakers(),
                         breakerName);

// ── Spacing ───────────────────────────────────────────────────────────────

TEST(ParagraphStyle, SpaceBeforeIsNotSuppressedAtTheHeadOfTheFlow) {
  FontContext& fonts = sigil::test::fonts();
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
  FontContext& fonts = sigil::test::fonts();
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

// EVERY BLOCK OPENS ON ITS OWN FIRST LINE, whichever breaker sets it: the
// optimizing breaker reads past a block's last line before the next block
// opens, and the band it read must still take the next block's indent.
class EveryBlockIndent : public BrokenBothWays {};

TEST_P(EveryBlockIndent, EachBlockIndentsItsFirstLineAndNoOther) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(140, 600));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  options.blockDefault.indent.firstLine = 20.0f;
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  const int opening = secondBlockLine(paragraph, layout);
  const std::vector<float> starts = lineStarts(layout);
  ASSERT_GE(opening, 2) << "the first block did not wrap";
  ASSERT_GE(starts.size(), static_cast<size_t>(opening) + 2)
      << "the second block did not wrap";
  for (size_t line = 0; line < starts.size(); ++line)
    EXPECT_NEAR(starts[line],
                line == 0 || line == static_cast<size_t>(opening) ? 20.0f
                                                                   : 0.0f,
                0.01f)
        << "line " << line;
}

INSTANTIATE_TEST_SUITE_P(Breakers, EveryBlockIndent, bothBreakers(),
                         breakerName);

TEST(ParagraphStyle, StartIndentMovesEveryLine) {
  FontContext& fonts = sigil::test::fonts();
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
  FontContext& fonts = sigil::test::fonts();
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
  FontContext& fonts = sigil::test::fonts();
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
  FontContext& fonts = sigil::test::fonts();
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
  FontContext& fonts = sigil::test::fonts();
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
  FontContext& fonts = sigil::test::fonts();
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
