/** @file
 * The schedule: the five orderings and their determinism, the even
 * ladder and the amount-mode division, a cue table and what a short one
 * does, the nested cascade's compounded beat, the looping fold, and the
 * beat a host reads back.
 */

#include <gtest/gtest.h>
#include <sigilmotion/schedule/Schedule.h>

#include <vector>

using namespace sigil::motion;

namespace {

std::vector<float> orderOf(Spread::From from, uint32_t count,
                           uint32_t seed = 0) {
  std::vector<float> out;
  cascadeOrder(from, count, seed, out);
  return out;
}

}  // namespace

TEST(Order, TheFiveShapes) {
  EXPECT_EQ(orderOf(Spread::From::Start, 4), (std::vector<float>{0, 1, 2, 3}));
  EXPECT_EQ(orderOf(Spread::From::End, 4), (std::vector<float>{3, 2, 1, 0}));
  EXPECT_EQ(orderOf(Spread::From::Center, 4), (std::vector<float>{3, 1, 1, 3}));
  EXPECT_EQ(orderOf(Spread::From::Edges, 4), (std::vector<float>{0, 2, 2, 0}));
}

TEST(Order, OneUnitNeverSpreads) {
  // Whichever end it claims to start from, the single member opens at 0.
  for (Spread::From from :
       {Spread::From::Start, Spread::From::Center, Spread::From::End,
        Spread::From::Random, Spread::From::Edges})
    EXPECT_EQ(orderOf(from, 1), (std::vector<float>{0}));
}

