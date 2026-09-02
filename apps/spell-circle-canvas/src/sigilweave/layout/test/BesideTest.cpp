/** @file
 * A reading set beside a base — the band it needs, where it stands against
 * the base it reads, and how a base broken across two lines shares it — and
 * the note set in two lines inside one line's room.
 */

#include <gtest/gtest.h>
#include <sigilweave/layout/Beside.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

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
      layoutBeside(fonts, reading,
                   {.base = base,
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
      layoutBeside(fonts, reading,
                   {.base = base,
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

// ── A note set in two lines inside one ────────────────────────────────────

TEST(Warichu, TheCutLeavesTwoLinesAsCloseInLengthAsTheBreaksAllow) {
  FontContext& fonts = sharedContext();
  Paragraph note = makeParagraph(u8"one two three four", 8.0f);
  const WarichuSplit split = warichuSplit(fonts, note);
  ASSERT_GT(split.cutWord, 0u);
  ASSERT_LT(split.cutWord, note.words().size());
  // Neither half is longer than the split says, and moving the cut either
  // way makes the two halves less alike.
  const std::vector<Word>& words = note.words();
  const auto advanceOf = [&](uint32_t from, uint32_t to) {
    float total = 0;
    for (uint32_t index = from; index < to; ++index)
      total += words[index].width +
               (index + 1 < to ? words[index].spaceWidth : 0.0f);
    return total;
  };
  const float first = advanceOf(0, split.cutWord);
  const float second =
      advanceOf(split.cutWord, static_cast<uint32_t>(words.size()));
  EXPECT_NEAR(split.advance, std::max(first, second), 0.01f);
  const float chosen = std::abs(first - second);
  for (uint32_t cut = 1; cut < words.size(); ++cut) {
    if (cut == split.cutWord) continue;
    const float other =
        std::abs(advanceOf(0, cut) -
                 advanceOf(cut, static_cast<uint32_t>(words.size())));
    EXPECT_LE(chosen, other + 0.01f);
  }
}

TEST(Warichu, TheTwoLinesStackInsideTheSlotTheBaseReserved) {
  FontContext& fonts = sharedContext();
  Paragraph note = makeParagraph(u8"one two three four", 8.0f);
  const WarichuSplit split = warichuSplit(fonts, note);
  const SkRect slot = SkRect::MakeXYWH(100, 200, split.advance, split.band);
  const ParagraphLayout layout =
      layoutWarichu(fonts, note, slot, WritingMode::kHorizontal);
  ASSERT_FALSE(layout.runs.empty());
  std::vector<float> baselines;
  for (const PositionedRun& run : layout.runs)
    if (std::find(baselines.begin(), baselines.end(), run.origin.y()) ==
        baselines.end())
      baselines.push_back(run.origin.y());
  EXPECT_EQ(baselines.size(), 2u);
  for (const PositionedRun& run : layout.runs) {
    EXPECT_GE(run.origin.x(), slot.left() - 0.01f);
    EXPECT_LE(run.origin.y(), slot.bottom() + 0.01f);
  }
}

TEST(Warichu, AColumnSetsItsTwoLinesSideBySideAcrossTheSlot) {
  FontContext& fonts = sharedContext();
  Paragraph note =
      makeParagraph(u8"\xe3\x81\xbb\xe3\x82\x93\xe3\x81\xa8", 8.0f);
  const SkRect slot = SkRect::MakeXYWH(100, 200, 24, 60);
  const ParagraphLayout layout =
      layoutWarichu(fonts, note, slot, WritingMode::kVerticalRL);
  ASSERT_FALSE(layout.runs.empty());
  for (const PositionedRun& run : layout.runs)
    EXPECT_LE(run.origin.x(), slot.right() + 0.01f);
}
