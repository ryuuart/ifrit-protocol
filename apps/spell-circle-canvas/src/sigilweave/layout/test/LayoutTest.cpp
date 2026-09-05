/** @file
 * layoutParagraph() placement into a block: the line-width invariant both
 * breakers uphold, alignment, mandatory breaks, exclusion, word spacing, a
 * span restyle that follows its words, bidi visual order, an edit at a
 * surrogate boundary, and the per-line metrics derived from the runs. How
 * a justified line is fitted is its own subject and its own file.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── Line-width invariant, both breakers ──────────────────────────────────

namespace {

/// The one width invariant every breaker must uphold: no placed run may
/// stick out past the measure — overfull lines are infeasible unless there
/// is truly no alternative. Greedy and Knuth-Plass are held to the
/// identical standard on the same hyphen-laden text across a sweep of
/// measures.
class LineWidthInvariant : public BrokenBothWays {};

}  // namespace

TEST_P(LineWidthInvariant, LinesNeverExceedTheMeasure) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(
      u8"The para­graph breaker con­sid­ers every way to break "
      "this text into lines and picks the one with the least bad­ness, "
      "ex­act­ly like TeX. Greedy breaking com­mits line by "
      "line and leaves rag­ged, in­con­sis­tent "
      "spac­ing be­hind; op­ti­mal breaking spreads the "
      "slack across the whole para­graph in­stead.",
      basicStyle(17.0f));

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineBreakStrategy = breaker();
  options.lineMetrics.height = 27;

  for (int measureStep = 150; measureStep <= 430; measureStep += 7) {
    const float measure = static_cast<float>(measureStep);
    BlockFlow flow(SkRect::MakeWH(measure, 3000));
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    EXPECT_FALSE(layout.overflowed());
    for (const PositionedRun& run : layout.runs) {
      if (!run.shaped) continue;
      const float end = run.origin.x() + run.shaped->advance;
      EXPECT_LE(end, measure + 0.75f)
          << "line " << run.lineIndex << " leaks past the " << measure
          << "px measure";
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Breakers, LineWidthInvariant, bothBreakers(),
                         breakerName);

TEST(ParagraphLayout, MandatoryBreakStartsNewLine) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"alpha\nbeta");
  BlockFlow flow(SkRect::MakeWH(500, 300));
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  ASSERT_EQ(layout.runs.size(), 2u);
  EXPECT_NE(layout.runs[0].lineIndex, layout.runs[1].lineIndex);
  EXPECT_LT(layout.runs[0].origin.y(), layout.runs[1].origin.y());
}

TEST(ParagraphLayout, CentringHalvesTheSlackAndEndAlignmentTakesItAll) {
  FontContext& fontContext = sigil::test::fonts();
  ParagraphLayoutOptions options;

  Paragraph paragraph = makeParagraph(u8"word");
  BlockFlow flow(SkRect::MakeWH(400, 100));

  options.alignment = TextAlignment::kStart;
  const float startX =
      layoutParagraph(fontContext, paragraph, flow, options).runs[0].origin.x();
  options.alignment = TextAlignment::kCenter;
  const float centerX =
      layoutParagraph(fontContext, paragraph, flow, options).runs[0].origin.x();
  options.alignment = TextAlignment::kEnd;
  const float endX =
      layoutParagraph(fontContext, paragraph, flow, options).runs[0].origin.x();

  EXPECT_FLOAT_EQ(startX, 0);
  EXPECT_GT(centerX, startX);
  EXPECT_GT(endX, centerX);
  EXPECT_NEAR(centerX * 2, endX, 1.0f);
}

TEST(ParagraphLayout, JustifiedLinesFillTheMeasure) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"justification stretches the spaces between words so every full line "
      "extends to the right edge of the measure exactly");
  BlockFlow flow(SkRect::MakeWH(260, 600));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_GT(layout.lineCount, 2);

  // Every line except the last must reach (near) the right edge.
  const std::vector<float> ends = lineEnds(layout, paragraph);
  for (int line = 0; line + 1 < layout.lineCount; ++line)
    EXPECT_NEAR(ends[static_cast<size_t>(line)], 260.0f, 3.0f)
        << "line " << line << " not justified";
  EXPECT_LT(ends.back(), 260.0f);  // ragged last line
}

TEST(ParagraphLayout, ExclusionShapeSplitsText) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"text flows around the shape and continues on the far side of it, "
      "filling both fragments of every interrupted line with words");
  ExclusionFlow flow(SkRect::MakeWH(400, 300));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(140, 40, 120, 120), 6});
  flow.setMinIntervalWidth(40);
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // Some line must have runs both left and right of the circle.
  bool split = false;
  for (int line = 0; line < layout.lineCount && !split; ++line) {
    bool left = false, right = false;
    for (const PositionedRun& run : layout.runs) {
      if (run.lineIndex != line) continue;
      if (run.origin.x() < 140) left = true;
      if (run.origin.x() > 260) right = true;
    }
    split = left && right;
  }
  EXPECT_TRUE(split);
}

TEST(ParagraphLayout, WordSpacingReachesTheBreakerAndTheNaturalWidth) {
  FontContext& fontContext = sigil::test::fonts();
  TextStyle spaced = basicStyle();
  spaced.shaping.wordSpacing = 40.0f;
  Paragraph wide;
  wide.appendText(u8"one two three four five", spaced);
  Paragraph normal = makeParagraph(u8"one two three four five");
  // Natural width grows by exactly four gaps of forty pixels.
  EXPECT_NEAR(wide.naturalWidth(fontContext),
              normal.naturalWidth(fontContext) + 4 * 40.0f, 0.01f);
  // A measure that fits the normal text on one line wraps the spaced one,
  // so the breaker fitted against the same widths placement spends.
  const float measure = normal.naturalWidth(fontContext) + 20.0f;
  BlockFlow flowNormal(SkRect::MakeWH(measure, 400));
  BlockFlow flowWide(SkRect::MakeWH(measure, 400));
  EXPECT_EQ(layoutParagraph(fontContext, normal, flowNormal).lineCount, 1);
  EXPECT_GT(layoutParagraph(fontContext, wide, flowWide).lineCount, 1);
}

TEST(ParagraphLayout, AContinuousSpanOfEmphasisFollowsItsWordsOntoEveryLine) {
  FontContext& fontContext = sigil::test::fonts();
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

// ── Bidi order, and an edit at a surrogate boundary ─────────────────────

TEST(BidiOrder, AReorderedPairRendersInVisualOrderBetweenItsNeighbours) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"aaa בבב גגג zzz", 16.0f);
  BlockFlow flow(SkRect::MakeWH(600, 60));  // one wide line
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  // Logical order: aaa(0) בבב(1) גגג(2) zzz(3). UAX#9: the two RTL words
  // swap visually — גגג renders left of בבב, both between aaa and zzz.
  float runOrigins[4] = {0, 0, 0, 0};
  for (const PositionedRun& run : layout.runs)
    if (run.wordIndex < 4) runOrigins[run.wordIndex] = run.origin.x();
  EXPECT_LT(runOrigins[0], runOrigins[2]);
  EXPECT_LT(runOrigins[2], runOrigins[1])
      << "RTL pair must render in reversed visual order";
  EXPECT_LT(runOrigins[1], runOrigins[3]);
}

TEST(EditSafety, ACutThroughASurrogatePairLeavesEveryWordInsideTheText) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"ab 𝕏𝕐 cd");  // 𝕏/𝕐 are surrogate pairs
  paragraph.ensureShaped(fontContext);
  // Cut straight through the middle of the first surrogate pair.
  const size_t textOffset = paragraph.text().find(u"ab");
  ASSERT_NE(textOffset, std::u16string::npos);
  paragraph.replaceText(4, 5, u8"Z");  // [4,5) is inside a pair for this string
  paragraph.ensureShaped(fontContext);  // must not crash or emit garbage words
  for (const Word& word : paragraph.words())
    EXPECT_LE(word.textEnd, paragraph.text().size());
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph,
                      *std::make_unique<BlockFlow>(SkRect::MakeWH(400, 100)));
  EXPECT_FALSE(layout.runs.empty());
}

// ── Line metrics (ParagraphLayout::lineMetrics) ──────────────────────────

TEST(LineMetricsQuery, DescribesEveryPlacedLine) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      u8"enough words to wrap this paragraph across a handful of lines in "
      "a narrow measure so every line has real geometry to report");
  BlockFlow flow(SkRect::MakeXYWH(10, 20, 220, 600));
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 24;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  ASSERT_GT(layout.lineCount, 2);

  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), static_cast<size_t>(layout.lineCount));

  for (size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
    const LineMetrics& line = lines[lineNumber];
    EXPECT_EQ(line.lineIndex, static_cast<int>(lineNumber));
    EXPECT_GT(line.ascent, 0.0f);
    EXPECT_GT(line.descent, 0.0f);
    EXPECT_GT(line.right, line.left);
    EXPECT_GE(line.left, 10.0f);  // inside the flow bounds
    if (lineNumber > 0) {
      // Baselines descend by the configured line pitch.
      EXPECT_NEAR(line.baseline - lines[lineNumber - 1].baseline, 24.0f, 0.5f);
      // Character ranges advance monotonically and stay contiguous-ish
      // (each line starts where the previous one's glue ended).
      EXPECT_EQ(line.textBegin, lines[lineNumber - 1].textEnd);
    }
    // rect() is the ascent/descent band around the baseline.
    const SkRect band = line.rect();
    EXPECT_FLOAT_EQ(band.top(), line.baseline - line.ascent);
    EXPECT_FLOAT_EQ(band.bottom(), line.baseline + line.descent);
  }
  EXPECT_EQ(lines.front().textBegin, 0u);
  EXPECT_EQ(lines.back().textEnd,
            static_cast<uint32_t>(paragraph.text().size()));

  // Every run's geometry sits inside its line's band.
  for (const PositionedRun& run : layout.runs) {
    if (!run.shaped) continue;
    const LineMetrics& line = lines[static_cast<size_t>(run.lineIndex)];
    EXPECT_GE(run.origin.x(), line.left);
    EXPECT_LE(run.origin.x() + run.shaped->advance, line.right + 0.01f);
    EXPECT_FLOAT_EQ(run.origin.y(), line.baseline);
  }
}

TEST(LineMetricsQuery, MixedFontsGrowTheLineBand) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph;
  paragraph.appendText(u8"small ", basicStyle(14.0f));
  paragraph.appendText(u8"HUGE", basicStyle(40.0f));
  BlockFlow flow(SkRect::MakeWH(600, 100));  // one line
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);

  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), 1u);

  Paragraph smallOnly = makeParagraph(u8"small", 14.0f);
  BlockFlow smallFlow(SkRect::MakeWH(600, 100));
  const std::vector<LineMetrics> smallLines =
      layoutParagraph(fontContext, smallOnly, smallFlow).lineMetrics(smallOnly);
  ASSERT_EQ(smallLines.size(), 1u);
  EXPECT_GT(lines[0].ascent, smallLines[0].ascent)
      << "the 40px span must raise the mixed line's ascent";
}
