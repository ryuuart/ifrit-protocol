/** @file
 * The block partial: what it overlays, what it leaves alone, and the sheet
 * of named partials.
 */

#include <gtest/gtest.h>
#include <sigilweave/layout/Block.h>

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

TEST(Block, AFieldWrittenAsAKeywordTakesTheBaseOrNothingAtAll) {
  // Every field of a block inherits, so `inherit` and `unset` are the
  // block in force above and `initial` is a field nobody stated, which is
  // the paragraph layout's own answer.
  Block base;
  base.alignment = TextAlignment::kCenter;
  base.widowLines = 4;
  Block stop;
  stop.keywords.set(BlockField::Alignment, Keyword::Initial);
  const Block stopped = overlay(base, stop);
  EXPECT_FALSE(stopped.alignment.has_value());
  EXPECT_EQ(stopped.widowLines, 4) << "a field it says nothing about";
  EXPECT_TRUE(stopped.keywords.empty()) << "a resolved block states none";

  Block keep;
  keep.keywords.set(BlockField::Alignment, Keyword::Unset);
  EXPECT_EQ(overlay(base, keep).alignment, TextAlignment::kCenter);

  // A keyword and a value in one partial are one layer, and the keyword
  // is what stands.
  Block both;
  both.alignment = TextAlignment::kEnd;
  both.keywords.set(BlockField::Alignment, Keyword::Inherit);
  EXPECT_EQ(overlay(base, both).alignment, TextAlignment::kCenter);

  // A merge ACCUMULATES rather than applying: it has no block in force to
  // resolve against, so the keyword rides to the overlay.
  Block own;
  merge(own, stop);
  EXPECT_FALSE(own.empty());
  EXPECT_FALSE(overlay(base, own).alignment.has_value());
}

TEST(Block, AKeywordOnAPartialHandedStraightToAStyleIsTheStylesOwnAnswer) {
  // Under no ancestor at all, `inherit` and `initial` come to the same
  // thing: the field nobody stated, which is what the whole style already
  // carries. It must not be a silent no-op at that door.
  ParagraphStyle base;
  base.alignment = TextAlignment::kCenter;
  Block stop;
  stop.alignment = TextAlignment::kEnd;
  stop.keywords.set(BlockField::Alignment, Keyword::Initial);
  EXPECT_EQ(overlay(base, stop).alignment, TextAlignment::kCenter)
      << "the value the partial also stated is covered by the keyword";

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kStart;
  apply(options, stop);
  EXPECT_EQ(options.alignment, TextAlignment::kStart);
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
