/** @file
 * The rag a balanced block is set to, and the room a mojikumi table asks
 * for between two full-width characters.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Balanced ragged lines ─────────────────────────────────────────────────

TEST(Balance, ABalancedBlockIsSetInTheNarrowestMeasureThatKeepsItsLineCount) {
  FontContext& fonts = sharedContext();
  const std::u8string text =
      u8"A heading of several words that wants an even rag beneath it";
  const auto fillWith = [&](bool balance) {
    Paragraph paragraph = makeParagraph(text, 16.0f);
    BlockFlow flow(SkRect::MakeWH(200, 400));
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
  const auto spread = [](const std::vector<float>& widths) {
    const auto [low, high] = std::minmax_element(widths.begin(), widths.end());
    return *high - *low;
  };
  EXPECT_LT(spread(balanced), spread(loose));
  EXPECT_LE(*std::max_element(balanced.begin(), balanced.end()),
            *std::max_element(loose.begin(), loose.end()) + 0.5f);
}

TEST(Balance, ABlockCutIntoUnequalLinesGivesUpAProportionOfEachOfThem) {
  FontContext& fonts = sharedContext();
  // The narrowing is a FRACTION of each interval's own length, so a line an
  // exclusion already cut short is not asked for the same number of pixels
  // as a full-width one. What has to hold either way: the block keeps its
  // line count, places all its text, and every line still fits the room it
  // was given.
  const std::u8string text =
      u8"A caption of several words set beside a figure it has to flow "
      u8"around, wanting an even rag under the cut";
  const auto fillWith = [&](bool balance) {
    Paragraph paragraph = makeParagraph(text, 15.0f);
    ExclusionFlow flow(SkRect::MakeWH(300, 400));
    flow.shapes().push_back(
        {ExclusionFlow::Shape::kRect, SkRect::MakeXYWH(200, 0, 100, 60), 0});
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    ParagraphStyle style;
    style.balanceRaggedLines = balance;
    options.blocks = {style};
    const ParagraphLayout layout =
        layoutParagraph(fonts, paragraph, flow, options);
    EXPECT_FALSE(layout.overflowed());
    return std::make_pair(layout.lineCount, lineWidths(layout, paragraph));
  };
  const auto [looseLines, loose] = fillWith(false);
  const auto [balancedLines, balanced] = fillWith(true);
  ASSERT_GE(loose.size(), 3u);
  EXPECT_EQ(looseLines, balancedLines);
  ASSERT_EQ(loose.size(), balanced.size());
  for (const float width : balanced) EXPECT_LE(width, 300.0f);
  EXPECT_LE(*std::max_element(balanced.begin(), balanced.end()),
            *std::max_element(loose.begin(), loose.end()) + 0.5f);
}

TEST(Balance, ABalancedBlockIsStillSetInItsWholeMeasure) {
  FontContext& fonts = sharedContext();
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

// ── The room between two full-width characters ────────────────────────────

namespace {

/// A table that closes the gap between a closing and an opening bracket,
/// which is the pair every real mojikumi table has an opinion about.
MojikumiTable bracketTable(float room) {
  MojikumiTable table;
  table.members[static_cast<size_t>(MojikumiClass::kOpening)] = u"（「";
  table.members[static_cast<size_t>(MojikumiClass::kClosing)] = u"）」";
  table.room[static_cast<size_t>(MojikumiClass::kClosing)]
            [static_cast<size_t>(MojikumiClass::kOpening)] = room;
  return table;
}

/// The advance from the first placed run to the end of the last.
float placedExtent(const Paragraph& paragraph, const ParagraphLayout& layout) {
  if (layout.runs.empty()) return 0;
  float left = layout.runs.front().origin.x();
  float right = left;
  for (const PositionedRun& run : layout.runs) {
    left = std::min(left, run.origin.x());
    right = std::max(right, runEnd(paragraph, run));
  }
  return right - left;
}

}  // namespace

TEST(Mojikumi, ATableClosesTheGapItNamesAndLeavesEveryOtherGapAlone) {
  FontContext& fonts = sharedContext();
  const std::u8string text = u8"\xef\xbc\x89\xef\xbc\x88";  // ） （
  const auto extentWith = [&](const MojikumiTable& table) {
    Paragraph paragraph = makeParagraph(text, 20.0f);
    BlockFlow flow(SkRect::MakeWH(400, 200));
    ParagraphLayoutOptions options;
    options.mojikumi = table;
    const ParagraphLayout layout =
        layoutParagraph(fonts, paragraph, flow, options);
    return placedExtent(paragraph, layout);
  };
  const float plain = extentWith(MojikumiTable{});
  const float closed = extentWith(bracketTable(-0.5f));
  EXPECT_NEAR(closed, plain - 10.0f, 0.5f) << "half an em of a 20px face";
  // A table with no opinion about this pair changes nothing.
  EXPECT_NEAR(extentWith(bracketTable(0.0f)), plain, 0.01f);
}

TEST(Mojikumi, TsumeClosesTheGapBetweenTwoPlainFullWidthCharacters) {
  FontContext& fonts = sharedContext();
  const std::u8string text =
      u8"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";  // 日本語
  const auto extentWith = [&](float tsume) {
    Paragraph paragraph = makeParagraph(text, 20.0f);
    BlockFlow flow(SkRect::MakeWH(400, 200));
    ParagraphLayoutOptions options;
    options.tsume = tsume;
    const ParagraphLayout layout =
        layoutParagraph(fonts, paragraph, flow, options);
    return placedExtent(paragraph, layout);
  };
  EXPECT_LT(extentWith(0.05f), extentWith(0.0f));
}

TEST(Mojikumi, TheBreakerFitsAgainstTheRoomTheTableAsked) {
  FontContext& fonts = sharedContext();
  // A measure that holds the characters only once the table has closed the
  // gaps: the breaker must have fitted against the same widths placement
  // spends, or the line ends somewhere else.
  const std::u8string text =
      u8"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x96\x87";
  const auto lineCountWith = [&](float tsume) {
    Paragraph paragraph = makeParagraph(text, 20.0f);
    BlockFlow flow(SkRect::MakeWH(70, 400));
    ParagraphLayoutOptions options;
    options.tsume = tsume;
    return layoutParagraph(fonts, paragraph, flow, options).lineCount;
  };
  EXPECT_GT(lineCountWith(0.0f), 1);
  EXPECT_LE(lineCountWith(0.2f), lineCountWith(0.0f));
}
