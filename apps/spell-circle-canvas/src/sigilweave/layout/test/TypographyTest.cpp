/** @file
 * Typographic options through the breakers: last-line alignment, soft
 * hyphens rendered and refused, a span restyle across lines, and word
 * spacing consumed by the breaker and the natural width.
 */

#include <gtest/gtest.h>
#include <include/core/SkTextBlob.h>

#include <algorithm>
#include <string>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Typography, TheLastLineTakesItsOwnAlignmentAndNotTheParagraphs) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"a justified paragraph whose final line is pushed to the right edge "
      "instead of hanging on the left like usual short last lines do");
  BlockFlow flow(SkRect::MakeWH(260, 600));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.justification.lastLineAlignment = TextAlignment::kEnd;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_GT(layout.lineCount, 1);

  // The last line's last word must end at the right edge, and its first run
  // must not start at x=0.
  float lastLineEnd = 0, lastLineStart = 1e9f;
  for (const PositionedRun& run : layout.runs) {
    if (run.lineIndex != layout.lineCount - 1) continue;
    lastLineEnd = std::max(lastLineEnd, runEnd(paragraph, run));
    lastLineStart = std::min(lastLineStart, run.origin.x());
  }
  EXPECT_NEAR(lastLineEnd, 260.0f, 1.0f);
  EXPECT_GT(lastLineStart, 5.0f);
}

TEST(SoftHyphen, RendersHyphenOnBreak) {
  FontContext& fontContext = sharedContext();
  // "extra­ordinarily" fits neither whole nor as "extra" without the
  // discretionary break being taken on a narrow measure.
  Paragraph paragraph = makeParagraph(u8"an extra­ordinarily narrow measure");
  BlockFlow flow(SkRect::MakeWH(90, 300));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const Word* hyphenWord = nullptr;
  for (const Word& word : paragraph.words())
    if (word.hyphenBreak) hyphenWord = &word;
  ASSERT_NE(hyphenWord, nullptr);
  ASSERT_TRUE(hyphenWord->hyphenGlyph);

  // The hyphen glyph's shared blob must appear among the placed runs.
  const SkTextBlob* hyphenBlob = wordBlob(*hyphenWord->hyphenGlyph).get();
  bool found = false;
  for (const PositionedRun& run : layout.runs)
    found |= run.blob.get() == hyphenBlob;
  EXPECT_TRUE(found);
}

TEST(SoftHyphen, InvisibleWhenNotBroken) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"extra­ordinarily");
  BlockFlow flow(SkRect::MakeWH(500, 100));  // plenty of room: no break
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  ASSERT_EQ(paragraph.words().size(), 2u);  // "extra·" + "ordinarily"
  const Word& first = paragraph.words()[0];
  EXPECT_TRUE(first.hyphenBreak);
  EXPECT_FLOAT_EQ(first.spaceWidth, 0.0f);  // halves join with zero gap
  const SkTextBlob* hyphenBlob = wordBlob(*first.hyphenGlyph).get();
  for (const PositionedRun& run : layout.runs)
    EXPECT_NE(run.blob.get(), hyphenBlob) << "hyphen rendered without break";
  // Both halves sit on one line, adjacent.
  ASSERT_EQ(layout.lineCount, 1);
}

TEST(SoftHyphen, KnuthPlassTakesDiscretionaryBreaks) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"the as­ton­ish­ing­ly in­com­pre­hen"
      "­si­ble hy­phen­ation ma­chin­ery works");
  BlockFlow flow(SkRect::MakeWH(120, 600));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 2);
}

TEST(SoftHyphen, DisabledRemovesTheBreakOpportunity) {
  FontContext& fontContext = sharedContext();
  // One word, one soft hyphen, and a measure too narrow for the whole word:
  // the only thing that can decide the line count is whether the hyphen is a
  // break opportunity.
  Paragraph paragraph = makeParagraph(u8"extra­ordinarily");
  BlockFlow flow(SkRect::MakeWH(90, 300));

  ParagraphLayoutOptions hyphenating;
  hyphenating.hyphenation.enabled = true;
  ParagraphLayout broken =
      layoutParagraph(fontContext, paragraph, flow, hyphenating);
  ASSERT_EQ(paragraph.words().size(), 2u);  // "extra·" + "ordinarily"
  ASSERT_TRUE(paragraph.words()[0].hyphenBreak);
  EXPECT_EQ(broken.lineCount, 2);
  const SkTextBlob* hyphenBlob =
      wordBlob(*paragraph.words()[0].hyphenGlyph).get();
  bool hyphenOnFirstLine = false;
  for (const PositionedRun& run : broken.runs)
    hyphenOnFirstLine |= run.blob.get() == hyphenBlob && run.lineIndex == 0;
  EXPECT_TRUE(hyphenOnFirstLine);

  ParagraphLayoutOptions whole;
  whole.hyphenation.enabled = false;
  ParagraphLayout unbroken =
      layoutParagraph(fontContext, paragraph, flow, whole);
  // The halves fuse: one word, no discretionary break, one line — and it
  // overflows the measure rather than splitting, which is the whole point.
  ASSERT_EQ(paragraph.words().size(), 1u);
  EXPECT_FALSE(paragraph.words()[0].hyphenBreak);
  EXPECT_EQ(unbroken.lineCount, 1);
  ASSERT_FALSE(unbroken.runs.empty());
  float extent = 0;
  for (const PositionedRun& run : unbroken.runs)
    extent = std::max(extent, runEnd(paragraph, run));
  EXPECT_GT(extent, 90.0f);

  // Turning it back on restores the break, so the decision is not sticky.
  ParagraphLayout again =
      layoutParagraph(fontContext, paragraph, flow, hyphenating);
  EXPECT_EQ(paragraph.words().size(), 2u);
  EXPECT_EQ(again.lineCount, 2);
}

