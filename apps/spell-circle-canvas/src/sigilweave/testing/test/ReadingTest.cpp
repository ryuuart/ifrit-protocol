/** @file
 * A laid passage read back as values: a line's box and measure, the
 * natural width, ratio and badness the breaker chose a justified line at,
 * the gaps it spent them in, the glyphs where they rest, and the
 * hyphenation points beside the ones a line took. Every case sets the
 * instrument
 * face, where a letter is 0.6 em, the space 0.3 em and the hyphen 0.4
 * em, so every number below is arithmetic on those.
 */

#include <gtest/gtest.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/testing/Passage.h>
#include <sigilweave/testing/Reading.h>

#include <cmath>
#include <utility>

#include "support/Paragraphs.h"

using namespace sigil::weave;
using namespace sigil::weave::test;
namespace weave = sigil::weave;

namespace {

/// At 16 px in the instrument face.
constexpr float kLetter = 9.6f;
constexpr float kSpace = 4.8f;
constexpr float kHyphen = 6.4f;

/// Laid by the optimizing breaker, the one that scores its lines.
weave::testing::Passage laidInBlock(std::u8string_view text, float measure,
                                    ParagraphLayoutOptions options = {}) {
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  BlockFlow flow(sigil::geometry::path::Rect::of({0, 0}, {measure, 400}));
  return weave::testing::lay(sigil::test::fonts(), makeParagraph(text), flow,
                             std::move(options));
}

}  // namespace

TEST(WeaveReading, ALineReadsItsBoxMeasureAndNaturalWidth) {
  const weave::testing::Reading reading =
      weave::testing::read(laidInBlock(u8"aa bb cc", 1000));
  ASSERT_EQ(reading.lines.size(), 1u);
  const weave::testing::LineReading& line = reading.lines.front();
  EXPECT_FLOAT_EQ(line.measure, 1000.0f);
  ASSERT_EQ(line.scores.size(), 1u);
  const LineScore& score = line.scores.front();
  EXPECT_NEAR(score.natural, 6 * kLetter + 2 * kSpace, 1e-3f);
  EXPECT_NEAR(line.extent, score.natural, 1e-3f);
  ASSERT_EQ(line.gaps.size(), 2u);
  EXPECT_NEAR(line.gaps[0], kSpace, 1e-3f);
  EXPECT_NEAR(line.box.width(), score.natural, 1e-3f);
  // An ascent of 0.8 em over a descent of 0.2.
  EXPECT_NEAR(line.box.height(), 16.0f, 1e-3f);
  EXPECT_EQ(line.textBegin, 0u);
  EXPECT_EQ(line.textEnd, 8u);
  EXPECT_EQ(score.badness, 0.0f) << "a block's last line that fits";
}

TEST(WeaveReading, AGreedyLineReadsNoScore) {
  // The greedy breaker weighs no break, so there is nothing to read back.
  BlockFlow flow(sigil::geometry::path::Rect::of({0, 0}, {1000, 400}));
  const weave::testing::Reading reading =
      weave::testing::read(weave::testing::lay(sigil::test::fonts(),
                                               makeParagraph(u8"aa bb"), flow));
  ASSERT_EQ(reading.lines.size(), 1u);
  EXPECT_TRUE(reading.lines.front().scores.empty());
  EXPECT_NEAR(reading.lines.front().extent, 4 * kLetter + kSpace, 1e-3f);
}

TEST(WeaveReading, AJustifiedLineReadsTheRatioItsGapsOpenedBy) {
  // "aa bb cc" is 67.2 px and the measure 70, so the line's two gaps open
  // by 2.8 px between them out of the 4.8 their stretch allows.
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  const weave::testing::Reading reading =
      weave::testing::read(laidInBlock(u8"aa bb cc dd", 70, options));
  ASSERT_EQ(reading.lines.size(), 2u);
  const weave::testing::LineReading& first = reading.lines.front();
  const float slack = 70.0f - (6 * kLetter + 2 * kSpace);
  const float ratio = slack / (2 * kSpace * 0.5f);
  ASSERT_EQ(first.scores.size(), 1u);
  ASSERT_TRUE(first.scores.front().adjustmentRatio);
  EXPECT_NEAR(*first.scores.front().adjustmentRatio, ratio, 1e-3f);
  EXPECT_NEAR(first.scores.front().badness, 100.0f * ratio * ratio * ratio,
              1e-2f);
  ASSERT_EQ(first.gaps.size(), 2u);
  EXPECT_NEAR(first.gaps[0], kSpace + slack / 2, 1e-2f);
  EXPECT_NEAR(first.gaps[1], kSpace + slack / 2, 1e-2f);
  EXPECT_NEAR(first.extent, 70.0f, 1e-2f);
  EXPECT_TRUE(first.fit.plain()) << "the gaps alone took the slack";
  ASSERT_EQ(reading.lines.back().scores.size(), 1u);
  EXPECT_EQ(reading.lines.back().scores.front().badness, 0.0f);
}

