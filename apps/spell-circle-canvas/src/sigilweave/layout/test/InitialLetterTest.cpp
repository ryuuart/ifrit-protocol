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

TEST(InitialLetter, AnInitialOnALaterBlockOpensThatBlockAndNotTheFirst) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = twoBlocks();
  BlockFlow flow(SkRect::MakeWH(300, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle plain;
  ParagraphStyle dropped;
  dropped.initial = {.lines = 2, .margin = 4.0f};
  options.blocks = {plain, dropped};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_TRUE(layout.initial.placed);
  const std::vector<float> lines = baselines(layout);
  ASSERT_GE(lines.size(), 3u);
  // The initial belongs to the second block, so it sits on that block's
  // first baseline and not on the paragraph's.
  EXPECT_GT(layout.initial.baseline.y(), lines.front() + 1.0f);

  // The runs stay in logical order: the second block's cap never stands
  // before the first block's words.
  ASSERT_FALSE(layout.runs.empty());
  EXPECT_EQ(layout.runs.front().wordIndex, 0u);
  for (size_t index = 1; index < layout.runs.size(); ++index)
    EXPECT_LE(layout.runs[index - 1].wordIndex, layout.runs[index].wordIndex)
        << "run " << index;

  // And it reports the line it is on, which is a line of its own block.
  int capLine = -1;
  for (const PositionedRun& run : layout.runs)
    if (run.origin == layout.initial.baseline) capLine = run.lineIndex;
  ASSERT_GE(capLine, 1) << "the cap is not on the paragraph's first line";
}

TEST(InitialLetter, ABlockShorterThanTheSinkKeepsTheNextBlocksLinesClear) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"Ah.\n"
      u8"The second block runs on for long enough to wrap several times "
      u8"under the cap the block above it opened with.",
      16.0f);
  BlockFlow flow(SkRect::MakeWH(300, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle dropped;
  dropped.initial = {.lines = 3, .margin = 6.0f};
  options.blocks = {dropped, ParagraphStyle{}};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_TRUE(layout.initial.placed);
  EXPECT_EQ(layout.initial.bands, 3);
  const std::vector<float> starts = lineStarts(layout);
  ASSERT_GE(starts.size(), 4u);
  // The block the initial opened has one line; the two bands the cap still
  // stands in belong to the block after it, and they stand off it exactly
  // as its own line would have.
  EXPECT_NEAR(starts[1], layout.initial.notch, 0.5f);
  EXPECT_NEAR(starts[2], layout.initial.notch, 0.5f);
  EXPECT_NEAR(starts[3], 0.0f, 0.5f);
}

TEST(InitialLetter, AGlyphWrapMeasuresTheOutlineAndNotTheAdvanceBox) {
  FontContext& fonts = sigil::test::fonts();
  BlockFlow flow(SkRect::MakeWH(300, 400));

  const auto startsUnder = [&](InitialLetter::Wrap wrap) {
    Paragraph paragraph = makeParagraph(passage(), 14.0f);
    ParagraphLayoutOptions options;
    ParagraphStyle style;
    style.initial = {.lines = 3, .wrap = wrap, .margin = 4.0f};
    options.blocks = {style};
    return lineStarts(layoutParagraph(fonts, paragraph, flow, options));
  };

  const std::vector<float> box = startsUnder(InitialLetter::Wrap::kBox);
  const std::vector<float> glyph = startsUnder(InitialLetter::Wrap::kGlyph);
  ASSERT_GE(box.size(), 4u);
  ASSERT_GE(glyph.size(), 4u);
  // The ink of a letter stops short of its advance, so a line tucks in
  // closer under the outline than under the box on every band the initial
  // covers — and neither cuts the band below it.
  EXPECT_LT(glyph[1], box[1]);
  EXPECT_LT(glyph[2], box[2]);
  EXPECT_NEAR(glyph[3], 0.0f, 0.5f);
  EXPECT_NEAR(box[3], 0.0f, 0.5f);
}

TEST(InitialLetter, ANegativeSinkLeavesTheCapOnTheFirstBaseline) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(passage(), 14.0f);
  BlockFlow flow(SkRect::MakeWH(300, 400));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.initial = {.lines = 3, .sink = -2};
  options.blocks = {style};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);

  ASSERT_TRUE(layout.initial.placed);
  const std::vector<float> lines = baselines(layout);
  ASSERT_FALSE(lines.empty());
  // Nothing sits above the first baseline: a sink is how far DOWN the cap
  // goes, and the first line is as high as the frame goes.
  EXPECT_NEAR(layout.initial.baseline.y(), lines.front(), 0.5f);
  EXPECT_EQ(layout.initial.bands, 1);
}

