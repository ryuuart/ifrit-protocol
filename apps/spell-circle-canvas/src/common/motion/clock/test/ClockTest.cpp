/** @file
 * The host's side of the engine: what one wall reading after another
 * means, what a stated frame means, what a hold, the speed and the stall
 * ceiling do to them; which frames each ClockPolicy lets move the clock;
 * and the budget that runs out once, on the frame that reaches it. No
 * wall clock is read anywhere in this file: every wall reading and every
 * stated frame is a number the test hands in.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/advanced/ClockPolicy.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>

#include <chrono>
#include <optional>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

/** Moves @p engine one stated frame of @p step past where it stands. */
Duration stepBy(Engine& engine, Duration step) {
  return engine.advance(engine.elapsed() + step);
}

}  // namespace

// ---------------------------------------------------------------------------
// Wall frames.

TEST(Clock, TheFirstWallReadingIsAZeroLengthFrameAndEveryOneAfterItIsADelta) {
  Engine engine;
  EXPECT_EQ(engine.advanceWall(10.0), Duration{});
  EXPECT_NEAR(engine.advanceWall(10.016).count(), 0.016, 1e-9);
  EXPECT_NEAR(engine.elapsed().count(), 0.016, 1e-9);
  EXPECT_EQ(engine.frames(), 2u);
}

TEST(Clock, AStallIsClampedToTheLongestWallFrame) {
  // A suspended app comes back to one enormous reading; the engine moves
  // by the ceiling rather than by a delta every animation would jump on.
  Engine engine({.maxWallStep = 250ms});
  engine.advanceWall(0.0);
  EXPECT_NEAR(engine.advanceWall(5.0).count(), 0.25, 1e-9);
}

TEST(Clock, TheSpeedScalesEveryWallFrameAfterItIsSet) {
  Engine engine;
  engine.advanceWall(0.0);
  EXPECT_NEAR(engine.advanceWall(0.1).count(), 0.1, 1e-9);
  engine.setSpeed(0.5);
  EXPECT_EQ(engine.speed(), 0.5);
  EXPECT_NEAR(engine.advanceWall(0.2).count(), 0.05, 1e-9);

  // …and the speed an engine is built with is the speed it starts at.
  Engine doubled({.speed = 2.0});
  doubled.advanceWall(0.0);
  EXPECT_NEAR(doubled.advanceWall(0.1).count(), 0.2, 1e-9);
}

TEST(Clock, ABackwardReadingReportsNoTimeRatherThanRewinding) {
  // Time only goes forward, so a reading behind the last one is a
  // zero-length frame — never a negative delta an animation would step
  // backwards on. The engine still adopts the new reading, so the frame
  // after it measures from there.
  Engine engine;
  engine.advanceWall(10.0);
  EXPECT_NEAR(engine.advanceWall(10.1).count(), 0.1, 1e-9);
  EXPECT_EQ(engine.advanceWall(10.05), Duration{});
  EXPECT_NEAR(engine.elapsed().count(), 0.1, 1e-9);
  EXPECT_NEAR(engine.advanceWall(10.15).count(), 0.1, 1e-9);
}

TEST(Clock, AHeldWallFrameAddsNoTimeAndBanksNone) {
  Engine engine;
  engine.advanceWall(0.0);
  engine.advanceWall(0.1);
  engine.setHeld(true);
  EXPECT_TRUE(engine.isHeld());
  EXPECT_TRUE(engine.isPaused());
  EXPECT_EQ(engine.advanceWall(0.2), Duration{});
  EXPECT_NEAR(engine.elapsed().count(), 0.1, 1e-9);
  engine.setHeld(false);
  // A held frame still takes its reading, so the held span is consumed
  // rather than banked: releasing the hold yields the delta since the last
  // frame, not a catch-up jump covering the whole hold.
  EXPECT_NEAR(engine.advanceWall(0.3).count(), 0.1, 1e-9);
}

