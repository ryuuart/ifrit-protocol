/** @file
 * Knuth-Plass optimal line breaking: validity, raggedness
 * versus greedy, and CJK justification.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string>
#include <tuple>
#include <vector>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── ParagraphLayout: Knuth-Plass
// ───────────────────────────────────────────────────

namespace {

// Sum of squared leftover space across all full lines — the raggedness
// measure Knuth-Plass style breaking should not lose to greedy on.
float raggedness(const Paragraph& paragraph, const ParagraphLayout& layout,
                 float measure) {
  if (layout.lineCount <= 1) return 0;
  std::vector<float> lineEnds(static_cast<size_t>(layout.lineCount), 0.0f);
  for (const PositionedRun& run : layout.runs)
    lineEnds[static_cast<size_t>(run.lineIndex)] = std::max(
        lineEnds[static_cast<size_t>(run.lineIndex)], runEnd(paragraph, run));
  float total = 0;
  for (int line = 0; line + 1 < layout.lineCount; ++line) {
    const float slack = measure - lineEnds[static_cast<size_t>(line)];
    total += slack * slack;
  }
  return total;
}

}  // namespace

TEST(KnuthPlass, ProducesValidLines) {
  FontContext& fontContext = sharedContext();
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
  seen.reserve(layout.runs.size());
  for (const PositionedRun& run : layout.runs) seen.push_back(run.wordIndex);
  EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()));
}

TEST(KnuthPlass, NoWorseRaggednessThanGreedy) {
  FontContext& fontContext = sharedContext();
  const char8_t* tale =
      u8"It was the best of times, it was the worst of times, it was the age "
      "of wisdom, it was the age of foolishness, it was the epoch of belief, "
      "it was the epoch of incredulity, it was the season of Light, it was "
      "the season of Darkness, it was the spring of hope, it was the winter "
      "of despair.";
  const float measure = 320;

  Paragraph paragraph = makeParagraph(tale);
  BlockFlow greedyFlow(SkRect::MakeWH(measure, 2000));
  ParagraphLayout greedyLayout =
      layoutParagraph(fontContext, paragraph, greedyFlow);  // ragged-right

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
  FontContext& fontContext = sharedContext();
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
  for (const PositionedRun& run : layout.runs)
    EXPECT_LE(runEnd(paragraph, run), 280.0f + 3.0f);
}

// ── The live composer ─────────────────────────────────────────────────────

namespace {

/// Where every run landed: the word, its line, and its origin.
std::vector<std::tuple<uint32_t, int, float, float>> placement(
    const ParagraphLayout& layout) {
  std::vector<std::tuple<uint32_t, int, float, float>> placed;
  for (const PositionedRun& run : layout.runs)
    placed.emplace_back(run.wordIndex, run.lineIndex, run.origin.x(),
                        run.origin.y());
  return placed;
}

/// The word each line ends at, ascending by line.
std::vector<uint32_t> lineEnds(const ParagraphLayout& layout) {
  std::vector<uint32_t> ends;
  for (const PositionedRun& run : layout.runs) {
    if (ends.empty() || run.lineIndex != (int)ends.size() - 1)
      ends.push_back(run.wordIndex);
    else
      ends.back() = run.wordIndex;
  }
  return ends;
}

ParagraphLayoutOptions liveComposer() {
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.live = true;
  return options;
}

std::u8string storyText() {
  return makePooledText(std::array<const char8_t*, 8>{
                            u8"measure", u8"and", u8"break", u8"the", u8"story",
                            u8"across", u8"frames", u8"again"},
                        200, 5);
}

}  // namespace

TEST(LiveComposer, AMeasureAlreadySeenIsNotDecidedAgain) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(storyText(), 16.0f);
  BlockFlow flow(SkRect::MakeWH(300, 4000));
  const ParagraphLayoutOptions options = liveComposer();
  const ParagraphLayout first = layoutParagraph(fonts, paragraph, flow, options);
  const ParagraphLayout second =
      layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_EQ(first.reusedBlocks, 0);
  EXPECT_EQ(second.reusedBlocks, 1);
  EXPECT_EQ(placement(first), placement(second));
}

TEST(LiveComposer, ALayoutThatSaysNothingDecidesEveryBreakItself) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(storyText(), 16.0f);
  BlockFlow flow(SkRect::MakeWH(300, 4000));
  ParagraphLayoutOptions settled;
  settled.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  layoutParagraph(fonts, paragraph, flow, settled);
  const ParagraphLayout again = layoutParagraph(fonts, paragraph, flow, settled);
  EXPECT_EQ(again.reusedBlocks, 0);
}

TEST(LiveComposer, TheSameBreaksAsTheSettledComposerAtTheSameMeasure) {
  FontContext& fonts = sharedContext();
  Paragraph live = makeParagraph(storyText(), 16.0f);
  Paragraph settled = makeParagraph(storyText(), 16.0f);
  BlockFlow flowA(SkRect::MakeWH(300, 4000));
  BlockFlow flowB(SkRect::MakeWH(300, 4000));
  ParagraphLayoutOptions settledOptions;
  settledOptions.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  EXPECT_EQ(placement(layoutParagraph(fonts, live, flowA, liveComposer())),
            placement(layoutParagraph(fonts, settled, flowB, settledOptions)));
}

TEST(LiveComposer, AFrameThatChangesOnlyInDepthNeverMovesALineBreak) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(storyText(), 16.0f);
  const ParagraphLayoutOptions options = liveComposer();
  BlockFlow tall(SkRect::MakeWH(300, 4000));
  const std::vector<uint32_t> whole =
      lineEnds(layoutParagraph(fonts, paragraph, tall, options));
  BlockFlow shallow(SkRect::MakeWH(300, 200));
  const ParagraphLayout cut =
      layoutParagraph(fonts, paragraph, shallow, options);
  const std::vector<uint32_t> held = lineEnds(cut);
  ASSERT_FALSE(held.empty());
  ASSERT_LT(held.size(), whole.size());
  EXPECT_TRUE(std::equal(held.begin(), held.end(), whole.begin()));
  EXPECT_TRUE(cut.overflowed());
}

TEST(LiveComposer, AChangeOfContentIsAMissAndNotAStaleAnswer) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(storyText(), 16.0f);
  BlockFlow flow(SkRect::MakeWH(300, 4000));
  const ParagraphLayoutOptions options = liveComposer();
  layoutParagraph(fonts, paragraph, flow, options);
  paragraph.replaceText(0, 7, u8"lengthened measure of a word");
  const ParagraphLayout after = layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_EQ(after.reusedBlocks, 0);
  EXPECT_FALSE(after.runs.empty());
}

TEST(LiveComposer, ABudgetTooShortLeavesTheBlockToTheGreedyBreaker) {
  FontContext& fonts = sharedContext();
  Paragraph paragraph = makeParagraph(
      makePooledText(std::array<const char8_t*, 4>{u8"budget", u8"is", u8"a",
                                                   u8"degrade"},
                     4000, 3),
      16.0f);
  BlockFlow flow(SkRect::MakeWH(300, 200000));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  // A budget no composer can meet on a text this long.
  options.knuthPlass.budgetMicroseconds = 1.0f;
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_EQ(layout.degradedBlocks, 1);
  // The frame is set, and set completely: a degrade is a cheaper answer,
  // never a missing one.
  EXPECT_FALSE(layout.runs.empty());
  EXPECT_FALSE(layout.overflowed());
}