TEST(InitialLetter, TheInitialTakesTheGraphemesItAsksForAndNoMoreThanTheWord) {
  FontContext& fonts = sigil::test::fonts();
  BlockFlow flow(SkRect::MakeWH(300, 400));

  const auto takenBy = [&](uint32_t graphemes) {
    Paragraph paragraph = makeParagraph(passage(), 14.0f);
    ParagraphLayoutOptions options;
    ParagraphStyle style;
    style.initial = {.lines = 2, .graphemes = graphemes};
    options.blocks = {style};
    return layoutParagraph(fonts, paragraph, flow, options).initial.textEnd;
  };

  EXPECT_EQ(takenBy(1), 1u);
  EXPECT_EQ(takenBy(2), 2u);
  // "Whale" is five letters, and an initial asked for more takes the word
  // and never the space after it.
  EXPECT_EQ(takenBy(9), 5u);
}

TEST(InitialLetter, AResumedPassDoesNotOpenTheInitialAgain) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(passage(), 14.0f);
  BlockFlow shallow(SkRect::MakeWH(300, 60));
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  style.initial = {.lines = 2, .margin = 4.0f};
  options.blocks = {style};

  const ParagraphLayout first =
      layoutParagraph(fonts, paragraph, shallow, options);
  ASSERT_TRUE(first.initial.placed);
  ASSERT_TRUE(first.overflowed());

  // The frame after it resumes the same block, and the initial belongs to
  // the frame the block began in.
  const ParagraphLayout resumed = layoutParagraph(
      fonts, paragraph, shallow, options, first.firstUnplacedWord);
  EXPECT_FALSE(resumed.initial.placed);
  const std::vector<float> starts = lineStarts(resumed);
  ASSERT_FALSE(starts.empty());
  EXPECT_NEAR(starts.front(), 0.0f, 0.5f) << "no notch is cut a second time";
}

TEST(InitialLetter, AColumnsInitialStandsUprightAtTheHeadOfItsColumn) {
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
  // The cap is set down the column, like everything around it, so its top
  // is the column's head and its box runs down the column and not across
  // the page.
  const PositionedRun* cap = nullptr;
  for (const PositionedRun& run : layout.runs)
    if (run.origin == layout.initial.baseline) cap = &run;
  ASSERT_NE(cap, nullptr);
  ASSERT_NE(cap->shaped, nullptr);
  EXPECT_TRUE(cap->shaped->vertical);
  EXPECT_NEAR(layout.initial.box.top(), flow.bounds().top(), 1.0f);
  EXPECT_NEAR(layout.initial.box.height(), cap->shaped->advance, 0.5f);
  EXPECT_NEAR(layout.initial.box.width(), layout.initial.fontSize, 0.5f);
  // The notch a column loses is that same pen travel, plus the standoff.
  EXPECT_NEAR(layout.initial.notch, cap->shaped->advance + 4.0f, 0.5f);
}

TEST(InitialLetter, TheLineTheCapStandsOnKeepsItsOwnBand) {
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
  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_GE(lines.size(), 3u);
  // A band a selection is drawn from is the LINE's, not the cap's: three
  // lines of cap would otherwise make the line it sits on three times as
  // tall as its neighbours.
  EXPECT_NEAR(lines[0].ascent, lines[1].ascent, 0.5f);
  EXPECT_LT(lines[0].ascent, layout.initial.fontSize * 0.5f);
  // The cap's own reach is reported where a caller looks for it.
  EXPECT_GT(layout.initial.box.height(), layout.linePitch * 2.0f);
}
