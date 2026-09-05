/** @file
 * Tab stops: an explicit stop and the repeating interval that follows them,
 * a cell that already reached past its stop, a stop past the measure, the
 * three alignments a stop can pin its cell by, the leader that fills the
 * gap a stop opened, alignment and justification over a resolved line, and
 * a tab nobody configured.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// x origin of the run for the word whose content is `needle`.
float runOriginFor(const Paragraph& paragraph, const ParagraphLayout& layout,
                   std::u16string_view needle) {
  const std::u16string& text = paragraph.text();
  for (const PositionedRun& run : layout.runs) {
    const Word& word = paragraph.words()[run.wordIndex];
    if (std::u16string_view(text).substr(
            word.textBegin, word.textEnd - word.textBegin) == needle)
      return run.origin.x();
  }
  return -1.0f;
}

/// Both breakers resolve tab stops through the same placement path, so
/// every claim about a stop is a claim about both of them.
class TabbedLine : public BrokenBothWays {};

}  // namespace

TEST_P(TabbedLine, EveryPostTabRunStartsAtItsStop) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"ab\tlongerhead\tx\ncdef\tk\tyz");
  BlockFlow flow(SkRect::MakeWH(600, 90));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  options.tabStops.stops = {{120.0f}, {300.0f}};
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_EQ(layout.lineCount, 2);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"longerhead"), 120.0f);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"x"), 300.0f);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"k"), 120.0f);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"yz"), 300.0f);
}

TEST_P(TabbedLine, AWordThatCannotFitAfterItsStopWrapsInsteadOfLeaking) {
  // At its shaped space-equivalent width "head tail" fits the 200px
  // measure, but the tab pushes "tail" to the 180px stop where it cannot;
  // a breaker scoring lines at natural glue width would leak it past the
  // measure instead of wrapping.
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"head\ttail");
  BlockFlow flow(SkRect::MakeWH(200, 200));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  options.tabStops.stops = {{180.0f}};  // "tail" cannot fit after the stop
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_GT(layout.lineCount, 1) << "unfittable tabbed word wraps";
  EXPECT_FALSE(layout.overflowed());
  for (const PositionedRun& run : layout.runs)
    EXPECT_LE(runEnd(paragraph, run), 200.0f + 0.75f)
        << "tabbed line leaks past the measure";
}

TEST(TabStops, RepeatingIntervalAfterExplicitStops) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"a\tb\tc\td");
  BlockFlow flow(SkRect::MakeWH(800, 60));
  ParagraphLayoutOptions options;
  options.tabStops.stops = {{50.0f}};
  options.tabStops.interval = 100.0f;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_EQ(layout.lineCount, 1);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"b"), 50.0f);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"c"), 150.0f);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"d"), 250.0f);
}

TEST(TabStops, ContentPastStopAdvancesToNext) {
  FontContext& fontContext = sigil::test::fonts();
  // "wideenough" extends past the 40px stop, so the tab after it must jump
  // to the following stop instead of backing up.
  Paragraph paragraph = makeParagraph(u8"wideenoughcontent\tafter");
  BlockFlow flow(SkRect::MakeWH(800, 60));
  ParagraphLayoutOptions options;
  options.tabStops.stops = {{40.0f}, {400.0f}};
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"after"), 400.0f);
}

TEST(TabStops, EndAlignedStopPinsTheCellsEnd) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"Chapter\t12");
  BlockFlow flow(SkRect::MakeWH(400, 200));
  ParagraphLayoutOptions options;
  options.tabStops.stops = {TabStop{300.0f, TabStop::Align::kEnd}};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  ASSERT_GE(layout.runs.size(), 2u);
  const PositionedRun& figures = layout.runs.back();
  EXPECT_NEAR(runEnd(paragraph, figures), 300.0f, 0.5f);
}

TEST(TabStops, CharacterAlignedStopPinsTheDecimalPoint) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"Total\t1284.50");
  BlockFlow flow(SkRect::MakeWH(400, 200));
  ParagraphLayoutOptions options;
  options.tabStops.stops = {TabStop{280.0f, TabStop::Align::kCharacter, u'.'}};
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  ASSERT_GE(layout.runs.size(), 2u);
  // The figures start left of the stop and reach past it: the point sits on
  // it, so neither edge does.
  const PositionedRun& figures = layout.runs.back();
  EXPECT_LT(figures.origin.x(), 280.0f);
  EXPECT_GT(runEnd(paragraph, figures), 280.0f);
}

TEST(TabStops, LeaderFillsTheGapItOpened) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph plain = makeParagraph(u8"Chapter\t12");
  Paragraph led = makeParagraph(u8"Chapter\t12");
  BlockFlow flowA(SkRect::MakeWH(400, 200));
  BlockFlow flowB(SkRect::MakeWH(400, 200));
  ParagraphLayoutOptions bare;
  bare.tabStops.stops = {TabStop{300.0f}};
  ParagraphLayoutOptions dotted = bare;
  dotted.tabStops.stops.front().leader = u".";
  const size_t bareRuns =
      layoutParagraph(fonts, plain, flowA, bare).runs.size();
  const size_t dottedRuns =
      layoutParagraph(fonts, led, flowB, dotted).runs.size();
  EXPECT_GT(dottedRuns, bareRuns + 5);
}

TEST_P(TabbedLine, JustificationStretchesOnlyTheGapsPastTheStop) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"a\tbb cc dd");
  BlockFlow flow(SkRect::MakeWH(400, 60));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = breaker();
  options.alignment = TextAlignment::kJustify;
  options.justification.justifyLastLine = true;
  options.tabStops.stops = {{100.0f}};
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_EQ(layout.lineCount, 1);
  // The column stays pinned to its stop; only the gaps past the tab
  // stretch, and they absorb the entire slack to the measure.
  EXPECT_FLOAT_EQ(runOriginFor(paragraph, layout, u"bb"), 100.0f);
  EXPECT_NEAR(lineEnds(layout, paragraph).front(), 400.0f, 0.75f)
      << "tabbed line not justified";
}

INSTANTIATE_TEST_SUITE_P(Breakers, TabbedLine, bothBreakers(), breakerName);

TEST(TabStops, CenterAlignmentShiftsTheResolvedLine) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"a\tb");
  BlockFlow flow(SkRect::MakeWH(300, 60));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kCenter;
  options.tabStops.stops = {{100.0f}};
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_EQ(layout.lineCount, 1);
  const float aOrigin = runOriginFor(paragraph, layout, u"a");
  const float bOrigin = runOriginFor(paragraph, layout, u"b");
  // Stops are line-local: the column offset survives the shift, and the
  // slack splits evenly around the tab-resolved line width.
  EXPECT_FLOAT_EQ(bOrigin - aOrigin, 100.0f);
  EXPECT_GT(aOrigin, 0.0f);
  EXPECT_NEAR(aOrigin, 300.0f - lineEnds(layout, paragraph).front(), 0.5f)
      << "line not centered";
}

TEST(TabStops, UnconfiguredTabsStillMeasureAsSpaces) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph tab = makeParagraph(u8"a\tb");
  Paragraph space = makeParagraph(u8"a b");
  BlockFlow tabFlow(SkRect::MakeWH(400, 60));
  BlockFlow spaceFlow(SkRect::MakeWH(400, 60));
  ParagraphLayout tabLayout = layoutParagraph(fontContext, tab, tabFlow);
  ParagraphLayout spaceLayout = layoutParagraph(fontContext, space, spaceFlow);
  EXPECT_FLOAT_EQ(runOriginFor(tab, tabLayout, u"b"),
                  runOriginFor(space, spaceLayout, u"b"));
}
