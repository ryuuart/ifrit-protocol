/** @file
 * Relayout locality: a one-word edit reshapes one word and leaves the other
 * blobs shared, and a moving exclusion repositions without reshaping.
 */

#include <gtest/gtest.h>
#include <include/core/SkTextBlob.h>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Incremental, OneWordEditKeepsOtherWordBlobs) {
  FontContext& fontContext = sharedContext();
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
  FontContext& fontContext = sharedContext();
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