TEST(SoftHyphen, DisabledRemovesTheBreakOpportunityForKnuthPlass) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"the as­ton­ish­ing­ly in­com­pre­hen"
      "­si­ble hy­phen­ation ma­chin­ery works");
  BlockFlow flow(SkRect::MakeWH(120, 600));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;

  ParagraphLayout hyphenated =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_FALSE(hyphenated.overflowed());
  const size_t hyphenatedWordCount = paragraph.words().size();
  size_t hyphenBreakCount = 0;
  for (const Word& word : paragraph.words())
    hyphenBreakCount += word.hyphenBreak;
  ASSERT_GT(hyphenBreakCount, 0u);

  options.hyphenation.enabled = false;
  ParagraphLayout whole =
      layoutParagraph(fontContext, paragraph, flow, options);
  // Every fused pair is one word fewer, and nothing left in the list can be
  // broken at a hyphen — Knuth-Plass has no discretionary breaks to weigh.
  EXPECT_LT(paragraph.words().size(), hyphenatedWordCount);
  for (const Word& word : paragraph.words()) EXPECT_FALSE(word.hyphenBreak);
  EXPECT_FALSE(whole.overflowed());
}

TEST(Typography, AContinuousSpanOfEmphasisFollowsItsWordsOntoEveryLine) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"a long sentence that will certainly wrap across several lines gets "
      "one continuous span of emphasis applied to its middle third and the "
      "styling must follow the words wherever the line breaker puts them");
  BlockFlow flow(SkRect::MakeWH(220, 600));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_GT(before.lineCount, 3);

  // Style the middle third, snapped to word boundaries.
  const std::u16string& text = paragraph.text();
  uint32_t from = static_cast<uint32_t>(text.find(u' ', text.size() / 3)) + 1;
  const uint32_t rangeEnd =
      static_cast<uint32_t>(text.find(u' ', 2 * text.size() / 3));
  paragraph.setPaint(from, rangeEnd, PaintStyle{SK_ColorRED});
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);

  // Red runs must exist on more than one line.
  const auto& spans = paragraph.spans();
  int firstRedLine = -1, lastRedLine = -1;
  for (const PositionedRun& run : after.runs) {
    if (run.styleIndex < spans.size() &&
        spans[run.styleIndex].style.paint.foreground.getColor() ==
            SK_ColorRED) {
      if (firstRedLine < 0) firstRedLine = run.lineIndex;
      lastRedLine = run.lineIndex;
    }
  }
  ASSERT_GE(firstRedLine, 0);
  EXPECT_GT(lastRedLine, firstRedLine);
}

TEST(WordSpacingTest, BreakerAndNaturalWidthConsumeIt) {
  FontContext& fontContext = sharedContext();
  TextStyle spaced = basicStyle();
  spaced.shaping.wordSpacing = 40.0f;
  Paragraph wide;
  wide.appendText(u8"one two three four five", spaced);
  Paragraph normal = makeParagraph(u8"one two three four five");
  // Natural width grows by exactly 4 gaps × 40px.
  EXPECT_NEAR(wide.naturalWidth(fontContext),
              normal.naturalWidth(fontContext) + 4 * 40.0f, 0.01f);
  // A measure that fits the normal text on one line wraps the spaced one.
  const float measure = normal.naturalWidth(fontContext) + 20.0f;
  BlockFlow flowNormal(SkRect::MakeWH(measure, 400));
  BlockFlow flowWide(SkRect::MakeWH(measure, 400));
  EXPECT_EQ(layoutParagraph(fontContext, normal, flowNormal).lineCount, 1);
  EXPECT_GT(layoutParagraph(fontContext, wide, flowWide).lineCount, 1);
}