TEST(Clock, AFrameThatMovesNothingMovesNoMotion) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  engine.animate(value, {.to = 10.0f, .duration = 1s, .ease = ease::linear});
  // Two frames inside the stall clamp, so each moves by what passed.
  engine.advanceWall(0.0);
  engine.advanceWall(0.25);
  engine.advanceWall(0.5);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);
  engine.setHeld(true);
  engine.advanceWall(0.75);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);
  // A motion running on a held engine is still declared to move.
  EXPECT_TRUE(engine.isRunning());
  engine.setHeld(false);
  engine.advanceWall(1.0);
  EXPECT_NEAR(value.value(), 7.5f, 1e-4f);
}

TEST(Clock, TheFirstFrameAnEngineTakesOnTheWallIsAZeroLengthOne) {
  // With no fixed step an engine's own frame reads the wall; the first
  // reading only starts the count.
  Engine engine;
  EXPECT_EQ(engine.advance(), Duration{});
  EXPECT_EQ(engine.frames(), 1u);
}

// ---------------------------------------------------------------------------
// Stated frames and a fixed step.

TEST(Clock, AStatedFrameMovesByExactlyTheDifferenceUnscaledAndUnclamped) {
  Engine engine({.maxWallStep = 250ms});
  EXPECT_TRUE(engine.isWall());
  // Under the wall a stated frame moves too — the caller chose it.
  EXPECT_EQ(engine.advance(Duration(0.5)), Duration(0.5));
  engine.setSpeed(0.5);
  EXPECT_EQ(stepBy(engine, 5s), Duration(5.0));
  EXPECT_EQ(engine.elapsed(), Duration(5.5));
  // Time only goes forward: a stated time behind the engine moves nothing.
  EXPECT_EQ(engine.advance(1s), Duration{});
  EXPECT_EQ(engine.elapsed(), Duration(5.5));
}

TEST(Clock, AReturnToTheWallAfterAStatedFrameStartsItsCountAgain) {
  Engine engine;
  engine.advanceWall(0.0);
  engine.advance(1s);
  // The stretch the caller was stating frames for is not caught up.
  EXPECT_EQ(engine.advanceWall(50.0), Duration{});
  EXPECT_NEAR(engine.advanceWall(50.1).count(), 0.1, 1e-9);
  EXPECT_NEAR(engine.elapsed().count(), 1.1, 1e-9);
}

TEST(Clock, AFixedStepMovesEveryFrameTheEngineTakesByExactlyThatStep) {
  Engine engine({.fixedStep = 1s / 60.0});
  for (int frame = 0; frame < 3; ++frame)
    EXPECT_EQ(engine.advance(), Duration(1.0 / 60.0));
  EXPECT_EQ(engine.frames(), 3u);
  engine.setHeld(true);
  EXPECT_EQ(engine.advance(), Duration{});
  engine.setHeld(false);
  engine.setPolicy(ClockPolicy::Pause);
  EXPECT_EQ(engine.advance(), Duration{});
}

// ---------------------------------------------------------------------------
// The policies.

TEST(Clock, UnderAdvanceOnlyAStatedFrameMovesItAndByExactlyTheStep) {
  Engine engine;
  engine.setPolicy(ClockPolicy::Advance);
  EXPECT_FALSE(engine.isWall());
  engine.advanceWall(0.0);
  EXPECT_EQ(engine.advanceWall(5.0), Duration{});
  // No speed and no stall clamp: the caller chose the step.
  engine.setSpeed(0.5);
  EXPECT_EQ(engine.advance(500ms), Duration(0.5));
  EXPECT_EQ(engine.elapsed(), Duration(0.5));
}

TEST(Clock, UnderAdvanceAFrameTheEngineTakesOnItsOwnMovesNothing) {
  // The policy's own contract: a frame the engine takes on its own moves
  // nothing, and only a stated frame does — a fixed step included, since
  // a fixed-step frame is still one the engine takes on its own.
  Engine engine({.fixedStep = 1s / 60.0});
  engine.setPolicy(ClockPolicy::Advance);
  EXPECT_EQ(engine.advance(), Duration{});
  EXPECT_EQ(engine.elapsed(), Duration{});
  EXPECT_EQ(stepBy(engine, 250ms), Duration(0.25));
}

TEST(Clock, PauseMovesNeitherAWallFrameNorAStatedOne) {
  Engine engine;
  engine.setPolicy(ClockPolicy::Pause);
  engine.advanceWall(0.0);
  EXPECT_EQ(engine.advanceWall(1.0), Duration{});
  EXPECT_EQ(engine.advance(1s), Duration{});
  EXPECT_TRUE(engine.isPaused());
  EXPECT_EQ(engine.elapsed(), Duration{});
}

