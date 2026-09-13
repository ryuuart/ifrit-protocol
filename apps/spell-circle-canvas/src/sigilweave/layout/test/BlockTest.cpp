/** @file
 * The block partial: what it overlays, what it leaves alone, and the sheet
 * of named partials.
 */

#include <gtest/gtest.h>
#include <sigilweave/layout/Block.h>
#include <sigilweave/layout/ParagraphStyleSheet.h>

using namespace sigil::weave;

TEST(Block, APartialOverlaysAWholeStyleFieldByField) {
  ParagraphStyle base;
  base.leading = Leading::multiple(1.2f);
  base.indent.start = 10;
  base.keep.widowLines = 2;
  Block over;
  over.alignment = TextAlignment::kCenter;
  over.firstLineIndent = 24;
  over.orphanLines = 3;
  const ParagraphStyle total = overlay(base, over);
  EXPECT_EQ(total.leading, Leading::multiple(1.2f)) << "unset: the base's";
  EXPECT_FLOAT_EQ(total.indent.start, 10) << "not the partial's to change";
  EXPECT_EQ(total.keep.widowLines, 2);
  EXPECT_EQ(total.alignment, TextAlignment::kCenter);
  EXPECT_FLOAT_EQ(total.indent.firstLine, 24);
  EXPECT_EQ(total.keep.orphanLines, 3);
  EXPECT_TRUE(Block{}.empty());
  EXPECT_FALSE(over.empty());
}

TEST(Block, MergeCopiesFieldsAndTheEmptyPartialChangesNothing) {
  Block into;
  into.leading = Leading::absolute(20);
  into.alignment = TextAlignment::kEnd;
  Block over;
  over.alignment = TextAlignment::kJustify;
  over.writingMode = WritingMode::kVerticalRL;
  over.lineBreakLocale = "ja";
  merge(into, over);
  EXPECT_EQ(into.leading, Leading::absolute(20));
  EXPECT_EQ(into.alignment, TextAlignment::kJustify) << "the second wins";
  EXPECT_EQ(into.writingMode, WritingMode::kVerticalRL);
  EXPECT_EQ(into.lineBreakLocale, "ja");
  const Block before = into;
  EXPECT_EQ(merge(into, Block{}), before);
  // A partial alone names the layout's own answer for what it leaves unset.
  EXPECT_EQ(toParagraphStyle(Block{}), ParagraphStyle{});
  EXPECT_EQ(toParagraphStyle(over).alignment, TextAlignment::kJustify);
}

TEST(Block, ApplySetsTheLayoutWideFieldsAPartialStates) {
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kEnd;
  options.tsume = 0.1f;
  Block block;
  block.lineBreak = LineBreakStrategy::kKnuthPlass;
  block.lastLineAlignment = TextAlignment::kCenter;
  block.justifyLastLine = true;
  block.hyphenation = HyphenationOptions{.enabled = false};
  apply(options, block);
  EXPECT_EQ(options.alignment, TextAlignment::kEnd) << "unset: as held";
  EXPECT_FLOAT_EQ(options.tsume, 0.1f);
  EXPECT_EQ(options.lineBreakStrategy, LineBreakStrategy::kKnuthPlass);
  EXPECT_EQ(options.justification.lastLineAlignment, TextAlignment::kCenter);
  EXPECT_TRUE(options.justification.justifyLastLine);
  EXPECT_FALSE(options.hyphenation.enabled);
  Block more;
  more.lineBreak = LineBreakStrategy::kGreedy;
  merge(block, more);
  EXPECT_EQ(block.lineBreak, LineBreakStrategy::kGreedy);
  EXPECT_FALSE(block.empty());
}

TEST(ParagraphStyleSheet, NamesResolveToPartialsAndAnAbsentNameIsAbsent) {
  ParagraphStyleSheet sheet;
  Block heading;
  heading.leading = Leading::multiple(1.1f);
  sheet.set("heading", heading);
  Block body;
  body.alignment = TextAlignment::kJustify;
  sheet.set("body", body);
  EXPECT_EQ(sheet.size(), 2u);
  ASSERT_NE(sheet.find("heading"), nullptr);
  EXPECT_EQ(*sheet.find("heading"), heading);
  EXPECT_EQ(sheet.find("footer"), nullptr);
  EXPECT_TRUE(sheet.contains("body"));
  Block wider;
  wider.alignment = TextAlignment::kCenter;
  sheet.set("body", wider);
  EXPECT_EQ(sheet.size(), 2u) << "replaced in place";
  EXPECT_EQ(sheet.find("body")->alignment, TextAlignment::kCenter);
}
