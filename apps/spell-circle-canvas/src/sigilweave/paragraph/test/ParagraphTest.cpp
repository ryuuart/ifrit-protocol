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
  FontContext& fontContext = sigil::test::fonts();
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

namespace {

/** The level the primitive gives the unit at @p offset of @p text. */
uint8_t levelAt(std::u16string_view text, uint32_t offset,
                unicode::BaseDirection base) {
  for (const unicode::BidiRun& run : unicode::bidi(text, base))
    if (run.start <= offset && offset < run.end) return run.level;
  return 0xFF;
}

/** Every word's level beside the primitive's level at its first unit. */
void expectLevelsOfPrimitive(const Paragraph& paragraph,
                             unicode::BaseDirection base) {
  for (const Word& word : paragraph.words()) {
    if (word.textEnd <= word.textBegin) continue;
    EXPECT_EQ(word.bidiLevel, levelAt(paragraph.text(), word.textBegin, base))
        << "the word at " << word.textBegin;
  }
}

}  // namespace

TEST(Paragraph, ABaseDirectionResolvesLevelsAsThePrimitiveDoes) {
  // A folio label in Persian digits, then English, on a Persian page: the
  // page's own direction decides the numbers' and the dot's levels, which
  // the text alone cannot.
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(u8"۱۵۲۵ · Folio 22v", basicStyle());
  paragraph.ensureAnalyzed(fontContext);
  EXPECT_EQ(paragraph.baseDirection(),
            unicode::BaseDirection::kAutoLeftToRight);
  expectLevelsOfPrimitive(paragraph, unicode::BaseDirection::kAutoLeftToRight);
  std::vector<uint8_t> automatic;
  for (const Word& word : paragraph.words())
    automatic.push_back(word.bidiLevel);

  // The base standing is set again: the analysis stands.
  const uint64_t analysed = paragraph.wordRevision();
  paragraph.setBaseDirection(unicode::BaseDirection::kAutoLeftToRight);
  EXPECT_EQ(paragraph.wordRevision(), analysed);

  // A different base runs it again, resolved as the primitive resolves it.
  paragraph.setBaseDirection(unicode::BaseDirection::kRightToLeft);
  EXPECT_NE(paragraph.wordRevision(), analysed);
  paragraph.ensureAnalyzed(fontContext);
  expectLevelsOfPrimitive(paragraph, unicode::BaseDirection::kRightToLeft);
  std::vector<uint8_t> rightToLeft;
  for (const Word& word : paragraph.words())
    rightToLeft.push_back(word.bidiLevel);
  EXPECT_NE(rightToLeft, automatic)
      << "a right-to-left page left every level where the text alone put it";
}
