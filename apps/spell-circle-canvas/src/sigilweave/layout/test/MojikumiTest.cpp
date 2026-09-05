/** @file
 * The room a mojikumi table and a tsume setting ask for between two
 * full-width characters, and the widths the breaker fits against.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

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
  FontContext& fonts = sigil::test::fonts();
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
  EXPECT_NEAR(closed, plain - 10.0f, 0.5f)
      << "half an em of a 20px face is what the table asked for";
  // A table with no opinion about this pair changes nothing.
  EXPECT_NEAR(extentWith(bracketTable(0.0f)), plain, 0.01f);
}

TEST(Mojikumi, TsumeClosesTheGapBetweenTwoPlainFullWidthCharacters) {
  FontContext& fonts = sigil::test::fonts();
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
  FontContext& fonts = sigil::test::fonts();
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
