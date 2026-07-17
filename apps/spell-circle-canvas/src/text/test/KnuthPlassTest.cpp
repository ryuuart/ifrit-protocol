/** @file
 * Knuth-Plass optimal line breaking: validity, raggedness
 * versus greedy, and CJK justification.
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>
using namespace textflow;
using namespace textflow::test;

// ── ParagraphLayout: Knuth-Plass
// ───────────────────────────────────────────────────

namespace {

// Sum of squared leftover space across all full lines — the raggedness
// measure Knuth-Plass style breaking should not lose to greedy on.
float raggedness(const Paragraph &paragraph, const ParagraphLayout &layout,
                 float measure) {
  if (layout.lineCount <= 1)
    return 0;
  std::vector<float> lineEnds(static_cast<size_t>(layout.lineCount), 0.0f);
  for (const PositionedRun &run : layout.runs)
    lineEnds[static_cast<size_t>(run.lineIndex)] = std::max(
        lineEnds[static_cast<size_t>(run.lineIndex)], runEnd(paragraph, run));
  float total = 0;
  for (int line = 0; line + 1 < layout.lineCount; ++line) {
    const float slack = measure - lineEnds[static_cast<size_t>(line)];
    total += slack * slack;
  }
  return total;
}

} // namespace

TEST(KnuthPlass, ProducesValidLines) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"In olden times when wishing still helped one, there lived a king "
      "whose daughters were all beautiful; and the youngest was so beautiful "
      "that the sun itself, which has seen so much, was astonished whenever "
      "it shone in her face.");
  BlockFlow flow(SkRect::MakeWH(300, 900));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 3);
  // Width containment is covered by LineWidthInvariant (LayoutTest.cpp)
  // for both breakers; this test owns the ordering/validity assertions.
  // Words appear in order (logical == visual for pure-LTR text).
  std::vector<uint32_t> seen;
  for (const PositionedRun &run : layout.runs)
    seen.push_back(run.wordIndex);
  EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()));
}

TEST(KnuthPlass, NoWorseRaggednessThanGreedy) {
  FontContext &fontContext = sharedContext();
  const char8_t *tale =
      u8"It was the best of times, it was the worst of times, it was the age "
      "of wisdom, it was the age of foolishness, it was the epoch of belief, "
      "it was the epoch of incredulity, it was the season of Light, it was "
      "the season of Darkness, it was the spring of hope, it was the winter "
      "of despair.";
  const float measure = 320;

  Paragraph paragraph = makeParagraph(tale);
  BlockFlow greedyFlow(SkRect::MakeWH(measure, 2000));
  ParagraphLayout greedyLayout =
      layoutParagraph(fontContext, paragraph, greedyFlow); // ragged-right

  BlockFlow knuthPlassFlow(SkRect::MakeWH(measure, 2000));
  ParagraphLayoutOptions knuthPlassOptions;
  knuthPlassOptions.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  ParagraphLayout knuthPlassLayout = layoutParagraph(
      fontContext, paragraph, knuthPlassFlow, knuthPlassOptions);

  EXPECT_FALSE(knuthPlassLayout.overflowed());
  EXPECT_LE(raggedness(paragraph, knuthPlassLayout, measure),
            raggedness(paragraph, greedyLayout, measure) * 1.05f);
}

TEST(KnuthPlass, JustifiedCjkParagraph) {
  FontContext &fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(
      u8"吾輩は猫である。名前はまだ無い。どこで生れたかとんと見当がつかぬ。"
      "何でも薄暗いじめじめした所でニャーニャー泣いていた事だけは記憶している"
      "。");
  BlockFlow flow(SkRect::MakeWH(280, 600));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);

  EXPECT_FALSE(layout.overflowed());
  EXPECT_GT(layout.lineCount, 2);
  for (const PositionedRun &run : layout.runs)
    EXPECT_LE(runEnd(paragraph, run), 280.0f + 3.0f);
}
