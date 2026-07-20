/** @file
 * The Paragraph document model: edits preserving styles,
 * incremental reshape locality, and inline placeholders.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <absl/container/flat_hash_set.h>

#include <string>
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Paragraph, ReplaceTextPreservesSurroundingStyles) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  TextStyle red = basicStyle();
  red.paint.foreground.setColor(SK_ColorRED);
  TextStyle blue = basicStyle();
  blue.paint.foreground.setColor(SK_ColorBLUE);
  paragraph.appendText(u8"red ", red);
  paragraph.appendText(u8"blue", blue);

  paragraph.replaceText(4, 8, u8"teal"); // swap the blue word's text
  paragraph.ensureShaped(fontContext);
  ASSERT_GE(paragraph.spans().size(), 2u);
  EXPECT_EQ(paragraph.spans().front().style.paint.foreground.getColor(),
            SK_ColorRED);
  // Inserted text inherits the style at the edit point (the blue span).
  EXPECT_EQ(paragraph.spans().back().style.paint.foreground.getColor(),
            SK_ColorBLUE);
  EXPECT_EQ(paragraph.text(), u"red teal");
}
// ── Incremental behavior end-to-end ───────────────────────────────────────

TEST(Incremental, OneWordEditKeepsOtherWordBlobs) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"steady text with one word that will change between frames while all "
      "other words keep their shaped blobs perfectly intact");
  BlockFlow flow(SkRect::MakeWH(300, 600));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);

  paragraph.replaceText(17, 20, u8"two"); // "one" → "two"
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);

  // Blobs are shared via the shape cache: unchanged words reuse the very
  // same SkTextBlob instances across the edit.
  size_t sharedBlobs = 0;
  for (const PositionedRun &beforeRun : before.runs)
    for (const PositionedRun &afterRun : after.runs)
      if (beforeRun.blob.get() == afterRun.blob.get())
        sharedBlobs++;
  EXPECT_GT(sharedBlobs, before.runs.size() / 2);
}

TEST(Incremental, MovingExclusionOnlyRepositions) {
  FontContext &fontContext = sharedContext();
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
// ── Inline placeholders (pills / images woven into the flow) ─────────────

TEST(Placeholders, ReservesWidthInTheLine) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"before ", basicStyle());
  paragraph.appendPlaceholder({90, 20, 0}, basicStyle());
  paragraph.appendText(u8" after", basicStyle());

  BlockFlow flow(SkRect::MakeWH(600, 60)); // everything on one line
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_FLOAT_EQ(rects[0].rect.width(), 90);
  EXPECT_FLOAT_EQ(rects[0].rect.height(), 20);

  // "after" starts past the slot's right edge.
  float afterX = -1;
  for (const PositionedRun &run : layout.runs) {
    if (run.placeholderIndex >= 0)
      continue;
    const Word &word = paragraph.words()[run.wordIndex];
    const std::u16string_view text(paragraph.text());
    if (text.substr(word.textBegin, 5) == u"after")
      afterX = run.origin.x();
  }
  ASSERT_GE(afterX, 0);
  EXPECT_GE(afterX, rects[0].rect.right() - 0.25f);
}

TEST(Placeholders, SitOnTheBaselineWithDrop) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"x ", basicStyle());
  paragraph.appendPlaceholder({40, 30, 8},
                              basicStyle()); // bottom 8px below base
  BlockFlow flow(SkRect::MakeWH(300, 60));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  float baselineY = -1;
  for (const PositionedRun &run : layout.runs)
    if (run.placeholderIndex < 0)
      baselineY = run.origin.y();
  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_FLOAT_EQ(rects[0].rect.bottom(), baselineY + 8);
  EXPECT_FLOAT_EQ(rects[0].rect.top(), baselineY + 8 - 30);
}

TEST(Placeholders, WrapAndJustifyLikeWords) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  for (int placeholderIndex = 0; placeholderIndex < 6; ++placeholderIndex) {
    paragraph.appendText(u8"word word word ", basicStyle());
    paragraph.appendPlaceholder({60, 14, 0}, basicStyle());
    paragraph.appendText(u8" ", basicStyle());
  }
  BlockFlow flow(SkRect::MakeWH(220, 400));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  const auto rects = layout.placeholderRects(paragraph);
  ASSERT_EQ(rects.size(), 6u);
  absl::flat_hash_set<int> lines;
  for (const auto &placed : rects) {
    lines.insert(placed.lineIndex);
    // Slots never overflow the measure.
    EXPECT_GE(placed.rect.left(), -0.25f);
    EXPECT_LE(placed.rect.right(), 220.5f);
  }
  EXPECT_GT(lines.size(), 1u) << "slots must wrap onto later lines";
}

TEST(Placeholders, ResizeRelayoutsLive) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph;
  paragraph.appendText(u8"pill: ", basicStyle());
  paragraph.appendPlaceholder({50, 16, 0}, basicStyle());
  BlockFlow flow(SkRect::MakeWH(400, 60));
  ParagraphLayout before = layoutParagraph(fontContext, paragraph, flow);

  fontContext.resetStats();
  paragraph.setPlaceholder(0, {120, 16, 0});
  ParagraphLayout after = layoutParagraph(fontContext, paragraph, flow);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "resizing a slot reshapes nothing";
  EXPECT_FLOAT_EQ(after.placeholderRects(paragraph)[0].rect.width(), 120);
}
