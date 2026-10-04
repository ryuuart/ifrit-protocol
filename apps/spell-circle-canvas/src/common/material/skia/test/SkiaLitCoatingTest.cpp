/** @file
 * The clear coat over a lit surface: an untinted lobe above the body, its
 * own reflection of the environment, the emission beneath it attenuated,
 * coverage kept, weights bounded and the lobe following the shared normal.
 */

#include "SkiaLitTestSupport.h"

TEST(SkiaLit, ClearcoatAddsAnUntintedLobeAboveARoughBase) {
  const Lighting key =
      studio({.elevation = 90, .intensity = .03f, .ambient = 0});
  const auto atWeight = [&](float weight) {
    return sampled(
        skia::lit(from(Color{.08f, .18f, .32f, 1})
                      .surface({.roughness = 1.0f, .clearcoat = weight}),
                  key));
  };
  const SkColor4f zero = atWeight(0), half = atWeight(.5f), one = atWeight(1);
  // At the front-facing roughness endpoint, the uncoated highlight is 1/pi.
  constexpr float pi = 3.14159265f;
  EXPECT_NEAR(zero.fR, (.08f + .04f / pi) * .03f, .00001f);
  EXPECT_NEAR(zero.fG, (.18f + .04f / pi) * .03f, .00001f);
  EXPECT_NEAR(zero.fB, (.32f + .04f / pi) * .03f, .00001f);
  // A front-facing dielectric transmits 96% of the base response.
  const float redCoat = one.fR - .96f * zero.fR;
  EXPECT_GT(redCoat, .04f);
  EXPECT_NEAR(redCoat, one.fG - .96f * zero.fG, .00001f);
  EXPECT_NEAR(redCoat, one.fB - .96f * zero.fB, .00001f);
  EXPECT_NEAR(half.fR, (zero.fR + one.fR) * .5f, .00001f);
  EXPECT_NEAR(half.fG, (zero.fG + one.fG) * .5f, .00001f);
  EXPECT_NEAR(half.fB, (zero.fB + one.fB) * .5f, .00001f);
}

TEST(SkiaLit, ClearcoatReflectsAnEnvironmentAboveAFullyRoughBody) {
  const Lighting around = environment(from(Color{.1f, .3f, .7f, 1}),
                                      {.intensity = .5f, .size = {1, 1}});
  const auto reflected = [&](float weight, float reflection, float occlusion) {
    return sampled(skia::lit(from(Color{0, 0, 0, 1})
                                 .surface({.roughness = 1.0f,
                                           .occlusion = occlusion,
                                           .clearcoat = weight,
                                           .reflectionWeight = reflection}),
                             around));
  };
  const SkColor4f zero = reflected(0, .8f, .5f);
  const SkColor4f one = reflected(1, .8f, .5f);
  constexpr float scale = .5f * .8f * .5f;
  const float base = environmentReflectance(1) * scale;
  EXPECT_NEAR(zero.fR, .1f * base, .000002f);
  EXPECT_NEAR(zero.fG, .3f * base, .000002f);
  EXPECT_NEAR(zero.fB, .7f * base, .000002f);
  EXPECT_GT(base, 0);
  const float response = .96f * base + environmentReflectance(.2f) * scale;
  EXPECT_NEAR(one.fR, .1f * response, .000002f);
  EXPECT_NEAR(one.fG, .3f * response, .000002f);
  EXPECT_NEAR(one.fB, .7f * response, .000002f);
  EXPECT_FLOAT_EQ(reflected(1, 0, .5f).fB, 0);
  EXPECT_FLOAT_EQ(reflected(1, .8f, 0).fB, 0);
}

TEST(SkiaLit, ClearcoatAttenuatesEmissionBelowTheCoating) {
  const Lighting dark = studio({.intensity = 0, .ambient = 0});
  const auto emitted = [&](std::optional<Material> normal) {
    return sampled(skia::lit(from(Color{0, 0, 0, 1})
                                 .surface({.roughness = 1.0f,
                                           .normal = std::move(normal),
                                           .emission = {.25f, .5f, .75f, 1},
                                           .emissionStrength = 1,
                                           .clearcoat = 1}),
                             dark));
  };
  const SkColor4f front = emitted(std::nullopt);
  EXPECT_NEAR(front.fR, .24f, .00001f);
  EXPECT_NEAR(front.fG, .48f, .00001f);
  EXPECT_NEAR(front.fB, .72f, .00001f);
  const SkColor4f grazing = emitted(from(Color{.99f, .5f, .505f, 1}));
  EXPECT_GT(grazing.fB, 0);
  EXPECT_LT(grazing.fB, front.fB * .1f);
}

