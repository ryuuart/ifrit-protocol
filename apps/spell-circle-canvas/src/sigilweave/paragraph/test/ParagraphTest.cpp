/** @file
 * The Paragraph document model: an edit preserves the styles around it,
 * and sentence starts follow the text and survive paint edits.
 */

#include <gtest/gtest.h>

#include <string>

#include "support/ParagraphSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Paragraph, ReplaceTextPreservesSurroundingStyles) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph;
  TextStyle red = basicStyle();
  red.paint.foreground.setColor(SK_ColorRED);
  TextStyle blue = basicStyle();
  blue.paint.foreground.setColor(SK_ColorBLUE);
  paragraph.appendText(u8"red ", red);
  paragraph.appendText(u8"blue", blue);

  paragraph.replaceText(4, 8, u8"teal");  // swap the blue word's text
  paragraph.ensureShaped(fontContext);
  ASSERT_GE(paragraph.spans().size(), 2u);
  EXPECT_EQ(paragraph.spans().front().style.paint.foreground.getColor(),
            SK_ColorRED);
  // Inserted text inherits the style at the edit point (the blue span).
  EXPECT_EQ(paragraph.spans().back().style.paint.foreground.getColor(),
            SK_ColorBLUE);
  EXPECT_EQ(paragraph.text(), u"red teal");
}

TEST(SentenceSegmentation, StartsFollowTheTextAndSurvivePaintEdits) {
  Paragraph paragraph = mixedStyleParagraph();
  const std::span<const uint32_t> starts = paragraph.sentenceStarts();
  ASSERT_EQ(starts.size(), 3u);
  EXPECT_EQ(starts[0], 0u);
  EXPECT_EQ(starts[1], offsetOf(paragraph, u"Some"));
  EXPECT_EQ(starts[2], offsetOf(paragraph, u"The rest"));

  // Paint is not text: recoloring must not move a sentence boundary.
  paragraph.setPaint(0, 5, PaintStyle{SK_ColorGREEN});
  const std::span<const uint32_t> afterPaint = paragraph.sentenceStarts();
  ASSERT_EQ(afterPaint.size(), 3u);
  EXPECT_EQ(afterPaint[1], starts[1]);

  // An edit does move them.
  paragraph.replaceText(0, 0, u8"Wait. ");
  const std::span<const uint32_t> afterEdit = paragraph.sentenceStarts();
  ASSERT_EQ(afterEdit.size(), 4u);
  EXPECT_EQ(afterEdit[1], 6u);
}

TEST(SentenceSegmentation, EmptyTextHasNoSentences) {
  Paragraph paragraph;
  EXPECT_TRUE(paragraph.sentenceStarts().empty());
}
