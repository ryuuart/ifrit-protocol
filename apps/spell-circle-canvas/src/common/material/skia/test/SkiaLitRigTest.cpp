/** @file
 * A lit surface under its rig: a normal map turns the surface toward or
 * away from the light, so the same fill lit from two sides is brightest
 * on opposite halves; a surface with no lighting is its colours; a light
 * that moves makes the pass live while the colours beneath stay one
 * lowered paint; a surface's own lighting stands over the scene's; and
 * sources, ambient shares, emission and bindings add as stated.
 */

#include "SkiaLitTestSupport.h"

TEST(SkiaLit, TheSideANormalMapTurnsTowardTheLightIsTheBrighterOne) {
  const Material surface = relief(64, 32);
  ASSERT_TRUE(skia::isLit(surface));
  const SkBitmap fromLeft = drawn(
      skia::lit(surface, studio({.direction = 180.0f, .elevation = 30.0f})), 64,
      32);
  const SkBitmap fromRight =
      drawn(skia::lit(surface, studio({.direction = 0.0f, .elevation = 30.0f})),
            64, 32);
  EXPECT_GT(brightness(fromLeft, 12, 16), brightness(fromLeft, 52, 16) + 60)
      << "lit from the left, the half facing left is the brighter";
  EXPECT_GT(brightness(fromRight, 52, 16), brightness(fromRight, 12, 16) + 60)
      << "lit from the right, the half facing right is the brighter";
  // Where the normal faces the same way under both lights the two
  // pictures swap, pixel for pixel, across the middle.
  EXPECT_NEAR(brightness(fromLeft, 12, 16), brightness(fromRight, 52, 16), 3);
}

TEST(SkiaLit, APlacedTextureNormalChannelUsesItsSamplingTransform) {
  const Texture normal = Texture(twoFacedNormals(16, 16))
                             .at({8, 0})
                             .tile(Repeat::Pad)
                             .sampling(Sampling::Nearest);
  const Material surface =
      from(Color{0.6f, 0.6f, 0.6f, 1})
          .surface({.roughness = 0.8f, .normal = image(normal)});
  const SkBitmap left = drawn(
      skia::lit(surface, studio({.direction = 180, .elevation = 30})), 32, 16);
  const SkBitmap right = drawn(
      skia::lit(surface, studio({.direction = 0, .elevation = 30})), 32, 16);
  EXPECT_GT(brightness(left, 12, 8), brightness(left, 24, 8) + 60);
  EXPECT_GT(brightness(right, 24, 8), brightness(right, 12, 8) + 60);
}

TEST(SkiaLit, WithNoLightingASurfaceIsItsColours) {
  const Material surface = relief(16, 16);
  const SkBitmap flat = drawn(skia::lit(surface, Lighting{}), 16, 16);
  const SkBitmap colours = drawn(skia::paint(surface), 16, 16);
  EXPECT_EQ(colours.getColor(4, 8), flat.getColor(4, 8));
  EXPECT_EQ(colours.getColor(12, 8), flat.getColor(12, 8));
  // An unlit surface ignores a light too.
  const Material unlit =
      from(Color{0.6f, 0.6f, 0.6f, 1}).surface({.unlit = true});
  EXPECT_FALSE(skia::isLit(unlit));
  EXPECT_EQ(drawn(skia::paint(unlit), 4, 4).getColor(2, 2),
            drawn(skia::lit(unlit, studio()), 4, 4).getColor(2, 2));
}

TEST(SkiaLit, AnEnvironmentWithNoPictureLightsNothing) {
  // An environment whose picture is missing reflects nothing and takes no
  // ambient share over, so with no light beside it the surface is its
  // colours rather than black.
  const Lighting empty{Environment{}};
  EXPECT_FALSE(empty);
  EXPECT_FALSE(empty.dependsOnPlacement());
  const Material surface = relief(16, 16);
  const SkBitmap colours = drawn(skia::paint(surface), 16, 16);
  const SkBitmap lit = drawn(skia::lit(surface, empty), 16, 16);
  EXPECT_EQ(colours.getColor(4, 8), lit.getColor(4, 8));
  EXPECT_EQ(colours.getColor(12, 8), lit.getColor(12, 8));
  // Beside a light it is the light alone.
  Lighting keyed{studio(), Environment{}};
  EXPECT_TRUE(keyed);
  EXPECT_EQ(drawn(skia::lit(surface, keyed), 16, 16).getColor(4, 8),
            drawn(skia::lit(surface, studio()), 16, 16).getColor(4, 8));
}

