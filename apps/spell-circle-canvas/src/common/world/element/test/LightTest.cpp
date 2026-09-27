/** @file
 * The emitter values a description carries: what each factory fixes, the
 * windowed distance falloff reaching exactly zero at the range, the
 * spot's cone, and equality by value.
 */

#include <gtest/gtest.h>
#include <sigilworld/light/Light.h>

#include <cmath>
#include <glm/geometric.hpp>

using namespace sigil;
using namespace sigil::world;

TEST(Light, FactoriesFixTheirKind) {
  const material::Light s = light::sun({0, -1, 0}, {1, 0.9f, 0.8f, 1}, 2.5f);
  EXPECT_EQ(s.kind, material::LightKind::Directional);
  EXPECT_FLOAT_EQ(s.intensity.value(), 2.5f);
  EXPECT_EQ(light::radiance(s), glm::vec3(2.5f, 2.25f, 2.0f));

  const material::Light p = light::point({0, 100, 0});
  EXPECT_EQ(p.kind, material::LightKind::Point);
  EXPECT_EQ(p.position, glm::vec3(0, 100, 0));

  const material::Light c = light::spot({0, 100, 0}, {0, -1, 0}, 30, 10);
  EXPECT_EQ(c.kind, material::LightKind::Spot);
  EXPECT_FLOAT_EQ(c.outerAngle, 30);
  EXPECT_FLOAT_EQ(c.innerAngle, 10);
  EXPECT_EQ(c.range, p.range);

  // Values compare by value, which is what lets a scene prune.
  EXPECT_EQ(light::point({0, 100, 0}), p);
  EXPECT_FALSE(light::point({0, 101, 0}) == p);
}

TEST(Light, DistanceFallsOffOnAWindow) {
  const material::Light p = light::point({0, 0, 0}, {1, 1, 1, 1}, 1, 100);
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
  const material::Light c = light::spot({0, 100, 0}, {0, -1, 0}, /*outerAngle=*/45,
                                     /*innerAngle=*/20, {1, 1, 1, 1}, 1, 1000);
  // On the axis: the distance window alone.
  const float axis = light::attenuation(c, {0, 0, 0});
  EXPECT_GT(axis, 0.9f);
  // Inside the inner angle keeps the whole cone term.
  EXPECT_FLOAT_EQ(
      light::attenuation(c, {10, 0, 0}),
      light::attenuation(
          light::point(c.position, c.color, c.intensity.value(), c.range), {10, 0, 0}));
  // Outside the outer angle is dark.
  EXPECT_FLOAT_EQ(light::attenuation(c, {200, 0, 0}), 0.0f);
  // Between the two the cone tapers.
  const float between = light::attenuation(c, {60, 0, 0});
  EXPECT_GT(between, 0.0f);
  EXPECT_LT(between, axis);
}

TEST(Light, AnglesReadAsAWorldDirection) {
  // Straight down is overhead, exactly.
  const material::Light overhead = light::sun({0, -1, 0});
  EXPECT_FLOAT_EQ(overhead.elevation.value(), 90.0f);
  EXPECT_EQ(light::travel(overhead), glm::vec3(0, -1, 0));
  // Material's stock key light, placed in a set: from the bearing 120
  // degrees counter-clockwise from +x seen from above, 45 degrees up.
  const glm::vec3 key = light::travel(material::studio());
  EXPECT_NEAR(key.y, -std::sqrt(0.5f), 1e-6f);
  EXPECT_NEAR(key.x, std::sqrt(0.5f) * 0.5f, 1e-6f);
  EXPECT_NEAR(key.z, std::sqrt(0.5f) * std::sqrt(0.75f), 1e-6f);
  // Aiming and reading back keep the direction.
  material::Light turned;
  light::aim(turned, {-0.4f, -0.8f, -0.4f});
  const glm::vec3 back = light::travel(turned);
  const glm::vec3 expected = glm::normalize(glm::vec3(-0.4f, -0.8f, -0.4f));
  EXPECT_NEAR(back.x, expected.x, 1e-6f);
  EXPECT_NEAR(back.y, expected.y, 1e-6f);
  EXPECT_NEAR(back.z, expected.z, 1e-6f);
}
