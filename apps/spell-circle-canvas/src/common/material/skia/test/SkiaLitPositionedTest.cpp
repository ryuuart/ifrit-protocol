/** @file
 * Point and spot lights on the page: falloff over the range window, cones
 * about the source-facing axis, live angles and positions, and the
 * affine placement that carries both the light and the normals.
 */

#include "SkiaLitTestSupport.h"

TEST(SkiaLit, APointLightHighlightsFlatInteriorsAndFollowsItsPosition) {
  const Material metal = from(Color{.2f, .2f, .2f, 1})
                             .surface({.metallic = 1.0f, .roughness = .3f});
  Light light;
  light.kind = LightKind::Point;
  light.position = {16.5f, 24.5f, 24};
  light.range = 120;
  light.intensity = .1f;
  light.ambient = 0;
  const Paint leftFill = skia::lit(metal, light);
  ASSERT_FALSE(leftFill.isSolid());
  const SkBitmap left = drawnFloat(leftFill, 96, 48);
  light.position.x = 80.5f;
  const SkBitmap right = drawnFloat(skia::lit(metal, light), 96, 48);
  EXPECT_GT(left.getColor4f(16, 24).fR, .1f);
  EXPECT_GT(right.getColor4f(80, 24).fR, .1f);
  EXPECT_LT(left.getColor4f(80, 24).fR, .0001f);
  EXPECT_LT(right.getColor4f(16, 24).fR, .0001f);
}

TEST(SkiaLit, PointFalloffUsesTheSquaredRangeWindow) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  Light light;
  light.kind = LightKind::Point;
  light.position = {4.5f, 4.5f, 50};
  light.range = 100;
  light.ambient = 0;
  constexpr float window = .75f * .75f;
  constexpr float specular = .04f / 3.14159265f;
  const SkColor4f atHalfRange = sampled(skia::lit(surface, light));
  EXPECT_NEAR(atHalfRange.fR, (.2f + specular) * window, .00001f);
  EXPECT_NEAR(atHalfRange.fG, (.3f + specular) * window, .00001f);
  EXPECT_NEAR(atHalfRange.fB, (.4f + specular) * window, .00001f);
  light.position.z = 100;
  EXPECT_FLOAT_EQ(sampled(skia::lit(surface, light)).fR, 0);
  light.position.z = 200;
  EXPECT_FLOAT_EQ(sampled(skia::lit(surface, light)).fR, 0);
}

TEST(SkiaLit, PointAnglesAreIgnoredAndItsIntensityStillBinds) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  auto direction = sigil::motion::animatable(0.0f);
  auto elevation = sigil::motion::animatable(45.0f);
  Light point;
  point.kind = LightKind::Point;
  point.direction = direction;
  point.elevation = elevation;
  point.position = {4.5f, 4.5f, 50};
  point.range = 100;
  point.ambient = 0;
  const skia::LitSurface prepared(surface);
  const Paint still = prepared.under(point);
  EXPECT_FALSE(still.isRunning());
  const SkColor4f before = sampled(still);
  EXPECT_GT(before.fR, 0);
  direction = 180;
  elevation = 90;
  EXPECT_FLOAT_EQ(sampled(still).fR, before.fR);
  direction = std::numeric_limits<float>::quiet_NaN();
  elevation = std::numeric_limits<float>::infinity();
  point.innerAngle = std::numeric_limits<float>::quiet_NaN();
  point.outerAngle = std::numeric_limits<float>::infinity();
  const SkColor4f ignored = sampled(prepared.under(point));
  EXPECT_FLOAT_EQ(ignored.fR, before.fR);
  EXPECT_FLOAT_EQ(ignored.fG, before.fG);
  EXPECT_FLOAT_EQ(ignored.fB, before.fB);

  auto intensity = sigil::motion::animatable(1.0f);
  point.intensity = intensity;
  const Paint live = prepared.under(point);
  EXPECT_TRUE(live.isRunning());
  intensity = .25f;
  const SkColor4f dimmed = sampled(live);
  EXPECT_NEAR(dimmed.fR, before.fR * .25f, .00001f);
  EXPECT_NEAR(dimmed.fG, before.fG * .25f, .00001f);
  EXPECT_NEAR(dimmed.fB, before.fB * .25f, .00001f);
}

