/** @file
 * The light values a light node carries: what each factory fixes, the
 * windowed distance falloff reaching exactly zero at the range, the
 * spot's cone, equality by value and by identity, and a light held still.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/node/Light.h>

#include <cmath>
#include <glm/geometric.hpp>

using namespace sigil;
using namespace sigil::world;

namespace {

/** @p light standing at @p position, as a frame reads it. */
PlacedLight standing(const Light& light, glm::vec3 position = {0, 0, 0}) {
  return PlacedLight{held(light.description()), position, ""};
}

}  // namespace

TEST(Light, FactoriesFixTheirKind) {
  const Light sun = directionalLight(
      {0, -1, 0},
      {.color = material::Color{1, 0.9f, 0.8f, 1}, .intensity = 2.5f});
  EXPECT_EQ(sun.description().kind, LightKind::Directional);
  EXPECT_FLOAT_EQ(sun.description().intensity.value(), 2.5f);
  EXPECT_EQ(radiance(standing(sun)), glm::vec3(2.5f, 2.25f, 2.0f));

  const Light lamp = pointLight();
  EXPECT_EQ(lamp.description().kind, LightKind::Point);

  const Light spot =
      spotLight({0, -1, 0}, {.innerAngle = 10, .outerAngle = 30});
  EXPECT_EQ(spot.description().kind, LightKind::Spot);
  EXPECT_FLOAT_EQ(spot.description().outerAngle, 30);
  EXPECT_FLOAT_EQ(spot.description().innerAngle, 10);
  EXPECT_EQ(spot.description().range, lamp.description().range);

  // Values compare by value, which is what lets a scene prune.
  EXPECT_EQ(pointLight({.range = 100}).description(),
            pointLight({.range = 100}).description());
  EXPECT_FALSE(pointLight({.range = 101}).description() ==
               pointLight({.range = 100}).description());
}

TEST(Light, DistanceFallsOffOnAWindow) {
  const PlacedLight lamp = standing(pointLight({.range = 100}));
  EXPECT_FLOAT_EQ(attenuation(lamp, {0, 0, 0}), 1.0f);
  // (1 - (d/range)^2)^2 at half the range.
  EXPECT_FLOAT_EQ(attenuation(lamp, {50, 0, 0}), 0.75f * 0.75f);
  // Exactly zero at the range and beyond, rather than trailing off.
  EXPECT_FLOAT_EQ(attenuation(lamp, {100, 0, 0}), 0.0f);
  EXPECT_FLOAT_EQ(attenuation(lamp, {400, 0, 0}), 0.0f);
  // A directional light reaches everything equally.
  EXPECT_FLOAT_EQ(
      attenuation(standing(directionalLight({0, -1, 0})), {9, 9, 9}), 1.0f);
}

TEST(Light, TheSpotIsACone) {
  const PlacedLight spot =
      standing(spotLight({0, -1, 0},
                         {.range = 1000, .innerAngle = 20, .outerAngle = 45}),
               {0, 100, 0});
  // On the axis: the distance window alone.
  const float axis = attenuation(spot, {0, 0, 0});
  EXPECT_GT(axis, 0.9f);
  // Inside the inner angle keeps the whole cone term.
  EXPECT_FLOAT_EQ(
      attenuation(spot, {10, 0, 0}),
      attenuation(standing(pointLight({.range = 1000}), {0, 100, 0}),
                  {10, 0, 0}));
  // Outside the outer angle is dark.
  EXPECT_FLOAT_EQ(attenuation(spot, {200, 0, 0}), 0.0f);
  // Between the two the cone tapers.
  const float between = attenuation(spot, {60, 0, 0});
  EXPECT_GT(between, 0.0f);
  EXPECT_LT(between, axis);
}

TEST(Light, RadianceSamplesALiveColour) {
  motion::Animatable<material::Color> color =
      motion::animatable(material::Color{2, 0.5f, 0.25f, 0.4f});
  const Light sun =
      directionalLight({0, -1, 0}, {.color = color, .intensity = 2.0f});
  const PlacedLight live{sun.description(), {}, ""};
  EXPECT_EQ(radiance(live), glm::vec3(4, 1, 0.5f));
  color = material::Color{0.125f, 0.25f, 3, 0.8f};
  EXPECT_EQ(radiance(live), glm::vec3(0.25f, 0.5f, 6));
}

TEST(Light, ColourConstantsCompareByValueAndLiveColoursByIdentity) {
  const material::Color red{2, 0, 0, 1};
  const auto describe = [](motion::Animatable<material::Color> color) {
    return pointLight({.color = std::move(color)}).description();
  };
  EXPECT_EQ(describe(red), describe(red));
  EXPECT_NE(describe(red), describe(material::Color{0, 0, 2, 1}));
  auto color = motion::animatable(red);
  const LightDescription light = describe(color);
  const LightDescription copied = light;
  const LightDescription separate = describe(motion::animatable(color.value()));
  EXPECT_EQ(light, copied);
  EXPECT_NE(light, separate);
  EXPECT_NE(light, describe(color.value()));
  color = material::Color{0, 0, 2, 1};
  EXPECT_EQ(copied.color.value(), color.value());
  EXPECT_EQ(light, copied);
}

TEST(Light, AHeldLightKeepsEveryFieldAndLetsGoOfItsLiveValues) {
  motion::Animatable<material::Color> color =
      motion::animatable(material::Color{0.5f, 0.25f, 2, 0.75f});
  motion::Animatable<float> strength = motion::animatable(3.0f);
  const Light source = spotLight({0.2f, -1, 0.1f}, {.color = color,
                                                    .intensity = strength,
                                                    .range = 250,
                                                    .innerAngle = 7,
                                                    .outerAngle = 33})
                           .lightLink("props")
                           .shadowLink("walls");

  const LightDescription heldLight = held(source.description());
  // ONE NAME PER FIELD, so a field added to the light stops this case
  // compiling until it is named here and its carrying is asserted.
  const auto& [kind, direction, heldColor, intensity, range, innerAngle,
               outerAngle, lightLink, shadowLink] = heldLight;
  EXPECT_EQ(kind, LightKind::Spot);
  EXPECT_EQ(direction, glm::vec3(0.2f, -1, 0.1f));
  EXPECT_EQ(heldColor.value(), (material::Color{0.5f, 0.25f, 2, 0.75f}));
  EXPECT_FLOAT_EQ(intensity.value(), 3.0f);
  EXPECT_FLOAT_EQ(range, 250);
  EXPECT_FLOAT_EQ(innerAngle, 7);
  EXPECT_FLOAT_EQ(outerAngle, 33);
  EXPECT_EQ(lightLink, Collection{.include = {"props"}});
  EXPECT_EQ(shadowLink, Collection{.include = {"walls"}});

  // Every animatable is a constant now, and the live values it was read
  // from move on without it.
  EXPECT_NE(heldColor.constant(), nullptr);
  EXPECT_NE(intensity.constant(), nullptr);
  color = material::Color{1, 1, 1, 1};
  strength = 9.0f;
  EXPECT_FLOAT_EQ(source.description().intensity.value(), 9.0f);
  EXPECT_FLOAT_EQ(heldLight.intensity.value(), 3.0f);
  EXPECT_EQ(heldLight.color.value(), (material::Color{0.5f, 0.25f, 2, 0.75f}));
}