TEST(SkiaLit, PlacementDependsOnPositionedSourcesAndTheSceneFrame) {
  EXPECT_FALSE(Lighting(studio()).dependsOnPlacement());
  EXPECT_TRUE(
      Lighting(studio({.kind = LightKind::Point})).dependsOnPlacement());
  Lighting scene(studio());
  scene.frame = LightingFrame::Scene;
  EXPECT_TRUE(scene.dependsOnPlacement());
  EXPECT_FALSE(Lighting{}.dependsOnPlacement());
}

TEST(SkiaLit, AMovingLightMakesThePassLiveAndOnlyThePass) {
  const Material surface = relief(32, 16);
  sigil::motion::Animatable<float> sun = sigil::motion::animatable(0.0f);
  const Paint turning = skia::lit(surface, studio({.direction = sun}));
  EXPECT_TRUE(turning.isRunning());
  EXPECT_FALSE(skia::paint(surface).isRunning())
      << "the colours beneath do not move";
  EXPECT_FALSE(skia::lit(surface, studio({.direction = 90.0f})).isRunning());
  sun = 180.0f;
  const SkBitmap left = drawn(turning, 32, 16);
  sun = 0.0f;
  const SkBitmap right = drawn(turning, 32, 16);
  EXPECT_GT(brightness(left, 4, 8), brightness(left, 28, 8));
  EXPECT_GT(brightness(right, 28, 8), brightness(right, 4, 8));
}

TEST(SkiaLit, ASurfacesOwnLightingStandsOverTheScenes) {
  const Material scene = relief(8, 8);
  const Lighting sceneLight = studio({.direction = 0.0f});
  EXPECT_EQ(sceneLight, skia::lightingFor(scene, sceneLight));
  const Material own =
      from(Color{0.6f, 0.6f, 0.6f, 1})
          .surface({.lighting = Lighting(studio({.direction = 180.0f}))});
  const Lighting chosen = skia::lightingFor(own, sceneLight);
  ASSERT_EQ(chosen.lights.size(), 1u);
  EXPECT_EQ(Lighting(studio({.direction = 180.0f})), chosen);
  EXPECT_FALSE(skia::lightingFor(from(Color{1, 1, 1, 1}), sceneLight))
      << "a material that states no surface takes no light";
}

TEST(SkiaLit, AnEnvironmentAloneLightsAMetal) {
  // An environment bright above the horizon and dark below: a flat metal
  // reflects the band straight toward the viewer, which is neither the
  // black of no light nor the colour painted flat.
  SkBitmap around;
  around.allocN32Pixels(64, 32, true);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      *around.getAddr32(x, y) = y < 16 ? SkPreMultiplyARGB(255, 240, 240, 240)
                                       : SkPreMultiplyARGB(255, 20, 20, 20);
  around.setImmutable();
  const Material metal = from(Color{0.9f, 0.7f, 0.3f, 1})
                             .surface({.metallic = 1.0f, .roughness = 0.1f});
  const SkBitmap lit =
      drawn(skia::lit(metal, environment(around.asImage())), 8, 8);
  EXPECT_GT(brightness(lit, 4, 4), 30.0f);
}

TEST(SkiaLit, TwoColoredSourcesAddTheirDirectResponses) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = .8f});
  Light red = studio({.elevation = 90,
                      .color = Color{1, 0, 0, 1},
                      .intensity = .2f,
                      .ambient = 0});
  Light blue = red;
  blue.kind = LightKind::Point;
  blue.position = {4.5f, 4.5f, 20};
  blue.range = 100;
  blue.color = Color{0, 0, 1, 1};
  const SkColor4f a = sampled(skia::lit(surface, red));
  const SkColor4f b = sampled(skia::lit(surface, blue));
  const SkColor4f both =
      sampled(skia::lit(surface, Lighting(std::vector{red, blue})));
  EXPECT_GT(a.fR, .01f);
  EXPECT_GT(b.fB, .01f);
  EXPECT_NEAR(both.fR, a.fR + b.fR, .00001f);
  EXPECT_NEAR(both.fG, a.fG + b.fG, .00001f);
  EXPECT_NEAR(both.fB, a.fB + b.fB, .00001f);
  EXPECT_FLOAT_EQ(both.fA, 1);
}

