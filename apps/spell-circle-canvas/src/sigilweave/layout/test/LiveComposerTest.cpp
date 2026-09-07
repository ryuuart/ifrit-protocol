/** @file
 * The live composer: the Knuth-Plass breaker asked to answer inside a
 * frame's budget. What a measure already seen costs the second time, what
 * a layout that asks for nothing decides, that a live answer is the
 * settled one, that only the measure moves a break, that changed content
 * is a miss rather than a stale answer, and what a budget too short leaves
 * behind.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "support/LayoutSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

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
std::vector<uint32_t> lastWordPerLine(const ParagraphLayout& layout) {
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
  return makePooledText(
      std::array<const char8_t*, 8>{u8"measure", u8"and", u8"break", u8"the",
                                    u8"story", u8"across", u8"frames",
                                    u8"again"},
      200, 5);
}

}  // namespace

/// The composer under an input that MOVES: a two-hundred-word story at
/// sixteen points in a flow deep enough to hold every line of it.
class LiveComposer : public ::testing::Test {
 protected:
  void SetUp() override { m_story.appendText(storyText(), basicStyle(16.0f)); }

  ParagraphLayout compose(const ParagraphLayoutOptions& options) {
    return layoutParagraph(sigil::test::fonts(), m_story, m_tall, options);
  }

  Paragraph m_story;
  BlockFlow m_tall{SkRect::MakeWH(300, 4000)};
};

TEST_F(LiveComposer, AMeasureAlreadySeenIsNotDecidedAgain) {
  const ParagraphLayout first = compose(liveComposer());
  const ParagraphLayout second = compose(liveComposer());
  EXPECT_EQ(first.reusedBlocks, 0);
  EXPECT_EQ(second.reusedBlocks, 1);
  EXPECT_EQ(placement(first), placement(second));
}

TEST_F(LiveComposer, ALayoutThatSaysNothingDecidesEveryBreakItself) {
  ParagraphLayoutOptions settled;
  settled.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  compose(settled);
  EXPECT_EQ(compose(settled).reusedBlocks, 0);
}

TEST_F(LiveComposer, TheSameBreaksAsTheSettledComposerAtTheSameMeasure) {
  FontContext& fonts = sigil::test::fonts();
  // A second story of its own: a live fill writes what it decided onto the
  // paragraph it read, so the settled answer has to come off a fresh one.
  Paragraph settled = makeParagraph(storyText(), 16.0f);
  BlockFlow flow(SkRect::MakeWH(300, 4000));
  ParagraphLayoutOptions settledOptions;
  settledOptions.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  EXPECT_EQ(placement(compose(liveComposer())),
            placement(layoutParagraph(fonts, settled, flow, settledOptions)));
}

TEST_F(LiveComposer, AFrameThatChangesOnlyInDepthNeverMovesALineBreak) {
  const ParagraphLayoutOptions options = liveComposer();
  const std::vector<uint32_t> whole = lastWordPerLine(compose(options));
  BlockFlow shallow(SkRect::MakeWH(300, 200));
  const ParagraphLayout cut =
      layoutParagraph(sigil::test::fonts(), m_story, shallow, options);
  const std::vector<uint32_t> held = lastWordPerLine(cut);
  ASSERT_FALSE(held.empty());
  ASSERT_LT(held.size(), whole.size());
  EXPECT_TRUE(std::equal(held.begin(), held.end(), whole.begin()));
  EXPECT_TRUE(cut.overflowed());
}

TEST_F(LiveComposer, AChangeOfContentIsAMissAndNotAStaleAnswer) {
  const ParagraphLayoutOptions options = liveComposer();
  compose(options);
  m_story.replaceText(0, 7, u8"lengthened measure of a word");
  const ParagraphLayout after = compose(options);
  EXPECT_EQ(after.reusedBlocks, 0);
  EXPECT_FALSE(after.runs.empty());
}

TEST_F(LiveComposer, ABudgetTooShortLeavesTheBlockToTheGreedyBreaker) {
  FontContext& fonts = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(
      makePooledText(
          std::array<const char8_t*, 4>{u8"budget", u8"is", u8"a", u8"degrade"},
          4000, 3),
      16.0f);
  BlockFlow flow(SkRect::MakeWH(300, 200000));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  // The shortest budget the option can carry, against a text no composer
  // could break inside it.
  options.knuthPlass.budgetMicroseconds = 1.0f;
  const ParagraphLayout layout =
      layoutParagraph(fonts, paragraph, flow, options);
  EXPECT_EQ(layout.degradedBlocks, 1);
  // The frame is set, and set completely: a degrade is a cheaper answer,
  // never a missing one.
  EXPECT_FALSE(layout.runs.empty());
  EXPECT_FALSE(layout.overflowed());
}
