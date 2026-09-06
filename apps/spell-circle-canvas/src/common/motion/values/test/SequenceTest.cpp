/** @file
 * The keyed track: keys are hit exactly, what happens between them is a
 * prop, outside is flat or wrapped, two keys at one time are a cut, and
 * a track with no keys answers a determinate number.
 */

#include <gtest/gtest.h>
#include <sigilmotion/values/Sequence.h>
#include <sigilmotion/values/Transition.h>

using namespace sigil::motion;

namespace {

/** An envelope: up over the first fifth, held, down over the last
 *  quarter — the shape a keyed track exists to say without a ticker. */
Sequence envelope() {
  return {.steps = {{0.0f, 0.0f}, {0.2f, 1.0f}, {0.75f, 1.0f}, {1.0f, 0.0f}}};
}

}  // namespace

TEST(Sequence, TheKeysAreHitExactlyAndTheLineIsStraightBetweenThem) {
  const Sequence track = envelope();
  EXPECT_FLOAT_EQ(track.at(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(track.at(0.2f), 1.0f);
  EXPECT_FLOAT_EQ(track.at(0.75f), 1.0f);
  EXPECT_FLOAT_EQ(track.at(1.0f), 0.0f);
  EXPECT_NEAR(track.at(0.1f), 0.5f, 1e-6f);
  EXPECT_NEAR(track.at(0.5f), 1.0f, 1e-6f);
  EXPECT_NEAR(track.at(0.875f), 0.5f, 1e-6f);
  EXPECT_FLOAT_EQ(track.duration(), 1.0f);
}

TEST(Sequence, OutsideTheKeysIsFlat) {
  const Sequence track = envelope();
  // A track carries no answer for what lies beyond its ends, and a flat
  // end keeps an out-of-range read visible instead of extrapolating one.
  EXPECT_FLOAT_EQ(track.at(-9.0f), 0.0f);
  EXPECT_FLOAT_EQ(track.at(9.0f), 0.0f);
}

TEST(Sequence, WhatHappensBetweenTwoKeysIsAProp) {
  Sequence track{.steps = {{0.0f, 0.0f}, {1.0f, 10.0f}, {2.0f, 0.0f}}};

  track.interpolation = Interpolation::Hold;
  EXPECT_FLOAT_EQ(track.at(0.99f), 0.0f);
  EXPECT_FLOAT_EQ(track.at(1.0f), 10.0f);
  EXPECT_FLOAT_EQ(track.at(1.99f), 10.0f);

  track.interpolation = Interpolation::Linear;
  EXPECT_NEAR(track.at(0.5f), 5.0f, 1e-5f);

  // The spline reaches the same keys and swings past them between: the
  // whole character of the curve, and the reason it is a prop rather
  // than a second type.
  track.interpolation = Interpolation::CatmullRom;
  EXPECT_NEAR(track.at(1.0f), 10.0f, 1e-5f);
  EXPECT_GT(track.at(0.9f), track.at(0.9f - 1e-3f));
  // A key arrived at with speed and left flat is where the swing shows:
  // the curve carries past the plateau and comes back to it.
  Sequence plateau{
      .steps = {{0.0f, 0.0f}, {1.0f, 10.0f}, {2.0f, 10.0f}, {3.0f, 0.0f}},
      .interpolation = Interpolation::CatmullRom};
  EXPECT_GT(plateau.at(1.5f), 10.0f);
  EXPECT_FLOAT_EQ(plateau.at(1.0f), 10.0f);
  EXPECT_FLOAT_EQ(plateau.at(2.0f), 10.0f);
}

TEST(Sequence, AKeysOwnCurveShapesTheSegmentLeavingIt) {
  Sequence eased{.steps = {{0.0f, 0.0f}, {1.0f, 100.0f}}};
  eased.steps[0].curve = ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f);
  // The CSS default curve is well past half way at the half — which a
  // straight line is not, and which is why the shape rides on the key.
  EXPECT_GT(eased.at(0.5f), 60.0f);
  EXPECT_FLOAT_EQ(eased.at(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(eased.at(1.0f), 100.0f);
  // It carries its own numbers, so a track eased by a named curve still
  // compares.
  Sequence same = eased;
  EXPECT_EQ(same, eased);
  same.steps[0].curve = ease::cubicBezier(0.4f, 0.0f, 0.2f, 1.0f);
  EXPECT_NE(same, eased);
}

TEST(Sequence, ALoopFoldsAtTheLastKeyAndSplinesAcrossTheSeam) {
  const Sequence cycle{.steps = {{0.0f, 0.0f}, {1.0f, 5.0f}, {2.0f, 0.0f}},
                       .loop = true};
  EXPECT_NEAR(cycle.at(2.5f), cycle.at(0.5f), 1e-5f);
  EXPECT_NEAR(cycle.at(-0.5f), cycle.at(1.5f), 1e-5f);
  EXPECT_NEAR(cycle.at(103.0f), cycle.at(1.0f), 1e-4f);

  // Across the seam the spline reaches for the keys either side of the
  // join rather than for the key that closes it, so the curve does not
  // flatten at exactly the place a loop exists to hide.
  Sequence smooth{
      .steps = {{0.0f, 0.0f}, {1.0f, 4.0f}, {2.0f, 1.0f}, {3.0f, 0.0f}},
      .interpolation = Interpolation::CatmullRom,
      .loop = true};
  const float justBefore = smooth.at(2.98f);
  const float justAfter = smooth.at(3.02f);
  EXPECT_NEAR(justBefore, justAfter, 0.3f);
  EXPECT_NE(justBefore, justAfter);
}

TEST(Sequence, TwoKeysAtOneTimeAreACutAndNoKeysIsADeterminateNumber) {
  const Sequence banded{
      .steps = {{0.0f, 0.0f}, {0.5f, 0.0f}, {0.5f, 1.0f}, {1.0f, 1.0f}}};
  EXPECT_FLOAT_EQ(banded.at(0.49f), 0.0f);
  EXPECT_FLOAT_EQ(banded.at(0.5f), 1.0f);

  EXPECT_FLOAT_EQ(Sequence{}.at(3.0f), 0.0f);
  EXPECT_FLOAT_EQ(Sequence{}.duration(), 0.0f);
  const Sequence one{.steps = {{4.0f, 7.0f}}};
  EXPECT_FLOAT_EQ(one.at(-100.0f), 7.0f);
  EXPECT_FLOAT_EQ(one.at(100.0f), 7.0f);
}

TEST(Sequence, ItIsAnInterpolatorAnythingCanCall) {
  const Sequence track = envelope();
  auto read = [](float t, const auto& signal) { return signal(t); };
  EXPECT_FLOAT_EQ(read(0.1f, track), track.at(0.1f));
}