TEST(SkiaLit, ClearcoatPreservesCoverageAndUnlitColors) {
  const auto surface = [](bool unlit) {
    return from(Color{.2f, .4f, .6f, .35f})
        .surface({.roughness = .8f, .clearcoat = 1, .unlit = unlit});
  };
  const Lighting key = studio({.intensity = .03f, .ambient = 0});
  const SkColor4f lit = sampled(skia::lit(surface(false), key));
  EXPECT_NEAR(lit.fA, .35f, .00001f);
  for (const SkColor4f flat : {sampled(skia::lit(surface(false), Lighting{})),
                               sampled(skia::lit(surface(true), key))}) {
    EXPECT_NEAR(flat.fR, .2f, .00001f);
    EXPECT_NEAR(flat.fG, .4f, .00001f);
    EXPECT_NEAR(flat.fB, .6f, .00001f);
    EXPECT_NEAR(flat.fA, .35f, .00001f);
  }
  const SkColor4f empty = sampled(
      skia::lit(from(Color{1, 1, 1, 0}).surface({.clearcoat = 1}), key));
  EXPECT_FLOAT_EQ(empty.fR, 0);
  EXPECT_FLOAT_EQ(empty.fG, 0);
  EXPECT_FLOAT_EQ(empty.fB, 0);
  EXPECT_FLOAT_EQ(empty.fA, 0);
}

TEST(SkiaLit, ClearcoatWeightsAreBoundedAndGrazingSamplesStayFinite) {
  const Lighting key =
      studio({.elevation = 90, .intensity = .03f, .ambient = 0});
  const auto atWeight = [&](float weight) {
    return sampled(
        skia::lit(from(Color{.1f, .2f, .3f, 1})
                      .surface({.roughness = 1.0f, .clearcoat = weight}),
                  key));
  };
  for (const auto [weight, expected] :
       std::array{std::pair{-2.0f, 0.0f}, std::pair{2.0f, 1.0f},
                  std::pair{std::numeric_limits<float>::infinity(), 0.0f},
                  std::pair{std::numeric_limits<float>::quiet_NaN(), 0.0f}}) {
    const SkColor4f actual = atWeight(weight), bounded = atWeight(expected);
    EXPECT_FLOAT_EQ(actual.fR, bounded.fR);
    EXPECT_FLOAT_EQ(actual.fG, bounded.fG);
    EXPECT_FLOAT_EQ(actual.fB, bounded.fB);
  }
  const SkColor4f grazing = sampled(skia::lit(
      from(Color{.1f, .2f, .3f, 1})
          .surface({.roughness = 1.0f,
                    .normal = from(Color{1, .5f, .5f, 1}),
                    .clearcoat = 1}),
      Lighting(studio({.elevation = 0, .intensity = .03f, .ambient = 0}),
               environment(from(Color{.1f, .2f, .3f, 1}), {.size = {1, 1}}))));
  EXPECT_TRUE(std::isfinite(grazing.fR));
  EXPECT_TRUE(std::isfinite(grazing.fG));
  EXPECT_TRUE(std::isfinite(grazing.fB));
  EXPECT_GE(grazing.fR, 0);
  EXPECT_FLOAT_EQ(grazing.fA, 1);
}

TEST(SkiaLit, ClearcoatFollowsTheSharedNormalAndMovingLight) {
  const Material coated = from(Color{0, 0, 0, 1})
                              .surface({.roughness = 1.0f,
                                        .normal = image(twoFacedNormals(16, 8)),
                                        .clearcoat = 1});
  sigil::motion::Animatable<float> sun = sigil::motion::animatable(180.0f);
  const Paint turning = skia::lit(
      coated,
      studio(
          {.direction = sun, .elevation = 0, .intensity = .03f, .ambient = 0}));
  EXPECT_TRUE(turning.isRunning());
  const SkBitmap left = drawnFloat(turning, 16, 8);
  sun = 0;
  const SkBitmap right = drawnFloat(turning, 16, 8);
  EXPECT_GT(left.getColor4f(2, 4).fR, left.getColor4f(14, 4).fR + .02f);
  EXPECT_GT(right.getColor4f(14, 4).fR, right.getColor4f(2, 4).fR + .02f);
  EXPECT_NEAR(left.getColor4f(2, 4).fR, right.getColor4f(14, 4).fR, .0001f);
}
