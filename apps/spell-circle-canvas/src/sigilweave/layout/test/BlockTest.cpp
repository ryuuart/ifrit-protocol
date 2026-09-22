/** @file
 * The block partial: what it overlays, what it leaves alone, and the sheet
 * of named partials.
 */

#include <gtest/gtest.h>
#include <sigilweave/layout/Block.h>
#include <sigilweave/layout/StyleSheet.h>

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

TEST(StyleSheet, ARuleHasATypeHalfAndABlockHalfAndANameStatedAgainAdds) {
  Block wide;
  wide.leading = Leading::multiple(2.0f);
  // Spelled by the half each line names, and once with the verbs; "body"
  // twice is one rule with both halves.
  const sigil::weave::StyleSheet sheet{
      {"body", {.size = 24.0f}},
      {"body", wide},
      {"lead", {.widowLines = 3}},
      sigil::weave::rule("note").font({.size = 9.0f}).block({.widowLines = 2}),
  };
  ASSERT_EQ(sheet.size(), 3u);
  ASSERT_NE(sheet.find("body"), nullptr);
  EXPECT_EQ(*sheet.find("body")->type().size, 24.0f);
  EXPECT_EQ(sheet.find("body")->block().leading, wide.leading);
  EXPECT_EQ(*sheet.find("lead")->block().widowLines, 3);
  EXPECT_FALSE(sheet.find("lead")->type().size.has_value());
  EXPECT_EQ(*sheet.find("note")->type().size, 9.0f);
  EXPECT_EQ(*sheet.find("note")->block().widowLines, 2);
  EXPECT_EQ(sheet.find("nope"), nullptr);
  EXPECT_EQ(sheet.rules()[0].name(), "body") << "rules keep their order";
}

TEST(StyleSheet, TheTypeHalfIsATypeSheetWithTheBase) {
  TextStyle base;
  base.shaping.fontSize = 19.5f;
  const sigil::weave::StyleSheet sheet{
      base, {{"big", {.size = 40.0f}}, {"lead", {.widowLines = 3}}}};
  const TypeSheet half = sheet.types();
  EXPECT_FLOAT_EQ(half.base().shaping.fontSize, 19.5f);
  ASSERT_NE(half.find("big"), nullptr);
  EXPECT_EQ(*half.find("big")->size, 40.0f);
  ASSERT_NE(half.find("lead"), nullptr)
      << "a block-only rule is a name with nothing over the base";
  EXPECT_FALSE(half.find("lead")->size.has_value());
}