TEST(SkiaLit, DirectionalElevationRemainsLive) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  auto elevation = sigil::motion::animatable(0.0f);
  const Paint live = skia::lit(
      surface,
      studio({.elevation = elevation, .intensity = .2f, .ambient = 0}));
  EXPECT_TRUE(live.isRunning());
  const SkColor4f grazing = sampled(live);
  elevation = 90;
  const SkColor4f front = sampled(live);
  EXPECT_GT(front.fR, grazing.fR + .03f);
  EXPECT_GT(front.fG, grazing.fG + .05f);
  EXPECT_GT(front.fB, grazing.fB + .07f);
}

TEST(SkiaLit, DegeneratePointSourcesRemainFinite) {
  const Material surface = from(Color{.2f, .3f, .4f, 1})
                               .surface({.roughness = 1.0f, .clearcoat = 1});
  Light light;
  light.kind = LightKind::Point;
  light.position = {4.5f, 4.5f, 0};
  light.range = 100;
  light.ambient = 0;
  const SkColor4f coincident = sampled(skia::lit(surface, light));
  EXPECT_TRUE(std::isfinite(coincident.fR));
  EXPECT_GT(coincident.fR, .2f);
  light.position.z = -20;
  const SkColor4f behind = sampled(skia::lit(surface, light));
  EXPECT_TRUE(std::isfinite(behind.fR));
  EXPECT_TRUE(std::isfinite(behind.fG));
  EXPECT_TRUE(std::isfinite(behind.fB));
  EXPECT_FLOAT_EQ(behind.fR, 0);
  EXPECT_FLOAT_EQ(behind.fA, 1);
  for (float range : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
    light.position.z = 20;
    light.range = range;
    EXPECT_FLOAT_EQ(sampled(skia::lit(surface, light)).fR, 0);
  }
  light.range = 100;
  for (float invalid : {std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    light.position.x = invalid;
    EXPECT_FLOAT_EQ(sampled(skia::lit(surface, light)).fR, 0);
  }
}

TEST(SkiaLit, SpotConesUseTheSourceFacingAxisAndCosineTransition) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  Light spot;
  spot.kind = LightKind::Spot;
  spot.elevation = 90;
  spot.position = {16.5f, 4.5f, 20};
  spot.range = 160;
  spot.innerAngle = 20;
  spot.outerAngle = 45;
  spot.ambient = 0;
  Light point = spot;
  point.kind = LightKind::Point;
  const SkBitmap cone = drawnFloat(skia::lit(surface, spot), 64, 8);
  const SkBitmap radial = drawnFloat(skia::lit(surface, point), 64, 8);
  EXPECT_NEAR(cone.getColor4f(16, 4).fR, radial.getColor4f(16, 4).fR, .00001f);
  EXPECT_NEAR(cone.getColor4f(20, 4).fR, radial.getColor4f(20, 4).fR, .00001f);
  const float cosine = 20 / std::sqrt(20.0f * 20 + 12.0f * 12);
  const float expected = (cosine - std::cos(45.0f * 3.14159265f / 180)) /
                         (std::cos(20.0f * 3.14159265f / 180) -
                          std::cos(45.0f * 3.14159265f / 180));
  EXPECT_NEAR(cone.getColor4f(28, 4).fR / radial.getColor4f(28, 4).fR, expected,
              .0001f);
  EXPECT_FLOAT_EQ(cone.getColor4f(48, 4).fR, 0);

  spot.elevation = 45;
  spot.direction = 180;
  spot.innerAngle = 8;
  spot.outerAngle = 15;
  const SkBitmap toRight = drawnFloat(skia::lit(surface, spot), 64, 8);
  EXPECT_GT(toRight.getColor4f(36, 4).fR, .05f);
  EXPECT_FLOAT_EQ(toRight.getColor4f(16, 4).fR, 0);
  spot.direction = 0;
  const SkBitmap toLeft = drawnFloat(skia::lit(surface, spot), 64, 8);
  EXPECT_FLOAT_EQ(toLeft.getColor4f(36, 4).fR, 0);
}

