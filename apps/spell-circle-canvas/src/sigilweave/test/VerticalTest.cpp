/** @file
 * Vertical writing mode: upright CJK columns, 'vert' forms,
 * auto-rotated Latin, and tate-chu-yoko.
 */

#include <gtest/gtest.h>

#include <set>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Vertical writing mode ────────────────────────────────────────────────

TEST(Vertical, UprightCjkStacksDownColumns) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"縦書きのテキストは上から下へ流れる",
                       basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 220));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30;  // column pitch
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  ASSERT_FALSE(layout.runs.empty());
  EXPECT_GT(layout.lineCount, 1) << "16 chars at 20px must overflow one column";
  float prevYInLine0 = -1e9f;
  float line0x = 0, line1x = 0;
  for (const PositionedRun& run : layout.runs) {
    EXPECT_FALSE(run.transformed) << "upright CJK is a positioned blob";
    EXPECT_TRUE(run.shaped->vertical);
    if (run.lineIndex == 0) {
      line0x = run.origin.x();
      EXPECT_GT(run.origin.y(), prevYInLine0) << "pen must travel downward";
      prevYInLine0 = run.origin.y();
    } else if (run.lineIndex == 1) {
      line1x = run.origin.x();
    }
  }
  EXPECT_LT(line1x, line0x) << "columns must advance right to left";
}

TEST(Vertical, VertFeatureSubstitutesForms) {
  FontContext& fontContext = sharedContext();
  auto glyphsOf = [&](WritingMode mode) {
    Paragraph paragraph;
    paragraph.appendText(u8"「縦組み」", basicStyle(20.0f));
    paragraph.setWritingMode(mode);
    paragraph.ensureShaped(fontContext);
    std::multiset<uint16_t> ids;
    for (const Word& word : paragraph.words())
      for (const WordSegment& segment : word.segments)
        for (uint16_t glyph : segment.shaped->glyphs) ids.insert(glyph);
    return ids;
  };
  // Vertical shaping must swap in 'vert' forms (rotated brackets at least).
  EXPECT_NE(glyphsOf(WritingMode::kHorizontal),
            glyphsOf(WritingMode::kVerticalRL));
}

TEST(Vertical, AutoRotatesLatinMixedIntoCjk) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"縦書きにHTTPが混ざる", basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  VerticalBlockFlow flow(SkRect::MakeWH(200, 400));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  bool sawUpright = false, sawRotated = false;
  for (const PositionedRun& run : layout.runs) {
    if (run.shaped->vertical)
      sawUpright = true;
    else if (run.transformed)
      sawRotated = true;  // Latin baked as a rotated RSXform blob
  }
  EXPECT_TRUE(sawUpright);
  EXPECT_TRUE(sawRotated);
}

TEST(Vertical, TateChuYokoSetsRunUprightAcrossColumn) {
  FontContext& fontContext = sharedContext();
  TextStyle japaneseStyle = basicStyle(20.0f);
  TextStyle tcy = basicStyle(20.0f);
  tcy.shaping.verticalForm = VerticalForm::kTateChuYoko;

  Paragraph paragraph;
  paragraph.appendText(u8"平成", japaneseStyle);
  paragraph.appendText(u8"31", tcy);
  paragraph.appendText(u8"年の縦組み", japaneseStyle);
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 400));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  const float axis = 200 - 30 * 0.5f;  // first column's central axis
  const PositionedRun* tcyRun = nullptr;
  for (const PositionedRun& run : layout.runs)
    if (!run.shaped->vertical && !run.transformed) tcyRun = &run;
  ASSERT_NE(tcyRun, nullptr) << "the digit run must be placed upright";
  // Centred across the column: origin shifted left by half its advance.
  EXPECT_NEAR(tcyRun->origin.x(), axis - tcyRun->shaped->advance * 0.5f, 0.5f);
  // And it must not consume more column length than its font height (~23px
  // at 20px), far less than the two digits' horizontal advance would be if
  // they were stacked.
  const Word& word = paragraph.words()[std::min<size_t>(
      tcyRun->wordIndex, paragraph.words().size() - 1)];
  EXPECT_LT(word.width, 30.0f);
}

TEST(Vertical, ColumnMetricsReportTheBandAndTheExtent) {
  // lineMetrics() has nothing to say about columns; columnMetrics() is the
  // answer for the other writing mode, and it is the one a caller measures
  // a vertical passage by.
  FontContext& fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"縦組みの文章は上から下へ流れ右から左へと列が進む",
                       basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 200));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  EXPECT_EQ(layout.linePitch, 30.0f) << "the pitch the geometry was queried at";
  EXPECT_TRUE(layout.lineMetrics(paragraph).empty())
      << "a column has no baseline to report";

  const std::vector<ColumnMetrics> columns = layout.columnMetrics(paragraph);
  ASSERT_GE(columns.size(), 2u);
  for (size_t i = 0; i < columns.size(); ++i) {
    EXPECT_EQ(columns[i].lineIndex, (int)i) << "ascending by column index";
    EXPECT_EQ(columns[i].pitch, 30.0f);
    EXPECT_GT(columns[i].bottom, columns[i].top) << "the column ran downward";
    EXPECT_LE(columns[i].rect().height(), 200.0f) << "inside the block";
  }
  EXPECT_FLOAT_EQ(columns[0].axis - columns[1].axis, 30.0f)
      << "columns advance right to left, one pitch apart";
  EXPECT_FLOAT_EQ(columns[0].rect().right(), 200.0f)
      << "the first column's band ends at the block's right edge";
  EXPECT_LT(columns[0].textBegin, columns[1].textBegin)
      << "reading order runs down column 0 first";
}

TEST(Vertical, TateChuYokoCountsItsFontHeightDownTheColumn) {
  // A run set horizontally inside the column consumes column pitch by its
  // FONT HEIGHT, not by its horizontal advance — so the extent must not
  // depend on how many digits it holds.
  FontContext& fontContext = sharedContext();
  const auto columnExtent = [&](const char8_t* digits) {
    TextStyle body = basicStyle(20.0f);
    TextStyle tcy = body;
    tcy.shaping.verticalForm = VerticalForm::kTateChuYoko;
    Paragraph paragraph;
    paragraph.appendText(u8"平成", body);
    paragraph.appendText(digits, tcy);
    paragraph.appendText(u8"年", body);
    paragraph.setWritingMode(WritingMode::kVerticalRL);
    VerticalBlockFlow flow(SkRect::MakeWH(120, 400));
    ParagraphLayoutOptions options;
    options.lineMetrics.height = 28;
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const std::vector<ColumnMetrics> columns = layout.columnMetrics(paragraph);
    return columns.empty() ? 0.0f : columns.front().rect().height();
  };
  EXPECT_NEAR(columnExtent(u8"31"), columnExtent(u8"312"), 0.5f)
      << "a wider tate-chu-yoko run must not lengthen the column";
}