TEST(Order, TheScatterIsAPermutationAndIsRepeatable) {
  const std::vector<float> a = orderOf(Spread::From::Random, 8);
  EXPECT_EQ(a, orderOf(Spread::From::Random, 8));
  std::vector<float> ranks = a;
  std::sort(ranks.begin(), ranks.end());
  EXPECT_EQ(ranks, (std::vector<float>{0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST(Order, ASeedDealsAnIndependentScatter) {
  EXPECT_NE(orderOf(Spread::From::Random, 8, 1),
            orderOf(Spread::From::Random, 8));
  EXPECT_NE(orderOf(Spread::From::Random, 8, 1),
            orderOf(Spread::From::Random, 8, 2));
}

TEST(Cascade, TheEvenLadder) {
  const Spread spec{.eachMs = 100, .durationMs = 400};
  Cascade cascade;
  cascade.build(spec, 4, 0);
  EXPECT_FLOAT_EQ(cascade.startMs(0, 0), 0.0f);
  EXPECT_FLOAT_EQ(cascade.startMs(3, 0), 300.0f);
  EXPECT_FLOAT_EQ(cascade.totalMs, 700.0f);
  EXPECT_FLOAT_EQ(spec.spanMs(4), 700.0f);
}

TEST(Cascade, AmountModeKeepsTheTotalAndShrinksTheSpacing) {
  const Spread spec{.amountMs = 300, .durationMs = 400};
  // Every count past one answers the same span, because the amount IS
  // the spread.
  EXPECT_FLOAT_EQ(spec.spanMs(4), 700.0f);
  EXPECT_FLOAT_EQ(spec.spanMs(31), 700.0f);
  // …and a single unit has nothing to spread over.
  EXPECT_FLOAT_EQ(spec.spanMs(1), 400.0f);
  EXPECT_FLOAT_EQ(spec.spanMs(0), 400.0f);
}

TEST(Cascade, ACueTableReplacesTheLadder) {
  Spread spec{.durationMs = 180};
  spec.cueMs = {0, 340, 720, 1180};
  spec.eachMs = 9999;  // said nothing: the table states the delays
  Cascade cascade;
  cascade.build(spec, 4, 0);
  EXPECT_FLOAT_EQ(cascade.startMs(2, 0), 720.0f);
  EXPECT_FLOAT_EQ(cascade.totalMs, 1360.0f);
}

TEST(Cascade, AShortCueTablePilesItsTailOnTheLastTime) {
  Spread spec{.durationMs = 100};
  spec.cueMs = {0, 50};
  Cascade cascade;
  cascade.build(spec, 5, 0);
  EXPECT_FLOAT_EQ(cascade.startMs(2, 0), 50.0f);
  EXPECT_FLOAT_EQ(cascade.startMs(4, 0), 50.0f);
}

TEST(Cascade, ANestedCascadeOwnsTheBeat) {
  Spread spec{.eachMs = 200, .durationMs = 9999};
  spec.then(Spread{.eachMs = 30, .durationMs = 120});
  Cascade cascade;
  cascade.build(spec, 3, 4);
  // The beat is the inner ladder's own extent, not the outer duration.
  EXPECT_FLOAT_EQ(cascade.beatMs, 120.0f + 30.0f * 3.0f);
  // A start compounds the two levels.
  EXPECT_FLOAT_EQ(cascade.startMs(2, 3), 400.0f + 90.0f);
}

TEST(Cascade, LocalTimeClampsAtBothEnds) {
  const Spread spec{.eachMs = 100, .durationMs = 400};
  Cascade cascade;
  cascade.build(spec, 4, 0);
  EXPECT_FLOAT_EQ(cascade.localTime(0.0f, 3, 0), 0.0f);
  EXPECT_FLOAT_EQ(cascade.localTime(1.0f, 3, 0), 1.0f);
  // Unit 3 opens at 300 of 700; halfway through the master is 350.
  EXPECT_FLOAT_EQ(cascade.localTime(0.5f, 3, 0), 50.0f / 400.0f);
}

TEST(Cascade, ALoopingCascadeSpansItsPeriodAndFoldsEveryUnit) {
  const Spread spec{.eachMs = 100, .durationMs = 200, .loopMs = 400};
  Cascade cascade;
  cascade.build(spec, 4, 0);
  EXPECT_FLOAT_EQ(cascade.totalMs, 400.0f);
  EXPECT_FLOAT_EQ(spec.spanMs(4), 400.0f);
  // Master 0 and master 1 name the same instant of the cycle, so a
  // wrapping phase crosses its own seam with no jump.
  for (uint32_t i = 0; i < 4; ++i)
    EXPECT_FLOAT_EQ(cascade.localTime(0.0f, i, 0),
                    cascade.localTime(1.0f, i, 0));
  // A unit whose start is past the period lands at start mod period
  // rather than waiting: every unit is always somewhere in its cycle.
  EXPECT_GT(cascade.localTime(0.0f, 3, 0), 0.0f);
}

TEST(Cascade, TheBeatReadBackAgreesWithTheTwoAccessors) {
  const Spread spec{.eachMs = 100, .durationMs = 400};
  Cascade cascade;
  cascade.build(spec, 4, 0);
  const Beat beat = cascade.beat(0.5f, 3, 0);
  EXPECT_EQ(beat.unitIndex, 3u);
  EXPECT_FLOAT_EQ(beat.startMs, cascade.startMs(3, 0));
  EXPECT_FLOAT_EQ(beat.localT, cascade.localTime(0.5f, 3, 0));
  EXPECT_TRUE(beat.active);
  // Begun and not finished is the whole of "running": a clamped local
  // time reads 0 before the beat opens and 1 forever after it closes.
  EXPECT_FALSE(cascade.beat(0.0f, 3, 0).active);
  EXPECT_FALSE(cascade.beat(1.0f, 3, 0).active);
}

TEST(Spread, EqualityReadsEveryFieldAndDescendsIntoTheNesting) {
  Spread a{.eachMs = 100, .durationMs = 400};
  Spread b = a;
  EXPECT_TRUE(a == b);
  b.seed = 3;
  EXPECT_FALSE(a == b);
  b = a;
  a.then(Spread{.eachMs = 10});
  EXPECT_FALSE(a == b);
  b.then(Spread{.eachMs = 10});
  EXPECT_TRUE(a == b);
  b.then(Spread{.eachMs = 11});
  EXPECT_FALSE(a == b);
}
