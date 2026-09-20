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
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Texture.h>

#include <memory>

using namespace sigil::material;

TEST(SkiaEffect, AFilterIsBuiltOnceAndComparesByItsIdentity) {
  const skia::Effect glow = skia::Effect::glow({0, 1, 1, 1}, 6.0f);
  EXPECT_NE(glow.resolvedImageFilter(nullptr), nullptr);
  EXPECT_FALSE(glow.isAnimated());
  // filter() compares by the built filter's pointer, so a copy prunes and
  // a separately built one does not.
  EXPECT_TRUE(glow == skia::Effect(glow));
  EXPECT_FALSE(glow == skia::Effect::glow({0, 1, 1, 1}, 6.0f));
  // The empty effect resolves to nothing and is reflexive.
  EXPECT_EQ(skia::Effect().resolvedImageFilter(nullptr), nullptr);
  EXPECT_TRUE(skia::Effect() == skia::Effect{});
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
  const skia::Effect captured = skia::Effect::recipe(material);
  ASSERT_NE(captured.imageFilter(), nullptr);
  EXPECT_TRUE(captured == skia::Effect::recipe(material));
  EXPECT_FALSE(captured == skia::Effect::recipe(material, 12.0f));
  EXPECT_TRUE(captured == captured.then(skia::Effect{}));
  EXPECT_FALSE(captured == skia::Effect::filter(captured.imageFilter()));

  Material changed = material;
  changed.set("gain", 0.5f);
  EXPECT_FALSE(captured == skia::Effect::recipe(changed));
  changed = material;
  changed.slot("tint", Material(tint, Parameters{0.5f}));
  EXPECT_FALSE(captured == skia::Effect::recipe(changed));
  EXPECT_TRUE(captured == skia::Effect::recipe(material));
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
  choreograph::Output<float> gain(1.0f);
  Material material(recipe, Parameters{});
  material.bind("gain", &gain);
  const skia::Effect first = skia::Effect::recipe(material);
  gain = 0.5f;
  const skia::Effect second = skia::Effect::recipe(material);
  ASSERT_NE(first.imageFilter(), nullptr);
  ASSERT_NE(second.imageFilter(), nullptr);
  EXPECT_FALSE(first == second);
  EXPECT_TRUE(first == skia::Effect(first));
  EXPECT_FALSE(first.isAnimated());

  const skia::Effect expiredSource = [&] {
    choreograph::Output<float> localGain(0.25f);
    Material local(recipe, Parameters{});
    local.bind("gain", &localGain);
    return skia::Effect::recipe(local);
  }();
  const skia::Effect copy = expiredSource;
  EXPECT_TRUE(expiredSource == copy);
  EXPECT_NE(copy.resolvedImageFilter(nullptr), nullptr);
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
  const skia::Effect table =
      skia::Effect::recipe(material, kRGBA_8888_SkColorType);
  const skia::Effect shader =
      skia::Effect::recipe(material, kRGBA_F16_SkColorType);
  ASSERT_NE(table.colorFilter(), nullptr);
  ASSERT_NE(shader.imageFilter(), nullptr);
  EXPECT_FALSE(table == shader);
}

TEST(SkiaEffect, ABoundUniformMakesItLiveAndItNeverPrunes) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "uniform float uK;\n"
               "half4 main(float2 p) { return content.eval(p) * half(uK); }"));
  ASSERT_NE(effect, nullptr);
  choreograph::Output<float> k(1.0f);
  skia::Effect live = skia::Effect::shader(effect);
  EXPECT_FALSE(live.isAnimated());
  live.uniform("uK", &k);
  EXPECT_TRUE(live.isAnimated());
  // Live never prunes — the same rule a live paint follows.
  EXPECT_FALSE(live == live);
}

TEST(SkiaEffect, ChainingPrecomposesAndAnEmptySideIsTheOther) {
  const skia::Effect blur = skia::Effect::directionalBlur(4.0f, 0.0f, 1.0f);
  const skia::Effect glow = skia::Effect::glow({1, 0, 0, 1}, 3.0f);
  EXPECT_NE(blur.then(glow).resolvedImageFilter(nullptr), nullptr);
  // then() over nothing is the effect itself, so a conditional chain
  // needs no branch at the call site.
  EXPECT_TRUE(blur.then(skia::Effect{}) == blur);
  EXPECT_TRUE(skia::Effect{}.then(blur) == blur);
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
  skia::Paint anchored = skia::Paint::solid({1, 0, 0, 1});
  anchored.worldSpace();
  EXPECT_TRUE(anchored.geometryDependent());

  skia::Effect shaded = skia::Effect::shader(effect);
  shaded.slot("tint", anchored);
  EXPECT_FALSE(shaded.isAnimated());
  EXPECT_TRUE(shaded.usesWorldSpace());

  const skia::Effect chained = shaded.then(skia::Effect::glow({0, 1, 1, 1}, 4));
  EXPECT_TRUE(chained.usesWorldSpace());
  EXPECT_NE(chained.resolvedImageFilter(nullptr), nullptr);
  // …and the other way round, since either side may hold the child.
  EXPECT_TRUE(
      skia::Effect::glow({0, 1, 1, 1}, 4).then(shaded).usesWorldSpace());
}

TEST(SkiaEffect, SettingOneUniformTwiceReplacesItRatherThanStacking) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "uniform float uK;\n"
               "half4 main(float2 p) { return content.eval(p) * half(uK); }"));
  ASSERT_NE(effect, nullptr);
  skia::Effect twice = skia::Effect::shader(effect);
  twice.uniform("uK", 0.25f);
  twice.uniform("uK", 0.75f);
  // Last write wins, as slot() does: the same effect described once at
  // the final value is the same recipe, so a re-described node prunes.
  skia::Effect once = skia::Effect::shader(effect);
  once.uniform("uK", 0.75f);
  EXPECT_TRUE(twice == once);
}
