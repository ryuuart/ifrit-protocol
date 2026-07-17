/** @file
 * Vertical writing mode: upright CJK columns, 'vert' forms,
 * auto-rotated Latin, and tate-chu-yoko.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <set>
using namespace textflow;
using namespace textflow::test;

// ── Vertical writing mode ────────────────────────────────────────────────

TEST(Vertical, UprightCjkStacksDownColumns) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"縦書きのテキストは上から下へ流れる",
                       basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);

  VerticalBlockFlow flow(SkRect::MakeWH(200, 220));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30; // column pitch
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  ASSERT_FALSE(layout.runs.empty());
  EXPECT_GT(layout.lineCount, 1) << "16 chars at 20px must overflow one column";
  float prevYInLine0 = -1e9f;
  float line0x = 0, line1x = 0;
  for (const PositionedRun &run : layout.runs) {
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
  FontContext &fontContext = sharedContext();
  auto glyphsOf = [&](WritingMode mode) {
    Paragraph paragraph;
    paragraph.appendText(u8"「縦組み」", basicStyle(20.0f));
    paragraph.setWritingMode(mode);
    paragraph.ensureShaped(fontContext);
    std::multiset<uint16_t> ids;
    for (const Word &word : paragraph.words())
      for (const WordSegment &segment : word.segments)
        for (uint16_t glyph : segment.shaped->glyphs)
          ids.insert(glyph);
    return ids;
  };
  // Vertical shaping must swap in 'vert' forms (rotated brackets at least).
  EXPECT_NE(glyphsOf(WritingMode::kHorizontal),
            glyphsOf(WritingMode::kVerticalRL));
}

TEST(Vertical, AutoRotatesLatinMixedIntoCjk) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"縦書きにHTTPが混ざる", basicStyle(20.0f));
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  VerticalBlockFlow flow(SkRect::MakeWH(200, 400));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 30;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  bool sawUpright = false, sawRotated = false;
  for (const PositionedRun &run : layout.runs) {
    if (run.shaped->vertical)
      sawUpright = true;
    else if (run.transformed)
      sawRotated = true; // Latin baked as a rotated RSXform blob
  }
  EXPECT_TRUE(sawUpright);
  EXPECT_TRUE(sawRotated);
}

TEST(Vertical, TateChuYokoSetsRunUprightAcrossColumn) {
  FontContext &fontContext = sharedContext();
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

  const float axis = 200 - 30 * 0.5f; // first column's central axis
  const PositionedRun *tcyRun = nullptr;
  for (const PositionedRun &run : layout.runs)
    if (!run.shaped->vertical && !run.transformed)
      tcyRun = &run;
  ASSERT_NE(tcyRun, nullptr) << "the digit run must be placed upright";
  // Centred across the column: origin shifted left by half its advance.
  EXPECT_NEAR(tcyRun->origin.x(), axis - tcyRun->shaped->advance * 0.5f, 0.5f);
  // And it must not consume more column length than its font height (~23px
  // at 20px), far less than the two digits' horizontal advance would be if
  // they were stacked.
  const Word &word = paragraph.words()[std::min<size_t>(
      tcyRun->wordIndex, paragraph.words().size() - 1)];
  EXPECT_LT(word.width, 30.0f);
}