TEST(SkiaLit, SpotDirectionAndElevationRemainLive) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  auto direction = sigil::motion::animatable(180.0f);
  auto elevation = sigil::motion::animatable(45.0f);
  Light spot;
  spot.kind = LightKind::Spot;
  spot.direction = direction;
  spot.elevation = elevation;
  spot.position = {16.5f, 4.5f, 20};
  spot.range = 160;
  spot.innerAngle = 8;
  spot.outerAngle = 15;
  spot.ambient = 0;
  const Paint live = skia::lit(surface, spot);
  EXPECT_TRUE(live.isRunning());
  const auto redAtRight = [&] {
    return drawnFloat(live, 64, 8).getColor4f(36, 4).fR;
  };
  const float aimed = redAtRight();
  EXPECT_GT(aimed, .05f);
  direction = 0;
  EXPECT_FLOAT_EQ(redAtRight(), 0);
  direction = 180;
  EXPECT_NEAR(redAtRight(), aimed, .00001f);
  elevation = 90;
  EXPECT_FLOAT_EQ(redAtRight(), 0);
  elevation = 45;
  EXPECT_NEAR(redAtRight(), aimed, .00001f);
}

TEST(SkiaLit, SpotAnglesClampAndEqualConesHaveAHardBoundary) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  Light light;
  light.kind = LightKind::Spot;
  light.elevation = 90;
  light.position = {4.5f, 4.5f, 20};
  light.range = 100;
  light.innerAngle = 30;
  light.outerAngle = 30;
  light.ambient = 0;
  const SkBitmap hard = drawnFloat(skia::lit(surface, light), 32, 8);
  EXPECT_GT(hard.getColor4f(4, 4).fR, .1f);
  EXPECT_FLOAT_EQ(hard.getColor4f(28, 4).fR, 0);
  light.innerAngle = 300;
  light.outerAngle = 300;
  const SkColor4f clamped = sampled(skia::lit(surface, light));
  light.innerAngle = 180;
  light.outerAngle = 180;
  EXPECT_FLOAT_EQ(clamped.fR, sampled(skia::lit(surface, light)).fR);
  light.innerAngle = std::numeric_limits<float>::infinity();
  light.outerAngle = std::numeric_limits<float>::quiet_NaN();
  const SkColor4f invalid = sampled(skia::lit(surface, light));
  EXPECT_TRUE(std::isfinite(invalid.fR));
  light.innerAngle = 0;
  light.outerAngle = 0;
  EXPECT_FLOAT_EQ(invalid.fR, sampled(skia::lit(surface, light)).fR);
}

TEST(SkiaLit, PositionedLightFollowsAffinePlacementAndRotatedNormals) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  Light light;
  light.kind = LightKind::Point;
  light.position = {4.5f, 4.5f, 20};
  light.range = 100;
  light.ambient = 0;
  const Paint fill = skia::lit(surface, light);
  EXPECT_TRUE(fill.usesWorldSpace());
  EXPECT_FALSE(fill.worldSpace());
  FrameData moved;
  moved.world[2][0] = 60;
  const SkColor4f origin = sampled(fill);
  const SkColor4f shifted = drawnFloat(fill, 8, 8, moved).getColor4f(4, 4);
  EXPECT_GT(origin.fR, shifted.fR + .1f);
  light.position.x += 60;
  const SkColor4f restored =
      drawnFloat(skia::lit(surface, light), 8, 8, moved).getColor4f(4, 4);
  EXPECT_NEAR(origin.fR, restored.fR, .00001f);

  const Material tilted =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.roughness = 1.0f, .normal = from(Color{1, .5f, 1, 1})});
  const FrameData rotated{.world = glm::mat3{0, 1, 0, -1, 0, 0, 20, 0, 1}};
  light.position = {15.5f, 24.5f, 20};
  const SkColor4f turned =
      drawnFloat(skia::lit(tilted, light), 8, 8, rotated).getColor4f(4, 4);
  const Material rootNormal =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.roughness = 1.0f, .normal = from(Color{.5f, 0, 1, 1})});
  light.position = {4.5f, 24.5f, 20};
  const SkColor4f reference = sampled(skia::lit(rootNormal, light));
  EXPECT_NEAR(turned.fR, reference.fR, .00001f);
}

