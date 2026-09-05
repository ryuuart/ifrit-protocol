/** @file
 * The line's edges: punctuation hung past the start of a line, a text with
 * no table squared on its advances, and the prohibitions that decide which
 * character a line may open with.
 */

#include <gtest/gtest.h>
#include <sigilweave/kit/LineTables.h>

#include <string>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

// ── The line's edges ──────────────────────────────────────────────────────

TEST(LineEdges, HangingPunctuationPullsALineBackPastItsStart) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph plain = makeParagraph(u8"“quoted opening words here”");
  Paragraph hung = makeParagraph(u8"“quoted opening words here”");
  BlockFlow flowA(SkRect::MakeWH(400, 200));
  BlockFlow flowB(SkRect::MakeWH(400, 200));
  const std::vector<float> square =
      lineStarts(layoutParagraph(fonts, plain, flowA));
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
  FontContext& fonts = sigil::test::fonts();
  Paragraph plain = makeParagraph(u8"“quoted opening words here”");
  BlockFlow flow(SkRect::MakeWH(400, 200));
  const std::vector<float> starts =
      lineStarts(layoutParagraph(fonts, plain, flow));
  ASSERT_FALSE(starts.empty());
  EXPECT_NEAR(starts.front(), 0.0f, 0.01f);
}

TEST(LineEdges, KinsokuNeverOpensALineWithAProhibitedCharacter) {
  FontContext& fonts = sigil::test::fonts();
  // A comma that would otherwise begin a column: the prohibition drops the
  // boundary before it, so the character before comes down with it.
  const std::u8string passage =
      u8"これは本文です、そして"
      u8"続きます。";
  Paragraph bare = machineParagraph(passage, 20.0f);
  Paragraph ruled = machineParagraph(passage, 20.0f);
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
  FontContext& fonts = sigil::test::fonts();
  // Two ideographs with a UAX #14 boundary between them, and a table that
  // forbids the second from opening a line: the boundary goes, and the two
  // become one unbreakable word.
  const std::u8string passage =
      u8"\xe6\x9c\xac\xe6\x96\x87\xe6\x9c\xac\xe6\x96\x87";
  Paragraph bare = machineParagraph(passage, 20.0f);
  Paragraph ruled = machineParagraph(passage, 20.0f);
  ParagraphLayoutOptions options;
  options.kinsoku.notLineStart = u"\u6587";
  BlockFlow flowA(SkRect::MakeWH(126, 300));
  BlockFlow flowB(SkRect::MakeWH(126, 300));
  layoutParagraph(fonts, bare, flowA);
  layoutParagraph(fonts, ruled, flowB, options);
  EXPECT_LT(ruled.words().size(), bare.words().size());
  for (const Word& word : ruled.words())
    if (word.textBegin > 0) EXPECT_NE(ruled.text()[word.textBegin], u'\u6587');
}
