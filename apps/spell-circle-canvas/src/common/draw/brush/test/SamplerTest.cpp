/** @file
 * The sampler: even spacing whatever the event rate, and the first dab's
 * direction.
 */

#include <gtest/gtest.h>
#include <sigildraw/Math.h>
#include <sigildraw/brush/Sampler.h>

#include <array>
#include <cmath>
#include <vector>

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;

TEST(Sampler, CarriesSpacingAcrossUnevenInputEvents) {
  const std::vector<brush::Input> input = {
      {.position = {0, 0}, .pressure = 0.2f, .seconds = 0.0},
      {.position = {3, 0}, .pressure = 0.4f, .seconds = 0.01},
      {.position = {11, 0}, .pressure = 1.0f, .seconds = 0.02},
  };
  const std::vector<brush::Dab> sampled = brush::dabs(input, 2.0f, 0.0f);
  ASSERT_EQ(sampled.size(), 7u);
  for (size_t i = 0; i < 6; ++i)
    EXPECT_NEAR(sampled[i].position.fX, (float)i * 2.0f, 1e-5f);
  EXPECT_FLOAT_EQ(sampled.back().position.fX, 11.0f);
  EXPECT_FLOAT_EQ(sampled.front().progress, 0.0f);
  EXPECT_FLOAT_EQ(sampled.back().progress, 1.0f);
  EXPECT_FLOAT_EQ(sampled.front().direction, sampled[1].direction);
  EXPECT_GT(sampled[2].speed, 0.0f);
}

TEST(Sampler, InterpolatesAnglesAcrossTheWrap) {
  const std::array<brush::Input, 2> turning{{
      {.position = {0, 0},
       .barrelRotation = radians(170.0f),
       .seconds = 0.0,
       .tiltDirection = radians(170.0f)},
      {.position = {10, 0},
       .barrelRotation = radians(-170.0f),
       .seconds = 0.1,
       .tiltDirection = radians(-170.0f)},
  }};
  const std::vector<brush::Dab> turn = brush::dabs(turning, 5.0f, 0.0f);
  ASSERT_EQ(turn.size(), 3u);
  EXPECT_NEAR(std::abs(turn[1].barrelRotation), PI, 1e-5f);
  EXPECT_NEAR(std::abs(turn[1].tiltDirection), PI, 1e-5f);
}

TEST(Sampler, HoldsTheFirstDabUntilTheFirstMoveGivesItAHeading) {
  brush::Sampler sampler(0.0f);
  EXPECT_TRUE(sampler.begin({.position = {20, 20}, .seconds = 0.0}).empty());
  const std::vector<brush::Dab> moved =
      sampler.move({.position = {20, 60}, .seconds = 0.1}, 10.0f);
  ASSERT_EQ(moved.size(), 5u);
  EXPECT_EQ(moved.front().position, SkPoint::Make(20, 20));
  EXPECT_NEAR(moved.front().direction, HALF_PI, 1e-6f);
  EXPECT_FLOAT_EQ(moved.front().distance, 0.0f);
  EXPECT_FLOAT_EQ(moved.back().distance, 40.0f);
}

TEST(Sampler, ATapIsOneDabAtDirectionZero) {
  brush::Sampler sampler;
  EXPECT_TRUE(sampler.begin({.position = {5, 5}}).empty());
  const std::vector<brush::Dab> tap = sampler.end({.position = {5, 5}}, 2.0f);
  ASSERT_EQ(tap.size(), 1u);
  EXPECT_EQ(tap.front().position, SkPoint::Make(5, 5));
  EXPECT_FLOAT_EQ(tap.front().direction, 0.0f);
  EXPECT_FALSE(sampler.active());
}

}  // namespace