TEST(SkiaLit, ReorderingMixedSourcesPreservesLightingUnderAffinePlacement) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.roughness = .7f,
                    .normal = image(twoFacedNormals(16, 8)),
                    .clearcoat = 1});
  Light key = studio({.direction = 0,
                      .elevation = 35,
                      .color = Color{1, .1f, .05f, 1},
                      .intensity = .1f,
                      .ambient = .04f});
  Light point = key;
  point.kind = LightKind::Point;
  point.position = {12, 16, 20};
  point.range = 100;
  point.color = Color{.05f, 1, .1f, 1};
  Light spot = point;
  spot.kind = LightKind::Spot;
  spot.elevation = 90;
  spot.innerAngle = 15;
  spot.outerAngle = 70;
  spot.color = Color{.1f, .05f, 1, 1};
  const Environment around =
      environment(from(Color{.1f, .2f, .3f, 1}), {.size = {1, 1}});
  FrameData frame;
  frame.world = glm::mat3{0, 1, 0, -2, .5f, 0, 20, 8, 1};
  const SkBitmap a = drawnFloat(
      skia::lit(surface, Lighting(std::vector{key, point, spot}, around)), 16,
      8, frame);
  const SkBitmap b = drawnFloat(
      skia::lit(surface, Lighting(std::vector{spot, point, key}, around)), 16,
      8, frame);
  for (int y : {1, 4, 6})
    for (int x : {1, 6, 10, 14}) {
      const SkColor4f first = a.getColor4f(x, y), reversed = b.getColor4f(x, y);
      EXPECT_NEAR(first.fR, reversed.fR, .00001f);
      EXPECT_NEAR(first.fG, reversed.fG, .00001f);
      EXPECT_NEAR(first.fB, reversed.fB, .00001f);
      EXPECT_FLOAT_EQ(first.fA, reversed.fA);
    }
}

TEST(SkiaLit, AmbientSharesAddAndAnEnvironmentAloneUsesAFullShare) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1})
          .surface(
              {.roughness = 1.0f, .occlusion = .5f, .reflectionWeight = 0});
  Light first = studio({.intensity = 0, .ambient = .1f});
  Light second = studio({.intensity = 0, .ambient = .2f});
  const Environment around =
      environment(from(Color{.5f, .4f, .3f, 1}), {.size = {1, 1}});
  const SkColor4f alone = sampled(skia::lit(surface, around));
  const SkColor4f sum =
      sampled(skia::lit(surface, Lighting(std::vector{first, second}, around)));
  EXPECT_NEAR(alone.fR, .2f * .5f * .5f, .00001f);
  EXPECT_NEAR(alone.fG, .3f * .4f * .5f, .00001f);
  EXPECT_NEAR(alone.fB, .4f * .3f * .5f, .00001f);
  EXPECT_NEAR(sum.fR, alone.fR * .3f, .00001f);
  EXPECT_NEAR(sum.fG, alone.fG * .3f, .00001f);
  EXPECT_NEAR(sum.fB, alone.fB * .3f, .00001f);
  EXPECT_EQ(sampled(skia::lit(surface, Lighting(std::vector<Light>{}))),
            sampled(skia::paint(surface)));
}

TEST(SkiaLit, ReflectionAndEmissionDoNotMultiplyWithTheSourceCount) {
  const Material surface = from(Color{.2f, .3f, .4f, 1})
                               .surface({.roughness = .6f,
                                         .emission = {.1f, .2f, .3f, 1},
                                         .emissionStrength = .5f,
                                         .clearcoat = 1});
  const Environment around =
      environment(from(Color{.1f, .2f, .3f, 1}), {.size = {1, 1}});
  const Light dark = studio({.intensity = 0, .ambient = 0});
  const SkColor4f one = sampled(skia::lit(surface, Lighting(dark, around)));
  const SkColor4f several = sampled(
      skia::lit(surface, Lighting(std::vector{dark, dark, dark}, around)));
  EXPECT_GT(one.fB, .1f);
  EXPECT_NEAR(one.fR, several.fR, .00001f);
  EXPECT_NEAR(one.fG, several.fG, .00001f);
  EXPECT_NEAR(one.fB, several.fB, .00001f);
}

