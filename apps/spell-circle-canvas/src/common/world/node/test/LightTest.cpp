/** @file
 * The light values a light node carries: what each factory fixes, the
 * inverse-square falloff and its window reaching exactly zero at the
 * range, the spot's cone edges, the colour decoded into light, the
 * intensity that delivers a strength at a distance, equality by value and
 * by identity, and a light held still.
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
  // The colour is decoded from the display encoding it is typed in
  // before the intensity scales it: white stays white, and a 0.9 is the
  // light a 0.9 on screen stands for.
  const glm::vec3 shone = radiance(standing(sun));
  EXPECT_FLOAT_EQ(shone.r, 2.5f);
  EXPECT_NEAR(shone.g, 2.5f * 0.7874589f, 1e-3f);
  EXPECT_NEAR(shone.b, 2.5f * 0.6038273f, 1e-3f);

  const Light lamp = pointLight();
  EXPECT_EQ(lamp.description().kind, LightKind::Point);

  const Light spot =
      spotLight({0, -1, 0}, {.innerAngle = 10, .outerAngle = 30});
  EXPECT_EQ(spot.description().kind, LightKind::Spot);
  EXPECT_FLOAT_EQ(spot.description().outerAngle, 30);
  EXPECT_FLOAT_EQ(spot.description().innerAngle, 10);
  EXPECT_EQ(spot.description().range, lamp.description().range);
  // With no range stated a positioned light reaches everywhere.
  EXPECT_FLOAT_EQ(lamp.description().range, 0);

  const Light fill = ambientLight({.intensity = 0.3f});
  EXPECT_EQ(fill.description().kind, LightKind::Ambient);
  EXPECT_FLOAT_EQ(fill.description().intensity.value(), 0.3f);
  const Light sky =
      hemisphereLight({.color = material::Color{0.6f, 0.7f, 1, 1},
                       .groundColor = material::Color{0.3f, 0.2f, 0.1f, 1}});
  EXPECT_EQ(sky.description().kind, LightKind::Hemisphere);
  // Its up is the node's, which the node's rotation turns.
  EXPECT_EQ(sky.description().direction, glm::vec3(0, 1, 0));
  EXPECT_EQ(sky.description().groundColor.value(),
            (material::Color{0.3f, 0.2f, 0.1f, 1}));

  // Values compare by value, which is what lets a scene prune.
  EXPECT_EQ(pointLight({.range = 100}).description(),
            pointLight({.range = 100}).description());
  EXPECT_FALSE(pointLight({.range = 101}).description() ==
               pointLight({.range = 100}).description());
}

TEST(Light, DistanceFallsOffWithTheInverseSquare) {
  const PlacedLight lamp = standing(pointLight());
  // One over the distance squared: a quarter as much at twice the
  // distance, a hundredth at ten times it.
  EXPECT_FLOAT_EQ(attenuation(lamp, {1, 0, 0}), 1.0f);
  EXPECT_FLOAT_EQ(attenuation(lamp, {2, 0, 0}), 0.25f);
  EXPECT_FLOAT_EQ(attenuation(lamp, {0, 0, 10}), 0.01f);
  EXPECT_NEAR(attenuation(lamp, {300, 0, 0}) / attenuation(lamp, {600, 0, 0}),
              4.0f, 1e-4f);
  // A directional, ambient or hemisphere light reaches everything
  // equally.
  EXPECT_FLOAT_EQ(
      attenuation(standing(directionalLight({0, -1, 0})), {9, 9, 9}), 1.0f);
  EXPECT_FLOAT_EQ(attenuation(standing(ambientLight()), {9, 9, 9}), 1.0f);
  EXPECT_FLOAT_EQ(attenuation(standing(hemisphereLight()), {9, 9, 9}), 1.0f);
}

TEST(Light, ARangeBringsTheInverseSquareSmoothlyToZero) {
  const PlacedLight lamp = standing(pointLight({.range = 100}));
  // The window (1 - (d/range)^4)^2 over the inverse square: near the
  // light it is the inverse square to within a hair.
  EXPECT_NEAR(attenuation(lamp, {1, 0, 0}), 1.0f, 1e-6f);
  const float window = 1.0f - 0.5f * 0.5f * 0.5f * 0.5f;
  EXPECT_FLOAT_EQ(attenuation(lamp, {50, 0, 0}),
                  window * window / (50.0f * 50.0f));
  // Exactly zero at the range and beyond, rather than trailing off.
  EXPECT_FLOAT_EQ(attenuation(lamp, {100, 0, 0}), 0.0f);
  EXPECT_FLOAT_EQ(attenuation(lamp, {400, 0, 0}), 0.0f);
}

TEST(Light, AnIntensityAtADistanceDeliversItsStrengthThere) {
  const float intensity = intensityAt(2.5f, 600.0f);
  EXPECT_FLOAT_EQ(intensity, 2.5f * 600.0f * 600.0f);
  const PlacedLight lamp = standing(pointLight({.intensity = intensity}));
  EXPECT_NEAR(intensity * attenuation(lamp, {0, 600, 0}), 2.5f, 1e-4f);
  // A moving strength is followed, scaled the same way.
  motion::Animatable<float> strength = motion::animatable(1.0f);
  const motion::Animatable<float> followed = intensityAt(strength, 10.0f);
  EXPECT_FLOAT_EQ(followed.value(), 100.0f);
  strength = 3.0f;
  EXPECT_FLOAT_EQ(followed.value(), 300.0f);
}

TEST(Light, TheSpotIsACone) {
  const PlacedLight spot =
      standing(spotLight({0, -1, 0},
                         {.range = 1000, .innerAngle = 20, .outerAngle = 45}),
               {0, 100, 0});
  const PlacedLight lamp = standing(pointLight({.range = 1000}), {0, 100, 0});
  // On the axis and inside the inner angle: the distance falloff alone.
  EXPECT_FLOAT_EQ(attenuation(spot, {0, 0, 0}), attenuation(lamp, {0, 0, 0}));
  EXPECT_FLOAT_EQ(attenuation(spot, {10, 0, 0}), attenuation(lamp, {10, 0, 0}));
  // At the inner edge the cone still lets everything through, and at the
  // outer edge nothing.
  const float inner = 100.0f * std::tan(20.0f * 3.14159265f / 180.0f);
  EXPECT_NEAR(attenuation(spot, {inner * 0.999f, 0, 0}) /
                  attenuation(lamp, {inner * 0.999f, 0, 0}),
              1.0f, 1e-3f);
  EXPECT_FLOAT_EQ(attenuation(spot, {100.0f, 0, 0}), 0.0f);
  EXPECT_FLOAT_EQ(attenuation(spot, {200, 0, 0}), 0.0f);
  // Between the two the cone tapers.
  const float between = attenuation(spot, {60, 0, 0});
  EXPECT_GT(between, 0.0f);
  EXPECT_LT(between, attenuation(lamp, {60, 0, 0}));
}

TEST(Light, RadianceSamplesALiveColour) {
  motion::Animatable<material::Color> color =
      motion::animatable(material::Color{1, 0, 0, 0.4f});
  const Light sun =
      directionalLight({0, -1, 0}, {.color = color, .intensity = 2.0f});
  const PlacedLight live{sun.description(), {}, ""};
  EXPECT_EQ(radiance(live), glm::vec3(2, 0, 0));
  color = material::Color{0, 1, 0, 0.8f};
  EXPECT_EQ(radiance(live), glm::vec3(0, 2, 0));
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
  const auto& [kind, direction, heldColor, intensity, ground, range, innerAngle,
               outerAngle, lightLink, shadowLink] = heldLight;
  EXPECT_EQ(kind, LightKind::Spot);
  EXPECT_EQ(direction, glm::vec3(0.2f, -1, 0.1f));
  EXPECT_EQ(heldColor.value(), (material::Color{0.5f, 0.25f, 2, 0.75f}));
  EXPECT_FLOAT_EQ(intensity.value(), 3.0f);
  EXPECT_EQ(ground.value(), (material::Color{0, 0, 0, 1}));
  EXPECT_NE(ground.constant(), nullptr);
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
