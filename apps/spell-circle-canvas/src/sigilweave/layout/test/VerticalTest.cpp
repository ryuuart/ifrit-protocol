/** @file
 * Vertical writing mode through the layout: upright CJK columns,
 * auto-rotated Latin, tate-chu-yoko, the per-column metrics, columns cut
 * by an exclusion, and the overflow marker at a clamped column's foot.
 */

#include <gtest/gtest.h>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Vertical writing mode ────────────────────────────────────────────────

TEST(Vertical, UprightCjkStacksDownColumns) {
  FontContext& fontContext = sigil::test::fonts();
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

TEST(Vertical, AutoRotatesLatinMixedIntoCjk) {
  FontContext& fontContext = sigil::test::fonts();
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
  FontContext& fontContext = sigil::test::fonts();
  // The same passage twice, differing only in the form the digits take, so
  // the column length the pair costs is read against the column length the
  // same pair costs stacked one above the other.
  const auto set = [&](VerticalForm form) {
    TextStyle japaneseStyle = basicStyle(20.0f);
    TextStyle digits = basicStyle(20.0f);
    digits.shaping.verticalForm = form;
    Paragraph paragraph;
    paragraph.appendText(u8"平成", japaneseStyle);
    paragraph.appendText(u8"31", digits);
    paragraph.appendText(u8"年の縦組み", japaneseStyle);
    paragraph.setWritingMode(WritingMode::kVerticalRL);
    VerticalBlockFlow flow(SkRect::MakeWH(200, 400));
    ParagraphLayoutOptions options;
    options.lineMetrics.height = 30;
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    float columnLength = 0;
    for (const PositionedRun& run : layout.runs)
      if (run.shaped)
        columnLength = std::max(columnLength,
                                run.origin.y() + run.shaped->advance);
    return std::make_pair(std::move(layout), columnLength);
  };

  const auto [layout, tcyLength] = set(VerticalForm::kTateChuYoko);
  const float axis = 200 - 30 * 0.5f;  // first column's central axis
  const PositionedRun* tcyRun = nullptr;
  for (const PositionedRun& run : layout.runs)
    if (!run.shaped->vertical && !run.transformed) tcyRun = &run;
  ASSERT_NE(tcyRun, nullptr) << "the digit run must be placed upright";
  // Centred across the column: origin shifted left by half its advance.
  EXPECT_NEAR(tcyRun->origin.x(), axis - tcyRun->shaped->advance * 0.5f, 0.5f);
  // And the pair costs less column length set across it than it does
  // stacked, which is the whole point of the form.
  EXPECT_LT(tcyLength, set(VerticalForm::kUpright).second);
}

TEST(Vertical, ColumnMetricsReportTheBandAndTheExtent) {
  // lineMetrics() has nothing to say about columns; columnMetrics() is the
  // answer for the other writing mode, and it is the one a caller measures
  // a vertical passage by.
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(u8"縦組みの文章は上から下へ流れ右から左へと列が進む",
                       basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 200));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  EXPECT_FLOAT_EQ(layout.linePitch, 30.0f)
      << "the pitch the geometry was queried at";
  EXPECT_TRUE(layout.lineMetrics(paragraph).empty())
      << "a column has no baseline to report";

  const std::vector<ColumnMetrics> columns = layout.columnMetrics(paragraph);
  ASSERT_GE(columns.size(), 2u);
  for (size_t i = 0; i < columns.size(); ++i) {
    EXPECT_EQ(columns[i].lineIndex, (int)i) << "ascending by column index";
    EXPECT_FLOAT_EQ(columns[i].pitch, 30.0f);
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
  FontContext& fontContext = sigil::test::fonts();
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

// ── Columns around an exclusion ──────────────────────────────────────────

TEST(Vertical, ColumnsFlowAroundASilhouette) {
  // The whole breaker runs over a column flow the way it runs over a line
  // flow: a column an exclusion crosses hands out two intervals, and no
  // run may sit anywhere but inside one of them.
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(
      u8"縦組みの文章が円をよけて流れる様子を見るための本文であり、列は右から"
      u8"左へと進みながら、障害物の上と下に分かれて組まれてゆく。文字は列の心"
      u8"に沿って落ちてゆき、円に出会えば頭と足に分かれ、円を過ぎればまた一本"
      u8"の列に戻る。",
      basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  constexpr float kPitch = 30;
  ExclusionFlow flow(SkRect::MakeWH(300, 400), FlowAxis::kColumns);
  flow.shapes().push_back(
      ExclusionFlow::Shape::fromCircle(SkRect::MakeXYWH(90, 140, 120, 120), 6));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = kPitch;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  ASSERT_FALSE(layout.runs.empty());
  for (const PositionedRun& run : layout.runs)
    ASSERT_FALSE(run.transformed) << "upright CJK all the way down";

  const IntervalContainment held =
      runsStayInsideIntervals(flow, layout, kPitch, 0, PenAxis::kDownColumns);
  EXPECT_GT(held.runs, 0);
  EXPECT_EQ(held.exhausted, 0)
      << "the flow refused a column it had already placed a run on";
  EXPECT_GT(held.splitBands, 0) << "the circle must have split some column";
  EXPECT_EQ(held.outside, 0)
      << "a run on column " << held.outsideBand << " spans ["
      << held.outsideStart << ", " << held.outsideEnd
      << "], outside every interval the column offered";

  // And the exclusion costs room: the same text in the same block with
  // nothing in its way needs fewer columns.
  VerticalBlockFlow clear(SkRect::MakeWH(300, 400));
  ParagraphLayout unobstructed =
      layoutParagraph(fontContext, paragraph, clear, options);
  EXPECT_GT(layout.lineCount, unobstructed.lineCount);
}

// ── The marker at a column's foot ────────────────────────────────────────

namespace {

/// How far down its column a run reaches. Every form a column carries is
/// placed from the column's own pen, so one reading covers them all.
float columnFoot(const PositionedRun& run) {
  return run.origin.y() + (run.shaped ? run.shaped->advance : 0.0f);
}

}  // namespace

/// One column of upright Japanese in a block two columns deep, clamped to
/// a single column — the setting an overflow marker has to end.
class ClampedColumn : public ::testing::Test {
 protected:
  void SetUp() override {
    m_paragraph.appendText(
        u8"縦組みの文章は上から下へ流れ右から左へと列が進み続けてゆく",
        basicStyle(20.0f));
    m_paragraph.setWritingMode(WritingMode::kVerticalRL);
  }

  ParagraphLayout column(bool withMarker) {
    VerticalBlockFlow flow(SkRect::MakeWH(200, 220));
    ParagraphLayoutOptions options;
    options.lineMetrics.height = 30;
    options.overflow.maxLines = 1;
    if (withMarker) options.overflow.ellipsis = u"…";
    return layoutParagraph(sigil::test::fonts(), m_paragraph, flow, options);
  }

  Paragraph m_paragraph;
};

TEST_F(ClampedColumn, TheMarkerStandsUprightAtTheFootOfTheColumnItCuts) {
  const ParagraphLayout layout = column(/*withMarker=*/true);

  ASSERT_TRUE(layout.overflowed());
  ASSERT_TRUE(layout.ellipsized);
  ASSERT_GE(layout.runs.size(), 2u);
  const PositionedRun& marker = layout.runs.back();
  ASSERT_TRUE(marker.shaped);
  EXPECT_TRUE(marker.shaped->vertical)
      << "an upright column takes an upright marker — the face's own "
         "vertical form when it has one";
  EXPECT_FALSE(marker.transformed);
  EXPECT_FLOAT_EQ(marker.origin.x(), 200 - 30 * 0.5f)
      << "on the column's central axis, like every glyph above it";
  // At the FOOT: below the last of the text, and inside the column.
  const PositionedRun& tail = layout.runs[layout.runs.size() - 2];
  EXPECT_GE(marker.origin.y(), columnFoot(tail) - 0.25f);
  EXPECT_LE(columnFoot(marker), 220.0f + 0.75f);
  EXPECT_EQ(marker.lineIndex, tail.lineIndex);

  // The column's own metrics reach it: the marker names the interval it
  // landed on and where along it, so the column it ends measures down to
  // the marker's foot rather than stopping at the text.
  const std::vector<ColumnMetrics> columns = layout.columnMetrics(m_paragraph);
  ASSERT_EQ(columns.size(), 1u);
  EXPECT_NEAR(columns.front().bottom, columnFoot(marker), 0.5f);
}

TEST_F(ClampedColumn, TheCutMovesUpTheColumnToMakeRoomForTheMarker) {
  // The marker is measured against the COLUMN's length, so the cut moves
  // up by exactly as much as the marker needs — the same trade a line
  // makes at its end.
  const ParagraphLayout bare = column(/*withMarker=*/false);
  const ParagraphLayout marked = column(/*withMarker=*/true);

  ASSERT_TRUE(bare.overflowed());
  ASSERT_TRUE(marked.ellipsized);
  ASSERT_FALSE(bare.ellipsized);
  // The last run that is TEXT, not the marker.
  const PositionedRun& bareTail = bare.runs.back();
  const PositionedRun& markedTail = marked.runs[marked.runs.size() - 2];
  EXPECT_LT(columnFoot(markedTail), columnFoot(bareTail))
      << "the cut moved up the column to make room";
  EXPECT_LT(marked.firstUnplacedWord, bare.firstUnplacedWord)
      << "and the words it gave up are reported unplaced";
}

TEST(Vertical, ARotatedRunTakesARotatedMarker) {
  // Latin rotates into a column, and the marker that cuts it is turned
  // with it rather than standing upright beside it.
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(
      u8"a Latin passage set down a column rotates a quarter turn and keeps "
      u8"going far past the room this clamp allows it",
      basicStyle(18.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(160, 200));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 26;
  options.overflow.maxLines = 1;
  options.overflow.ellipsis = u"…";
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  ASSERT_TRUE(layout.ellipsized);
  const PositionedRun& marker = layout.runs.back();
  EXPECT_TRUE(marker.transformed)
      << "the marker after a rotated run is baked into the column's turn";
  EXPECT_FALSE(marker.shaped->vertical);
  ASSERT_TRUE(marker.blob);
  // Baked placement: the blob's ink must sit inside the clamped column.
  EXPECT_LE(marker.blob->bounds().bottom(), 200.0f + 2.0f);
  EXPECT_GT(marker.blob->bounds().top(), 0.0f);
}
