/** @file
 * Inline placeholders through the breakers: the width they reserve, where
 * they sit against the baseline, how they wrap and justify like words, and
 * a resize relaying out live.
 */

#include <gtest/gtest.h>

#include <boost/unordered/unordered_flat_set.hpp>
#include <string>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Placeholders, ReservesWidthInTheLine) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(u8"before ", basicStyle());
  paragraph.appendPlaceholder({90, 20, 0}, basicStyle());
  paragraph.appendText(u8" after", basicStyle());

  BlockFlow flow(SkRect::MakeWH(600, 60));  // everything on one line
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_FLOAT_EQ(rects[0].rect.width(), 90);
  EXPECT_FLOAT_EQ(rects[0].rect.height(), 20);

  // "after" starts past the slot's right edge.
  float afterX = -1;
  for (const PositionedRun& run : layout.runs) {
    if (run.placeholderIndex >= 0) continue;
    const Word& word = paragraph.words()[run.wordIndex];
    const std::u16string_view text(paragraph.text());
    if (text.substr(word.textBegin, 5) == u"after") afterX = run.origin.x();
  }
  ASSERT_GE(afterX, 0);
  EXPECT_GE(afterX, rects[0].rect.right() - 0.25f);
}

TEST(Placeholders, SitOnTheBaselineWithDrop) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(u8"x ", basicStyle());
  paragraph.appendPlaceholder({40, 30, 8},
                              basicStyle());  // bottom 8px below base
  BlockFlow flow(SkRect::MakeWH(300, 60));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  float baselineY = -1;
  for (const PositionedRun& run : layout.runs)
    if (run.placeholderIndex < 0) baselineY = run.origin.y();
  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_FLOAT_EQ(rects[0].rect.bottom(), baselineY + 8);
  EXPECT_FLOAT_EQ(rects[0].rect.top(), baselineY + 8 - 30);
}

TEST(Placeholders, WrapAndJustifyLikeWords) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  for (int placeholderIndex = 0; placeholderIndex < 6; ++placeholderIndex) {
    paragraph.appendText(u8"word word word ", basicStyle());
    paragraph.appendPlaceholder({60, 14, 0}, basicStyle());
    paragraph.appendText(u8" ", basicStyle());
  }
  BlockFlow flow(SkRect::MakeWH(220, 400));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 6u);
  boost::unordered_flat_set<int> lines;
  for (const auto& placed : rects) {
    lines.insert(placed.lineIndex);
    // Slots never overflow the measure.
    EXPECT_GE(placed.rect.left(), -0.25f);
    EXPECT_LE(placed.rect.right(), 220.5f);
  }
  EXPECT_GT(lines.size(), 1u) << "slots must wrap onto later lines";
}

TEST(Placeholders, ResizeRelayoutsLive) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(u8"pill: ", basicStyle());
  paragraph.appendPlaceholder({50, 16, 0}, basicStyle());
  BlockFlow flow(SkRect::MakeWH(400, 60));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);

  paragraph.setPlaceholder(0, {120, 16, 0});
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_FLOAT_EQ(after.placeholderRects(paragraph)[0].rect.width(), 120);
}

TEST(Placeholders, VerticalSlotsReserveLogicalAdvanceAndReportPhysicalRects) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(u8"A ", basicStyle());
  paragraph.appendPlaceholder({72, 26, 4}, basicStyle());
  paragraph.appendText(u8" B", basicStyle());
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  VerticalBlockFlow flow(SkRect::MakeWH(160, 280));
  const ParagraphLayout layout = layoutParagraph(fonts, paragraph, flow);
  const auto slots = layout.placeholderRects(paragraph);
  ASSERT_EQ(slots.size(), 1u);
  const SkRect rect = slots.front().rect;
  EXPECT_FLOAT_EQ(rect.width(), 26);
  EXPECT_FLOAT_EQ(rect.height(), 72);
  const auto columns = layout.columnMetrics(paragraph);
  ASSERT_EQ(columns.size(), 1u);
  EXPECT_FLOAT_EQ(rect.centerX(), columns.front().axis);
  EXPECT_TRUE(layout.lineMetrics(paragraph).empty());
  bool before = false, after = false;
  for (const PositionedRun& run : layout.runs) {
    if (run.placeholderIndex >= 0) {
      EXPECT_FLOAT_EQ(rect.top(), run.origin.y());
      continue;
    }
    const Word& word = paragraph.words()[run.wordIndex];
    if (paragraph.text()[word.textBegin] == u'A') {
      before = true;
      EXPECT_LE(run.penOffset + run.advance, rect.top() + 0.01f);
    } else if (paragraph.text()[word.textBegin] == u'B') {
      after = true;
      EXPECT_GE(run.penOffset, rect.bottom() - 0.01f);
    }
  }
  EXPECT_TRUE(before && after);
}

TEST(Placeholders, VerticalCrossSizeOpensColumnsWithoutHorizontalDrop) {
  FontContext& fonts = sigil::test::fonts();
  for (float drop : {0.0f, 200.0f}) {
    Paragraph paragraph;
    paragraph.appendPlaceholder({72, 54, drop}, basicStyle());
    paragraph.setWritingMode(WritingMode::kVerticalRL);
    VerticalBlockFlow flow(SkRect::MakeWH(160, 280));
    const ParagraphLayout layout = layoutParagraph(fonts, paragraph, flow);
    const auto slots = layout.placeholderRects(paragraph);
    ASSERT_EQ(slots.size(), 1u);
    const auto columns = layout.columnMetrics(paragraph);
    ASSERT_EQ(columns.size(), 1u);
    EXPECT_FLOAT_EQ(layout.linePitch, 54);
    EXPECT_TRUE(columns.front().rect().contains(slots.front().rect));
  }
}

TEST(Placeholders, VerticalSlotsWrapByTheirLogicalAdvance) {
  FontContext& fonts = sigil::test::fonts();
  for (LineBreakStrategy breaker :
       {LineBreakStrategy::kGreedy, LineBreakStrategy::kKnuthPlass}) {
    Paragraph paragraph;
    for (int index = 0; index < 3; ++index)
      paragraph.appendPlaceholder({80, 24, 0}, basicStyle());
    paragraph.setWritingMode(WritingMode::kVerticalRL);
    VerticalBlockFlow flow(SkRect::MakeWH(160, 150));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = breaker;
    const ParagraphLayout layout =
        layoutParagraph(fonts, paragraph, flow, options);
    const auto slots = layout.placeholderRects(paragraph);
    ASSERT_EQ(slots.size(), 3u);
    EXPECT_EQ(layout.lineCount, 3);
    for (size_t index = 0; index < slots.size(); ++index) {
      EXPECT_FLOAT_EQ(slots[index].rect.height(), 80);
      EXPECT_GE(slots[index].rect.top(), 0);
      EXPECT_LE(slots[index].rect.bottom(), 150);
      if (index > 0)
        EXPECT_LT(slots[index].rect.centerX(), slots[index - 1].rect.centerX());
    }
  }
}
