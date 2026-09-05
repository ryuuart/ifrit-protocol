/** @file
 * The rag a balanced block is set to: the narrowest measure that keeps its
 * line count, the proportion an already-cut line gives up, and the measure
 * it is still set in.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"
#include "support/Readings.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Balance, ABalancedBlockIsSetInTheNarrowestMeasureThatKeepsItsLineCount) {
  FontContext& fonts = sigil::test::fonts();
  const std::u8string text =
      u8"A heading of several words that wants an even rag beneath it";
  // At 16 px in the instrument face (a letter 9.6 px, a space 4.8) the
  // loose setting in 260 px is "A heading of several words" (230.4),
  // "that wants an even rag" (192) and "beneath it" (91.2): three lines
  // with a rag the same count can be set far more evenly under.
  const auto fillWith = [&](bool balance) {
    Paragraph paragraph = makeParagraph(text, 16.0f);
    BlockFlow flow(SkRect::MakeWH(260, 400));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    ParagraphStyle style;
    style.balanceRaggedLines = balance;
    options.blocks = {style};
    const ParagraphLayout layout =
        layoutParagraph(fonts, paragraph, flow, options);
    return lineWidths(layout, paragraph);
  };
  const std::vector<float> loose = fillWith(false);
  const std::vector<float> balanced = fillWith(true);
  ASSERT_GE(loose.size(), 3u);
  // The count is what the narrowing is searched under, so it is unchanged.
  ASSERT_EQ(loose.size(), balanced.size());
  EXPECT_LT(spread(balanced), spread(loose));
  EXPECT_LE(*std::max_element(balanced.begin(), balanced.end()),
            *std::max_element(loose.begin(), loose.end()) + 0.5f);
}

TEST(Balance, ABlockCutIntoUnequalLinesGivesUpAProportionOfEachOfThem) {
  FontContext& fonts = sigil::test::fonts();
  // The narrowing is a FRACTION of each interval's own length, so a line an
  // exclusion already cut short is not asked for the same number of pixels
  // as a full-width one. What has to hold either way: the block keeps its
  // line count, places all its text, and every line still fits the room it
  // was given.
  const std::u8string text =
      u8"A caption of several words set beside a figure it has to flow "
      u8"around, wanting an even rag under the cut";
  // The cut: the right 100 px of the first 60 px of the block. At 15 px in
  // the instrument face a line is 15 px tall, so the four lines whose top
  // is above 60 have 200 px and every line below has the whole 300.
  constexpr float kCutBottom = 60.0f;
  constexpr float kCutWidth = 200.0f;
  constexpr float kWholeWidth = 300.0f;
  const auto fillWith = [&](bool balance) {
    Paragraph paragraph = makeParagraph(text, 15.0f);
    ExclusionFlow flow(SkRect::MakeWH(kWholeWidth, 400));
    flow.shapes().push_back({ExclusionFlow::Shape::kRect,
                             SkRect::MakeXYWH(kCutWidth, 0,
                                              kWholeWidth - kCutWidth,
                                              kCutBottom),
                             0});
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    ParagraphStyle style;
    style.balanceRaggedLines = balance;
    options.blocks = {style};
    const ParagraphLayout layout =
        layoutParagraph(fonts, paragraph, flow, options);
    EXPECT_FALSE(layout.overflowed());
    // Every line against the room its own top had: the cut width while the
    // line starts inside the cut, the whole measure below it.
    for (const LineMetrics& line : layout.lineMetrics(paragraph))
      EXPECT_LE(line.right - line.left,
                line.baseline - line.ascent < kCutBottom ? kCutWidth
                                                         : kWholeWidth)
          << "line " << line.lineIndex;
    return std::make_pair(layout.lineCount, lineWidths(layout, paragraph));
  };
  const auto [looseLines, loose] = fillWith(false);
  const auto [balancedLines, balanced] = fillWith(true);
  ASSERT_GE(loose.size(), 3u);
  EXPECT_EQ(looseLines, balancedLines);
  ASSERT_EQ(loose.size(), balanced.size());
}

TEST(Balance, ABalancedBlockIsStillSetInItsWholeMeasure) {
  FontContext& fonts = sigil::test::fonts();
  // The narrowing is a break decision only: a centred block stays centred
  // on the measure it was given, not on the one it was broken against.
  Paragraph paragraph = makeParagraph(
      u8"A heading of several words that wants an even rag beneath it", 16.0f);
  BlockFlow flow(SkRect::MakeWH(260, 400));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  ParagraphStyle style;
  style.balanceRaggedLines = true;
  style.alignment = TextAlignment::kCenter;
  options.blocks = {style};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  const std::vector<float> widths = lineWidths(layout, paragraph);
  ASSERT_FALSE(widths.empty());
  std::vector<float> centres;
  for (const PositionedRun& run : layout.runs)
    if (run.lineIndex == 0) centres.push_back(run.origin.x());
  ASSERT_FALSE(centres.empty());
  const float left = *std::min_element(centres.begin(), centres.end());
  EXPECT_NEAR(left, (260.0f - widths.front()) * 0.5f, 1.0f);
}
