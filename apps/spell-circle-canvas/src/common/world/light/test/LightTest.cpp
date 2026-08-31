/** @file
 * world_light_test — the emitter values: what each factory fixes, the
 * windowed distance falloff reaching exactly zero at the range, the
 * spot's cone, and equality by value.
 */

#include <gtest/gtest.h>
#include <sigilworld/light/Light.h>

using namespace sigil::world;

TEST(Light, FactoriesFixTheirKind) {
  const light::Light s = light::sun({0, -1, 0}, {1, 0.9f, 0.8f, 1}, 2.5f);
  EXPECT_EQ(s.kind, light::Kind::Sun);
  EXPECT_FLOAT_EQ(s.intensity, 2.5f);
  EXPECT_EQ(light::radiance(s), glm::vec3(2.5f, 2.25f, 2.0f));

  const light::Light p = light::point({0, 100, 0});
  EXPECT_EQ(p.kind, light::Kind::Point);
  EXPECT_EQ(p.position, glm::vec3(0, 100, 0));

  const light::Light c = light::spot({0, 100, 0}, {0, -1, 0}, 30, 10);
  EXPECT_EQ(c.kind, light::Kind::Spot);
  EXPECT_FLOAT_EQ(c.outerDeg, 30);
  EXPECT_FLOAT_EQ(c.innerDeg, 10);
  EXPECT_EQ(c.range, p.range);

  // Values compare by value, which is what lets a scene prune.
  EXPECT_EQ(light::point({0, 100, 0}), p);
  EXPECT_FALSE(light::point({0, 101, 0}) == p);
}

TEST(Light, DistanceFallsOffOnAWindow) {
  const light::Light p = light::point({0, 0, 0}, {1, 1, 1, 1}, 1, 100);
  EXPECT_FLOAT_EQ(light::attenuation(p, {0, 0, 0}), 1.0f);
  // (1 - (d/range)^2)^2 at half the range.
  EXPECT_FLOAT_EQ(light::attenuation(p, {50, 0, 0}), 0.75f * 0.75f);
  // Exactly zero at the range and beyond, rather than trailing off.
  EXPECT_FLOAT_EQ(light::attenuation(p, {100, 0, 0}), 0.0f);
  EXPECT_FLOAT_EQ(light::attenuation(p, {400, 0, 0}), 0.0f);
  // A sun reaches everything equally.
  EXPECT_FLOAT_EQ(light::attenuation(light::sun({0, -1, 0}), {9, 9, 9}), 1.0f);
}

TEST(Light, TheSpotIsACone) {
  const light::Light c = light::spot({0, 100, 0}, {0, -1, 0}, /*outerDeg=*/45,
                                     /*innerDeg=*/20, {1, 1, 1, 1}, 1, 1000);
  // On the axis: the distance window alone.
  const float axis = light::attenuation(c, {0, 0, 0});
  EXPECT_GT(axis, 0.9f);
  // Inside the inner angle keeps the whole cone term.
  EXPECT_FLOAT_EQ(
      light::attenuation(c, {10, 0, 0}),
      light::attenuation(
          light::point(c.position, c.color, c.intensity, c.range), {10, 0, 0}));
  // Outside the outer angle is dark.
  EXPECT_FLOAT_EQ(light::attenuation(c, {200, 0, 0}), 0.0f);
  // Between the two the cone tapers.
  const float between = light::attenuation(c, {60, 0, 0});
  EXPECT_GT(between, 0.0f);
  EXPECT_LT(between, axis);
}
