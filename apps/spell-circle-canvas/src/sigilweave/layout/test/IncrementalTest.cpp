/** @file
 * Relayout locality: a one-word edit reshapes one word and leaves the other
 * blobs shared, a moving exclusion repositions without reshaping, and a
 * paint edit over an overflowed paragraph leaves the overflow where it was.
 */

#include <gtest/gtest.h>
#include <include/core/SkTextBlob.h>

#include <cstddef>
#include <cstdint>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Incremental, OneWordEditKeepsOtherWordBlobs) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"steady text with one word that will change between frames while all "
      "other words keep their shaped blobs perfectly intact");
  BlockFlow flow(SkRect::MakeWH(300, 600));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);

  paragraph.replaceText(17, 20, u8"two");  // "one" → "two"
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);

  // Blobs are shared via the shape cache: unchanged words reuse the very
  // same SkTextBlob instances across the edit.
  size_t sharedBlobs = 0;
  for (const PositionedRun& beforeRun : before.runs)
    for (const PositionedRun& afterRun : after.runs)
      if (beforeRun.blob.get() == afterRun.blob.get()) sharedBlobs++;
  EXPECT_GT(sharedBlobs, before.runs.size() / 2);
}

TEST(Incremental, MovingAnExclusionCostsNoCallToTheShaper) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"the shape moves through the paragraph and every frame the words "
      "reflow around it without any reshaping at all, just new positions");
  ExclusionFlow flow(SkRect::MakeWH(360, 400));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(50, 30, 90, 90), 4});

  ParagraphLayout first = layoutParagraph(fontContext, paragraph, flow);
  fontContext.resetStats();
  flow.shapes()[0].bounds.offset(60, 25);
  ParagraphLayout second = layoutParagraph(fontContext, paragraph, flow);

  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "moving a shape must not reshape";
  EXPECT_FALSE(second.runs.empty());
}

TEST(Incremental, APaintEditLeavesAnOverflowExactlyWhereItWas) {
  // A paint edit carries no geometry, so the frame it lands on must come
  // back with the same runs at the same origins and the same first
  // unplaced word — including when almost none of the text was placed.
  FontContext& fontContext = sigil::test::fonts();
  static constexpr const char8_t* kWordPool[] = {
      u8"letters", u8"falling", u8"gently", u8"against", u8"words",
      u8"beacon",  u8"steady",  u8"rhythm", u8"turing",  u8"flow",
      u8"lattice", u8"shapes",  u8"glyphs", u8"marker",  u8"cache"};
  Paragraph paragraph;
  paragraph.appendText(makePooledText(kWordPool, 4000, 7), basicStyle());
  BlockFlow flow(SkRect::MakeWH(420, 320));
  const ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_TRUE(before.overflowed());

  // Repaint the window that was actually placed, and nothing beyond it.
  const uint32_t placedEnd =
      paragraph.words()[before.firstUnplacedWord].textBegin;
  paragraph.setPaint(0, placedEnd, PaintStyle(0xFFCC0000));

  const ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_EQ(after.firstUnplacedWord, before.firstUnplacedWord);
  ASSERT_EQ(after.runs.size(), before.runs.size());
  for (size_t index = 0; index < after.runs.size(); ++index)
    EXPECT_EQ(after.runs[index].origin, before.runs[index].origin);
}
