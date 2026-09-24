/** @file
 * The clock's policy: which frames move it — the host's own under the
 * wall, a caller's stated steps under Advance, none under Pause, the
 * wall's unless something is arriving under PauseWhileLoading — and the
 * budget that runs out once, on the frame that reaches it. Every wall
 * reading is a number the test hands in.
 */

#include <gtest/gtest.h>
#include <sigilmotion/clock/ClockPolicy.h>

using sigil::motion::ClockPolicy;
using sigil::motion::PolicyClock;

TEST(PolicyClock, TheWallMovesAFrameByTheTimeThatPassedAndIgnoresAStep) {
  PolicyClock clock;
  EXPECT_TRUE(clock.wall());
  EXPECT_EQ(clock.frame(10.0), 0.0);
  EXPECT_NEAR(clock.frame(10.1), 0.1, 1e-9);
  EXPECT_EQ(clock.step(1.0), 0.0);
  EXPECT_NEAR(clock.elapsed(), 0.1, 1e-9);
  EXPECT_EQ(clock.frames(), 3u);
}

TEST(PolicyClock, UnderAdvanceOnlyAStepMovesItAndByExactlyTheStep) {
  PolicyClock clock;
  clock.setPolicy(ClockPolicy::Advance);
  EXPECT_FALSE(clock.wall());
  clock.frame(0.0);
  EXPECT_EQ(clock.frame(5.0), 0.0);
  // No time scale and no stall clamp: the caller chose the step.
  clock.setTimeScale(0.5);
  EXPECT_EQ(clock.step(0.5), 0.5);
  EXPECT_EQ(clock.elapsed(), 0.5);
}

TEST(PolicyClock, PauseMovesNeitherAFrameNorAStep) {
  PolicyClock clock;
  clock.setPolicy(ClockPolicy::Pause);
  clock.frame(0.0);
  EXPECT_EQ(clock.frame(1.0), 0.0);
  EXPECT_EQ(clock.step(1.0), 0.0);
  EXPECT_TRUE(clock.still());
  EXPECT_EQ(clock.elapsed(), 0.0);
}

TEST(PolicyClock, PauseWhileLoadingIsTheWallHeldWhileSomethingArrives) {
  PolicyClock clock;
  clock.setPolicy(ClockPolicy::PauseWhileLoading);
  clock.frame(0.0);
  EXPECT_NEAR(clock.frame(0.1), 0.1, 1e-9);
  EXPECT_EQ(clock.frame(0.2, true), 0.0);
  EXPECT_TRUE(clock.still());
  // The stretch it stood still for is not caught up afterwards.
  EXPECT_NEAR(clock.frame(0.3), 0.1, 1e-9);
  EXPECT_FALSE(clock.still());
  EXPECT_NEAR(clock.elapsed(), 0.2, 1e-9);
}

TEST(PolicyClock, AReturnToTheWallMeasuresFromItsLastReadingNotItsFirst) {
  PolicyClock clock;
  clock.frame(0.0);
  clock.setPolicy(ClockPolicy::Advance);
  clock.frame(3.0);
  clock.setPolicy(ClockPolicy::Wall);
  EXPECT_NEAR(clock.frame(3.05), 0.05, 1e-9);
}

TEST(PolicyClock, AHoldStopsFramesAndStepsAndKeepsThePolicy) {
  PolicyClock clock;
  clock.setPolicy(ClockPolicy::Advance);
  clock.setHeld(true);
  EXPECT_EQ(clock.step(0.25), 0.0);
  EXPECT_EQ(clock.policy(), ClockPolicy::Advance);
  clock.setHeld(false);
  EXPECT_EQ(clock.step(0.25), 0.25);
}

TEST(PolicyClock, ABudgetRunsOutOnceOnTheFrameThatReachesIt) {
  PolicyClock clock;
  clock.setPolicy(ClockPolicy::Advance, 1.0);
  int expiries = 0;
  for (int frame = 0; frame < 60; ++frame) {
    clock.step(1.0 / 60.0);
    if (clock.budgetExpired()) ++expiries;
    if (frame < 59) EXPECT_FALSE(clock.budgetExpired()) << frame;
  }
  EXPECT_EQ(expiries, 1);
  EXPECT_FALSE(clock.budgetRemaining());
  clock.step(1.0 / 60.0);
  EXPECT_FALSE(clock.budgetExpired());
}

TEST(PolicyClock, ABudgetUnderPauseNeverRunsOut) {
  PolicyClock clock;
  clock.setPolicy(ClockPolicy::Pause, 0.5);
  for (int frame = 0; frame < 10; ++frame) clock.frame(frame * 1.0);
  EXPECT_FALSE(clock.budgetExpired());
  EXPECT_EQ(clock.budgetRemaining(), 0.5);
}

TEST(PolicyClock, ARestartCountsFromZeroAndKeepsTheBudgetLeft) {
  PolicyClock clock;
  clock.setPolicy(ClockPolicy::Advance, 2.0);
  clock.step(0.5);
  clock.restart();
  EXPECT_EQ(clock.elapsed(), 0.0);
  EXPECT_EQ(clock.frames(), 0u);
  EXPECT_EQ(clock.budgetRemaining(), 1.5);
}

TEST(PolicyClock, ANewPolicyReplacesTheBudgetAndANegativeOneIsNone) {
  PolicyClock clock;
  clock.setPolicy(ClockPolicy::Advance, 1.0);
  clock.setPolicy(ClockPolicy::Advance, -1.0);
  EXPECT_FALSE(clock.budgetRemaining());
}