TEST(WeaveReading, ASoftHyphenIsAPointAndTheLineThatBreaksThereTakesIt) {
  // The whole word is 144 px; either half with its hyphen fits in 110.
  const weave::testing::Passage passage =
      laidInBlock(u8"an extra­ordinarily narrow measure", 110);
  const weave::testing::Reading reading = weave::testing::read(passage);
  // "an " is three units and "extra" with its soft hyphen six more.
  ASSERT_EQ(reading.hyphenationPoints.size(), 1u);
  EXPECT_EQ(reading.hyphenationPoints.front(), 9u);
  EXPECT_EQ(reading.hyphensTaken, reading.hyphenationPoints);

  int hyphenRuns = 0;
  for (const weave::testing::RunReading& run : reading.runs)
    hyphenRuns += run.hyphen ? 1 : 0;
  EXPECT_EQ(hyphenRuns, 1);
  int brokenLines = 0;
  for (const weave::testing::LineReading& line : reading.lines) {
    if (!line.endsInHyphen) continue;
    ++brokenLines;
    ASSERT_EQ(line.scores.size(), 1u);
    EXPECT_NEAR(line.scores.front().natural,
                5 * kLetter + kSpace + 2 * kLetter + kHyphen, 1e-3f)
        << "the line holding \"an extra-\" counts its hyphen";
  }
  EXPECT_EQ(brokenLines, 1);
}

TEST(WeaveReading, GlyphsRestWhereTheLineSetThem) {
  const weave::testing::Passage passage = laidInBlock(u8"aa bb", 1000);
  const weave::testing::Reading reading = weave::testing::read(passage);
  ASSERT_EQ(reading.glyphs.size(), 4u);
  ASSERT_EQ(reading.runs.size(), 2u);
  EXPECT_EQ(reading.glyphs[0].rest, reading.runs[0].origin);
  EXPECT_NEAR(reading.glyphs[1].rest.x - reading.glyphs[0].rest.x, kLetter,
              1e-3f);
  EXPECT_EQ(reading.glyphs[2].rest, reading.runs[1].origin);
  EXPECT_EQ(reading.glyphs[2].textIndex, 3u);
  EXPECT_EQ(reading.glyphs[3].wordIndex, 1u);
  for (const weave::testing::GlyphPlacement& glyph : reading.glyphs)
    EXPECT_FALSE(glyph.transformed);
}

TEST(WeaveReading, ATurnedLineReadsNoGaps) {
  LineSetFlow flow;
  const float diagonal = std::sqrt(0.5f);
  flow.lines().push_back({LineInterval{{10, 10}, {diagonal, diagonal}, 400}});
  const weave::testing::Passage passage =
      weave::testing::lay(sigil::test::fonts(), makeParagraph(u8"aa bb"), flow);
  const weave::testing::Reading reading = weave::testing::read(passage);
  ASSERT_EQ(reading.lines.size(), 1u);
  EXPECT_TRUE(reading.lines.front().gaps.empty());
  ASSERT_FALSE(reading.glyphs.empty());
  EXPECT_TRUE(reading.glyphs.front().transformed);
}

TEST(WeaveReading, ReadingTwiceAnswersTheSameValue) {
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  const weave::testing::Passage passage =
      laidInBlock(u8"an extra­ordinarily narrow measure", 110, options);
  EXPECT_EQ(weave::testing::read(passage), weave::testing::read(passage));
}
