/** @file
 * The initial letter: how deep the notch it cuts is, the size the two
 * alignments derive, where its baseline lands when it sinks, and the same
 * notch cut out of a column flow and a contour flow, which are given it by
 * the wrapper rather than by anything written for either.
 */

#include <gtest/gtest.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPath.h>

#include <string>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// The cap height the instrument reports at a size — the metric the sizing
/// rule reads, taken here the same way so a claim about the rule is a claim
/// about arithmetic and not about a number typed twice.
float capHeightAt(float fontSize) {
  SkFont font = makeFont(sigil::test::instrument::sans(), fontSize);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  return metrics.fCapHeight;
}

const std::u8string& passage() {
  static const std::u8string text =
      u8"Whale roads open under a sky the colour of pewter and the boats "
      u8"go out before the light does, one after another, until the whole "
      u8"harbour is empty and the gulls have the quay to themselves again.";
  return text;
}

}  // namespace

TEST(InitialLetter, TheNotchIsAsManyBandsDeepAsTheInitialSinks) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(passage(), 14.0f);
  BlockFlow flow(SkRect::MakeWH(300, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.initial = {.lines = 3, .margin = 6.0f};
  options.blocks = {style};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_TRUE(layout.initial.placed);
  // Three lines of cap, sunk by two: the initial stands on bands 0, 1 and 2
  // and band 3 is the first whole line.
  EXPECT_EQ(layout.initial.bands, 3);
  ASSERT_GE(layout.lineCount, 5);
  const std::vector<float> starts = lineStarts(layout);
  ASSERT_GE(starts.size(), 4u);
  EXPECT_NEAR(starts[1], layout.initial.notch, 0.5f);
  EXPECT_NEAR(starts[2], layout.initial.notch, 0.5f);
  EXPECT_NEAR(starts[3], 0.0f, 0.5f);
  EXPECT_GT(layout.initial.notch, 6.0f);
}

TEST(InitialLetter, AOneLineInitialIsSizedToTheFirstLinesOwnCapHeight) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(passage(), 14.0f);
  BlockFlow flow(SkRect::MakeWH(300, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.initial = {.lines = 1};
  options.blocks = {style};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_TRUE(layout.initial.placed);
  // One line of cap: the initial's top reference and the first line's are
  // the same point and its baseline is the first baseline, so the two
  // alignments leave it exactly the first line's cap height tall — which is
  // the rule stating the identity case.
  const Paragraph::Strut strut = paragraph.strut(fonts);
  EXPECT_NEAR(capHeightAt(layout.initial.fontSize), strut.capHeight, 0.05f);
}

TEST(InitialLetter, ThreeLinesOfCapReachFromTheFirstCapTopToTheThirdBaseline) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(passage(), 14.0f);
  BlockFlow flow(SkRect::MakeWH(300, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.initial = {.lines = 3};
  options.blocks = {style};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_TRUE(layout.initial.placed);
  const Paragraph::Strut strut = paragraph.strut(fonts);
  const float span = 2.0f * layout.linePitch + strut.capHeight;
  EXPECT_NEAR(capHeightAt(layout.initial.fontSize), span, 0.05f);
  // And the top of that cap is the first line's cap top, which is what the
  // alignment says and what a reader sees.
  const std::vector<float> lines = baselines(layout);
  ASSERT_GE(lines.size(), 3u);
  EXPECT_NEAR(
      layout.initial.baseline.y() - capHeightAt(layout.initial.fontSize),
      lines.front() - strut.capHeight, 0.5f);
}

TEST(InitialLetter, ASinkOfOnePutsTheInitialsBaselineOnTheSecondLine) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(passage(), 14.0f);
  BlockFlow flow(SkRect::MakeWH(300, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.initial = {.lines = 3, .sink = 1};
  options.blocks = {style};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_TRUE(layout.initial.placed);
  const std::vector<float> lines = baselines(layout);
  ASSERT_GE(lines.size(), 2u);
  // Sunk by one, so it sits on the second line's baseline and the notch it
  // cuts is two bands deep — a raised cap, its top a line above the first.
  EXPECT_NEAR(layout.initial.baseline.y(), lines[1], 0.5f);
  EXPECT_EQ(layout.initial.bands, 2);
}

TEST(InitialLetter, AColumnFlowGetsTheNotchTheSameWrapperCuts) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(passage(), 14.0f);
  paragraph.setWritingMode(WritingMode::kVerticalRL);
  ExclusionFlow flow(SkRect::MakeWH(300, 400), FlowAxis::kColumns);
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.initial = {.lines = 2, .margin = 4.0f};
  options.blocks = {style};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_TRUE(layout.initial.placed);
  EXPECT_EQ(layout.initial.bands, 2);
  ASSERT_GE(layout.intervals.size(), 3u);
  // A column is a line turned a quarter turn, so the notch is the same pen
  // travel off the head of the columns the initial stands in and none off
  // the one after them. The FIRST column also carries what is left of the
  // word the initial split, so the second is where the notch alone shows.
  EXPECT_NEAR(layout.intervals[1].origin.y() - flow.bounds().top(),
              layout.initial.notch, 1.0f);
  EXPECT_NEAR(layout.intervals[2].origin.y(), flow.bounds().top(), 1.0f);
}

TEST(InitialLetter, APathFlowGetsTheNotchAsArcLengthOffItsContour) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"Whale roads open under a sky", 14.0f);
  PathFlow flow(SkPath::Circle(200, 200, 150));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.initial = {.lines = 2, .margin = 4.0f};
  options.blocks = {style};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_FALSE(layout.intervals.empty());
  // The notch is pen travel taken off the head of the band, and on a
  // contour that is arc length: the line starts that far round the ring.
  EXPECT_GT(layout.intervals[0].contourStart, 0.0f);
  EXPECT_TRUE(layout.initial.placed);
}