TEST(SkiaLit, DirectCoatingResponsesAddWithoutAttenuatingEachOther) {
  const Material surface = from(Color{.2f, .3f, .4f, 1})
                               .surface({.roughness = 1.0f, .clearcoat = 1});
  const Light red = studio({.elevation = 90,
                            .color = Color{1, 0, 0, 1},
                            .intensity = .03f,
                            .ambient = 0});
  const Light blue = studio({.elevation = 90,
                             .color = Color{0, 0, 1, 1},
                             .intensity = .03f,
                             .ambient = 0});
  const SkColor4f a = sampled(skia::lit(surface, red));
  const SkColor4f b = sampled(skia::lit(surface, blue));
  const SkColor4f both =
      sampled(skia::lit(surface, Lighting(std::vector{red, blue})));
  EXPECT_GT(a.fR, .04f);
  EXPECT_GT(b.fB, .04f);
  EXPECT_NEAR(both.fR, a.fR + b.fR, .00001f);
  EXPECT_NEAR(both.fG, a.fG + b.fG, .00001f);
  EXPECT_NEAR(both.fB, a.fB + b.fB, .00001f);
}

TEST(SkiaLit, AnInvalidPositionedSourceDoesNotDisableOtherSources) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = .8f});
  const Light key = studio({.elevation = 90, .intensity = .2f, .ambient = 0});
  const SkColor4f expected = sampled(skia::lit(surface, key));
  for (bool invalidPosition : {false, true}) {
    Light invalid = key;
    invalid.kind = LightKind::Point;
    invalid.position = {4.5f, 4.5f, 20};
    invalid.range = 0;
    if (invalidPosition) {
      invalid.range = 100;
      invalid.position.x = std::numeric_limits<float>::quiet_NaN();
    }
    const SkColor4f actual =
        sampled(skia::lit(surface, Lighting(std::vector{invalid, key})));
    EXPECT_NEAR(actual.fR, expected.fR, .00001f);
    EXPECT_NEAR(actual.fG, expected.fG, .00001f);
    EXPECT_NEAR(actual.fB, expected.fB, .00001f);
  }
}

TEST(SkiaLit, ABindingOnALaterSourceKeepsTheLightingPassLive) {
  const Material surface = relief(16, 8);
  auto direction = sigil::motion::animatable(0.0f);
  const Light dark = studio({.intensity = 0, .ambient = 0});
  const Light moving = studio(
      {.direction = direction, .elevation = 0, .intensity = .2f, .ambient = 0});
  const Lighting lighting(std::vector{dark, moving});
  EXPECT_TRUE(lighting.isRunning());
  const Paint live = skia::lit(surface, lighting);
  EXPECT_TRUE(live.isRunning());
  EXPECT_FALSE(skia::paint(surface).isRunning());
  const SkBitmap right = drawnFloat(live, 16, 8);
  direction = 180;
  const SkBitmap left = drawnFloat(live, 16, 8);
  EXPECT_GT(right.getColor4f(14, 4).fR, right.getColor4f(2, 4).fR);
  EXPECT_GT(left.getColor4f(2, 4).fR, left.getColor4f(14, 4).fR);
}

TEST(SkiaLit, EveryDeclaredSourceContributesBeyondASmallFixedCount) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  const Light key = studio({.elevation = 90, .intensity = .001f, .ambient = 0});
  const SkColor4f one = sampled(skia::lit(surface, key));
  const SkColor4f many =
      sampled(skia::lit(surface, Lighting(std::vector<Light>(40, key))));
  EXPECT_NEAR(many.fR, one.fR * 40, .00001f);
  EXPECT_NEAR(many.fG, one.fG * 40, .00001f);
  EXPECT_NEAR(many.fB, one.fB * 40, .00001f);
}