TEST(SkiaLit, AffineNormalGradientsUseInverseTranspose) {
  const Material tilted =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.roughness = 1.0f, .normal = from(Color{1, .5f, 1, 1})});
  FrameData stretched;
  stretched.world[0][0] = 2;
  Light light;
  light.kind = LightKind::Point;
  light.position = {39, 4.5f, 40};
  light.range = 200;
  light.ambient = 0;
  const SkColor4f scaled =
      drawnFloat(skia::lit(tilted, light), 8, 8, stretched).getColor4f(4, 4);
  // Stretching X by two halves the X height gradient in the page frame.
  const Material rootNormal =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.roughness = 1.0f, .normal = from(Color{.75f, .5f, 1, 1})});
  light.position.x -= 4.5f;
  const SkColor4f reference = sampled(skia::lit(rootNormal, light));
  EXPECT_NEAR(scaled.fR, reference.fR, .00001f);
}

TEST(SkiaLit, UnsupportedSpatialTransformsKeepAmbientFinite) {
  const Material surface =
      from(Color{.2f, .3f, .4f, 1}).surface({.roughness = 1.0f});
  Light light;
  light.kind = LightKind::Point;
  light.position = {4.5f, 4.5f, 20};
  light.range = 100;
  light.ambient = .2f;
  FrameData singular;
  singular.world[0][0] = 0;
  FrameData perspective;
  perspective.world[0][2] = .01f;
  for (const FrameData& frame : {singular, perspective}) {
    const SkColor4f colour =
        drawnFloat(skia::lit(surface, light), 8, 8, frame).getColor4f(4, 4);
    EXPECT_TRUE(std::isfinite(colour.fR));
    EXPECT_NEAR(colour.fR, .04f, .00001f);
    EXPECT_NEAR(colour.fG, .06f, .00001f);
    EXPECT_NEAR(colour.fB, .08f, .00001f);
  }
}

TEST(SkiaLit, DirectionalLightRemainsIndependentOfPlacement) {
  const Material surface = relief(16, 8);
  const Paint fill = skia::lit(surface, studio({.direction = 0.0f}));
  EXPECT_FALSE(fill.usesWorldSpace());
  FrameData frame;
  frame.resolution = {16, 8};
  const auto original = skia::shader(fill, frame);
  frame.world = glm::mat3{0, 1, 0, -1, 0, 0, 80, 60, 1};
  const auto moved = skia::shader(fill, frame);
  ASSERT_TRUE(original);
  ASSERT_TRUE(moved);
  EXPECT_EQ(original.get(), moved.get());
  const SkBitmap a = drawnFloat(fill, 16, 8);
  const SkBitmap b = drawnFloat(fill, 16, 8, frame);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 16; ++x)
      EXPECT_EQ(a.getColor4f(x, y), b.getColor4f(x, y));

  const Material own =
      from(Color{.2f, .3f, .4f, 1})
          .surface({.lighting = Lighting(Light{.kind = LightKind::Point})});
  EXPECT_TRUE(skia::usesWorldSpace(own));
  EXPECT_FALSE(skia::usesWorldSpace(
      from(Color{.2f, .3f, .4f, 1}).surface({.lighting = Lighting(studio())})));
}