TEST(Clock, PauseWhileLoadingIsTheWallHeldWhileSomethingArrives) {
  Engine engine;
  engine.setPolicy(ClockPolicy::PauseWhileLoading);
  engine.advanceWall(0.0);
  EXPECT_NEAR(engine.advanceWall(0.1).count(), 0.1, 1e-9);
  engine.setArriving(true);
  EXPECT_EQ(engine.advanceWall(0.2), Duration{});
  EXPECT_TRUE(engine.isPaused());
  // The stretch it stood still for is not caught up afterwards.
  engine.setArriving(false);
  EXPECT_NEAR(engine.advanceWall(0.3).count(), 0.1, 1e-9);
  EXPECT_FALSE(engine.isPaused());
  EXPECT_NEAR(engine.elapsed().count(), 0.2, 1e-9);
}

TEST(Clock, AReturnToTheWallMeasuresFromItsLastReadingNotItsFirst) {
  Engine engine;
  engine.advanceWall(0.0);
  engine.setPolicy(ClockPolicy::Advance);
  engine.advanceWall(3.0);
  engine.setPolicy(ClockPolicy::Wall);
  EXPECT_NEAR(engine.advanceWall(3.05).count(), 0.05, 1e-9);
}

TEST(Clock, AHoldStopsWallAndStatedFramesAndKeepsThePolicy) {
  Engine engine;
  engine.setPolicy(ClockPolicy::Advance);
  engine.setHeld(true);
  EXPECT_EQ(engine.advance(250ms), Duration{});
  EXPECT_EQ(engine.policy(), ClockPolicy::Advance);
  engine.setHeld(false);
  EXPECT_EQ(engine.advance(250ms), Duration(0.25));
}

// ---------------------------------------------------------------------------
// The budget.

TEST(Clock, ABudgetRunsOutOnceOnTheFrameThatReachesIt) {
  Engine engine;
  engine.setPolicy(ClockPolicy::Advance, 1s);
  int expiries = 0;
  for (int frame = 0; frame < 60; ++frame) {
    stepBy(engine, 1s / 60.0);
    if (engine.isBudgetExpired()) ++expiries;
    if (frame < 59) EXPECT_FALSE(engine.isBudgetExpired()) << frame;
  }
  EXPECT_EQ(expiries, 1);
  EXPECT_FALSE(engine.budgetRemaining());
  stepBy(engine, 1s / 60.0);
  EXPECT_FALSE(engine.isBudgetExpired());
}

TEST(Clock, ABudgetUnderPauseNeverRunsOut) {
  Engine engine;
  engine.setPolicy(ClockPolicy::Pause, 500ms);
  for (int frame = 0; frame < 10; ++frame) engine.advanceWall(frame * 1.0);
  EXPECT_FALSE(engine.isBudgetExpired());
  ASSERT_TRUE(engine.budgetRemaining());
  EXPECT_EQ(*engine.budgetRemaining(), Duration(0.5));
}

TEST(Clock, ARestartCountsFromZeroAndKeepsTheBudgetLeft) {
  Engine engine;
  engine.setPolicy(ClockPolicy::Advance, 2s);
  engine.advance(500ms);
  engine.restart();
  EXPECT_EQ(engine.elapsed(), Duration{});
  EXPECT_EQ(engine.frames(), 0u);
  EXPECT_EQ(engine.policy(), ClockPolicy::Advance);
  ASSERT_TRUE(engine.budgetRemaining());
  EXPECT_EQ(*engine.budgetRemaining(), Duration(1.5));
}

TEST(Clock, ANewPolicyReplacesTheBudgetAndANegativeOneIsNone) {
  Engine engine;
  engine.setPolicy(ClockPolicy::Advance, 1s);
  engine.setPolicy(ClockPolicy::Advance, -1s);
  EXPECT_FALSE(engine.budgetRemaining());
  engine.setPolicy(ClockPolicy::Advance);
  EXPECT_FALSE(engine.budgetRemaining());
}
