// SigilMotion: the frame clock's pause/scale/clamp behavior, and the
// Ticker actually driving Choreograph motions to completion.

#include <sigilmotion/FrameClock.h>
#include <sigilmotion/Ticker.h>

#include <gtest/gtest.h>

using namespace sigil::motion;
namespace ch = choreograph;

TEST(FrameClockTest, FirstTickIsZeroThenDeltas) {
  FrameClock clock;
  EXPECT_EQ(clock.tick(10.0), 0.0);
  EXPECT_NEAR(clock.tick(10.016), 0.016, 1e-9);
  EXPECT_NEAR(clock.elapsed(), 0.016, 1e-9);
}

TEST(FrameClockTest, ClampsStallsAndScalesTime) {
  FrameClock clock({.maxDelta = 0.25});
  clock.tick(0.0);
  EXPECT_NEAR(clock.tick(5.0), 0.25, 1e-9); // suspended app: clamped

  clock.setTimeScale(0.5);
  EXPECT_NEAR(clock.tick(5.1), 0.05, 1e-9); // half speed
}

TEST(FrameClockTest, PauseFreezesElapsed) {
  FrameClock clock;
  clock.tick(0.0);
  clock.tick(0.1);
  clock.setPaused(true);
  EXPECT_EQ(clock.tick(0.2), 0.0);
  EXPECT_NEAR(clock.elapsed(), 0.1, 1e-9);
  clock.setPaused(false);
  // The paused span was consumed while paused — no catch-up jump.
  EXPECT_NEAR(clock.tick(0.3), 0.1, 1e-9);
}

TEST(TickerTest, DrivesMotionToCompletionAndSettles) {
  Ticker ticker;
  ch::Output<float> value = 0.0f;
  ticker.timeline().apply(&value).then<ch::RampTo>(10.0f, 1.0f);

  EXPECT_TRUE(ticker.active());
  ticker.tick(0.5);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4);
  EXPECT_TRUE(ticker.active());

  ticker.tick(0.6); // past the end
  EXPECT_NEAR(value.value(), 10.0f, 1e-4);
  EXPECT_FALSE(ticker.active()); // finished motions self-remove
}

TEST(TickerTest, SteppablesReportAndRetire) {
  Ticker ticker;
  double accumulated = 0.0;
  ticker.add([&accumulated](double dt) {
    accumulated += dt;
    return accumulated < 1.0;
  });

  EXPECT_TRUE(ticker.tick(0.4));
  EXPECT_TRUE(ticker.tick(0.4));
  EXPECT_FALSE(ticker.tick(0.4)); // crossed 1.0 → retired
  EXPECT_FALSE(ticker.active());
}