TEST(SkiaLit, ASceneDirectionalLightReadsTheRotatedPageNormal) {
  constexpr float slope = .70710678f;
  const Material surface =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.roughness = 1.0f,
                    .normal = from(
                        Color{.5f + .5f * slope, .5f, .5f + .5f * slope, 1})});
  Lighting lighting =
      studio({.direction = 270, .elevation = 0, .intensity = 1, .ambient = 0});
  FrameData frame;
  frame.world = glm::mat3{0, 1, 0, -1, 0, 0, 0, 0, 1};
  const Paint local = skia::lit(surface, lighting);
  EXPECT_FALSE(local.usesWorldSpace());
  const SkColor4f surfaceFrame =
      drawnFloat(local, 8, 8, frame).getColor4f(4, 4);
  EXPECT_NEAR(surfaceFrame.fR, 0, .00001f);
  lighting.frame = LightingFrame::Scene;
  const Paint page = skia::lit(surface, lighting);
  EXPECT_TRUE(page.usesWorldSpace());
  const SkColor4f sceneFrame = drawnFloat(page, 8, 8, frame).getColor4f(4, 4);
  constexpr float highlight = .04f / 3.14159265f;
  EXPECT_NEAR(sceneFrame.fR, (.2f + highlight) * slope, .00001f);
  EXPECT_NEAR(sceneFrame.fG, (.3f + highlight) * slope, .00001f);
  EXPECT_NEAR(sceneFrame.fB, (.4f + highlight) * slope, .00001f);
  EXPECT_TRUE(skia::usesWorldSpace(
      from(Color{.2f, .3f, .4f, 1}).surface({.lighting = lighting})));
  Lighting empty;
  empty.frame = LightingFrame::Scene;
  EXPECT_FALSE(skia::usesWorldSpace(
      from(Color{.2f, .3f, .4f, 1}).surface({.lighting = empty})));
}

TEST(SkiaLit, ASceneEnvironmentReadsTheRotatedPageNormalWithoutDirectSources) {
  const auto surface = [](Color normal) {
    return from(Color{.2f, .3f, .4f, 1})
        .surface({.metallic = .7f,
                  .roughness = .4f,
                  .normal = from(normal),
                  .clearcoat = .5f});
  };
  const EnvironmentMap panorama =
      EnvironmentMap::baked(32, [](float u, float v) {
        return glm::vec3{.1f + .4f * u, .1f + .4f * v, .2f};
      });
  Lighting around =
      environment(panorama.texture().source(), {.intensity = .5f});
  // Exactly representable encoded slopes keep normalization and environment
  // lookup independent of colour-channel quantization before the rotation.
  const Material localNormal = surface({.75f, .5f, .75f, 1});
  const Material pageNormal = surface({.5f, .25f, .75f, 1});
  const SkColor4f expected = sampled(skia::lit(pageNormal, around));
  around.frame = LightingFrame::Scene;
  const Paint pass = skia::lit(localNormal, around);
  EXPECT_TRUE(pass.usesWorldSpace());
  FrameData frame;
  frame.world = glm::mat3{0, 1, 0, -1, 0, 0, 0, 0, 1};
  const SkColor4f actual = drawnFloat(pass, 8, 8, frame).getColor4f(4, 4);
  EXPECT_NEAR(actual.fR, expected.fR, .00001f);
  EXPECT_NEAR(actual.fG, expected.fG, .00001f);
  EXPECT_NEAR(actual.fB, expected.fB, .00001f);
  EXPECT_FLOAT_EQ(actual.fA, expected.fA);
  EXPECT_TRUE(skia::usesWorldSpace(
      from(Color{.2f, .3f, .4f, 1}).surface({.lighting = around})));
}

TEST(SkiaLit, RoughnessOneWithAnAwayFacingNormalHasNoDirectLight) {
  const Lighting key = studio({.direction = 0.0f,
                               .elevation = 0.0f,
                               .intensity = 1.0f,
                               .ambient = 0.0f});
  for (float clearcoat : {0.0f, 1.0f}) {
    const Material surface =
        from(Color{.4f, .5f, .6f, 1})
            .surface({.roughness = 1.0f,
                      .normal = from(Color{0, .5f, .5f, 1}),
                      .clearcoat = clearcoat});
    const SkColor4f away = sampled(skia::lit(surface, key));
    EXPECT_TRUE(std::isfinite(away.fR));
    EXPECT_TRUE(std::isfinite(away.fG));
    EXPECT_TRUE(std::isfinite(away.fB));
    EXPECT_FLOAT_EQ(away.fR, 0);
    EXPECT_FLOAT_EQ(away.fG, 0);
    EXPECT_FLOAT_EQ(away.fB, 0);
    EXPECT_FLOAT_EQ(away.fA, 1);
  }
}
