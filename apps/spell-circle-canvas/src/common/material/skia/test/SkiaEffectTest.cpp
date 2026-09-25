/** @file
 * WHAT A POST-PROCESSING EFFECT IS: the identity a built filter compares
 * by, the values and operands a recipe snapshot compares by, what makes
 * one live, how two of them compose into a chain, and what setting the
 * same uniform twice does. Nothing here paints: the three files beside
 * this one paint the blurs, the light and the slot an executor fills.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Texture.h>

#include <memory>

using namespace sigil::material;

TEST(SkiaEffect, AStockFilterComparesByItsRecipe) {
  const Filter glow = Filter::glow({0, 1, 1, 1}, 6.0f);
  EXPECT_NE(skia::resolvedImageFilter(glow, nullptr), nullptr);
  EXPECT_FALSE(glow.isRunning());
  // A stock pass compares by what it was built from, so one described
  // again is equal although its built filter is a new object…
  EXPECT_TRUE(glow == Filter(glow));
  EXPECT_TRUE(glow == Filter::glow({0, 1, 1, 1}, 6.0f));
  EXPECT_FALSE(glow == Filter::glow({0, 1, 1, 1}, 7.0f));
  EXPECT_FALSE(glow == Filter::glow({1, 0, 1, 1}, 6.0f));
  const Filter shadow =
      Filter::dropShadow({0, 0, 0, 0.5f}, {.blur = 4, .offset = {2, 3}});
  EXPECT_TRUE(shadow ==
              Filter::dropShadow({0, 0, 0, 0.5f}, {.blur = 4, .offset = {2, 3}}));
  EXPECT_FALSE(shadow ==
               Filter::dropShadow({0, 0, 0, 0.5f}, {.blur = 4, .offset = {2, 4}}));
  EXPECT_TRUE(Filter::blur(3) == Filter::blur(3));
  EXPECT_FALSE(Filter::blur(3) == Filter::blur(4));
  EXPECT_TRUE(Filter::dilate(2) == Filter::dilate(2));
  // …while an image filter built by hand carries no recipe, and compares by
  // its identity.
  const sk_sp<SkImageFilter> raw = SkImageFilters::Blur(3, 3, nullptr);
  EXPECT_TRUE(skia::filter(raw) == skia::filter(raw));
  EXPECT_FALSE(skia::filter(raw) ==
               skia::filter(SkImageFilters::Blur(3, 3, nullptr)));
  // The empty effect resolves to nothing and is reflexive.
  EXPECT_EQ(skia::resolvedImageFilter(Filter(), nullptr), nullptr);
  EXPECT_TRUE(Filter() == Filter{});
}

TEST(SkiaEffect, RecipeSnapshotsCompareTheirValuesAndOperands) {
  struct Parameters {
    float gain = 1;
  };
  const auto tint = std::make_shared<const Recipe>(
      Recipe::of<Parameters>("effect.snapshot.tint")
          .body(Target::SkSL, "half4 main(float2 p) { return half4(gain); }"));
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<Parameters>("effect.snapshot")
          .slot("content")
          .slot("tint")
          .body(Target::SkSL,
                "half4 main(float2 p) { return content.eval(p) * "
                "tint.eval(p) * half(gain); }"));
  Material material(recipe, Parameters{});
  material.slot("tint", Material(tint, Parameters{}));
  const Filter captured = Filter::of(material);
  ASSERT_NE(skia::imageFilter(captured), nullptr);
  EXPECT_TRUE(captured == Filter::of(material));
  EXPECT_FALSE(captured == Filter::of(material, 12.0f));
  EXPECT_TRUE(captured == captured.then(Filter{}));
  EXPECT_FALSE(captured == skia::filter(skia::imageFilter(captured)));

  Material changed = material;
  changed.set("gain", 0.5f);
  EXPECT_FALSE(captured == Filter::of(changed));
  changed = material;
  changed.slot("tint", Material(tint, Parameters{0.5f}));
  EXPECT_FALSE(captured == Filter::of(changed));
  EXPECT_TRUE(captured == Filter::of(material));
}

TEST(SkiaEffect, RecipeSnapshotsKeepCapturedLiveValuesApart) {
  struct Parameters {
    float gain = 1;
  };
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<Parameters>("effect.snapshot.live")
          .slot("content")
          .body(
              Target::SkSL,
              "half4 main(float2 p) { return content.eval(p) * half(gain); }"));
  sigil::motion::Animatable<float> gain = sigil::motion::animatable(1.0f);
  Material material(recipe, Parameters{});
  material.bind("gain", gain);
  const Filter first = Filter::of(material);
  gain = 0.5f;
  const Filter second = Filter::of(material);
  ASSERT_NE(skia::imageFilter(first), nullptr);
  ASSERT_NE(skia::imageFilter(second), nullptr);
  EXPECT_FALSE(first == second);
  EXPECT_TRUE(first == Filter(first));
  EXPECT_FALSE(first.isRunning());

  const Filter expiredSource = [&] {
    sigil::motion::Animatable<float> localGain = sigil::motion::animatable(0.25f);
    Material local(recipe, Parameters{});
    local.bind("gain", localGain);
    return Filter::of(local);
  }();
  const Filter copy = expiredSource;
  EXPECT_TRUE(expiredSource == copy);
  EXPECT_NE(skia::resolvedImageFilter(copy, nullptr), nullptr);
}

TEST(SkiaEffect, RecipeSnapshotsDistinguishSurfaceLowering) {
  struct Parameters {};
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<Parameters>("effect.snapshot.surface")
          .slot("content")
          .slot("response")
          .channelwise("response")
          .body(Target::SkSL,
                "half4 main(float2 p) { half4 c = content.eval(p); "
                "return half4(response.eval(float2(c.r * 255, 0)).r, "
                "response.eval(float2(c.g * 255, 0)).g, "
                "response.eval(float2(c.b * 255, 0)).b, c.a); }"));
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256, 1));
  ASSERT_NE(surface, nullptr);
  surface->getCanvas()->clear(SK_ColorWHITE);
  Material material(recipe);
  material.slot("response", Texture::of(surface->makeImageSnapshot()));
  const Filter table =
      skia::lowered(material, kRGBA_8888_SkColorType);
  const Filter shader =
      skia::lowered(material, kRGBA_F16_SkColorType);
  ASSERT_NE(skia::colorFilter(table), nullptr);
  ASSERT_NE(skia::imageFilter(shader), nullptr);
  EXPECT_FALSE(table == shader);
}

TEST(SkiaEffect, ABoundUniformMakesItLiveAndItNeverPrunes) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "uniform float uK;\n"
               "half4 main(float2 p) { return content.eval(p) * half(uK); }"));
  ASSERT_NE(effect, nullptr);
  sigil::motion::Animatable<float> k = sigil::motion::animatable(1.0f);
  Filter live = skia::program(effect);
  EXPECT_FALSE(live.isRunning());
  live.bind("uK", k);
  EXPECT_TRUE(live.isRunning());
  // Live never prunes — the same rule a live paint follows.
  EXPECT_FALSE(live == live);
}

TEST(SkiaEffect, ChainingPrecomposesAndAnEmptySideIsTheOther) {
  const Filter blur = Filter::directionalBlur(4.0f, 0.0f, 1.0f);
  const Filter glow = Filter::glow({1, 0, 0, 1}, 3.0f);
  EXPECT_NE(skia::resolvedImageFilter(blur.then(glow), nullptr), nullptr);
  // then() over nothing is the effect itself, so a conditional chain
  // needs no branch at the call site.
  EXPECT_TRUE(blur.then(Filter{}) == blur);
  EXPECT_TRUE(Filter{}.then(blur) == blur);
}

TEST(SkiaEffect, ChainingKeepsTheNodesAContextNeedingChildLivesIn) {
  // Precomposing two static sides into one filter is what makes a chain
  // cost nothing per paint — but a child that needs the paint context is
  // not static: a sigma map reading uResolution, an image fitted to the
  // box. Frozen into the null-context snapshot it would paint the box it
  // was first described in for ever, and the composed effect would answer
  // usesWorldSpace() with false because the children are gone.
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "uniform shader tint;\n"
               "half4 main(float2 p) { return content.eval(p) * "
               "tint.eval(p); }"));
  ASSERT_NE(effect, nullptr);
  Paint anchored = Paint::solid({1, 0, 0, 1});
  anchored.worldSpace();
  EXPECT_TRUE(anchored.geometryDependent());

  Filter shaded = skia::program(effect);
  shaded.slot("tint", anchored);
  EXPECT_FALSE(shaded.isRunning());
  EXPECT_TRUE(shaded.usesWorldSpace());

  const Filter chained = shaded.then(Filter::glow({0, 1, 1, 1}, 4));
  EXPECT_TRUE(chained.usesWorldSpace());
  EXPECT_NE(skia::resolvedImageFilter(chained, nullptr), nullptr);
  // …and the other way round, since either side may hold the child.
  EXPECT_TRUE(
      Filter::glow({0, 1, 1, 1}, 4).then(shaded).usesWorldSpace());
}

TEST(SkiaEffect, SettingOneUniformTwiceReplacesItRatherThanStacking) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "uniform float uK;\n"
               "half4 main(float2 p) { return content.eval(p) * half(uK); }"));
  ASSERT_NE(effect, nullptr);
  Filter twice = skia::program(effect);
  twice.set("uK", 0.25f);
  twice.set("uK", 0.75f);
  // Last write wins, as slot() does: the same effect described once at
  // the final value is the same recipe, so a re-described node prunes.
  Filter once = skia::program(effect);
  once.set("uK", 0.75f);
  EXPECT_TRUE(twice == once);
}
