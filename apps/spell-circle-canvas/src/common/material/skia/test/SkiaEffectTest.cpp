/** @file
 * WHAT A POST-PROCESSING EFFECT IS: the identity a built filter compares
 * by, the values and operands a recipe snapshot compares by, what makes
 * one live, how two of them compose into a chain, and what setting the
 * same uniform twice does. A live array's publication is checked through
 * a filtered layer while a sibling scalar changes.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/values/Animatable.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "../Effect.h"
#include "support/EffectRead.h"

using namespace sigil::material;

namespace {
template <class Radius>
concept FilterRadius = requires(const Material& program, Radius radius) {
  Filter::of(program, radius);
};

static_assert(FilterRadius<float>);
static_assert(FilterRadius<int>);
static_assert(!FilterRadius<SkColorType>);
static_assert(!FilterRadius<SkBlendMode>);
}  // namespace

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
  EXPECT_TRUE(shadow == Filter::dropShadow({0, 0, 0, 0.5f},
                                           {.blur = 4, .offset = {2, 3}}));
  EXPECT_FALSE(shadow == Filter::dropShadow({0, 0, 0, 0.5f},
                                            {.blur = 4, .offset = {2, 4}}));
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
    sigil::motion::Animatable<float> localGain =
        sigil::motion::animatable(0.25f);
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
  material.slot("response", Texture(surface->makeImageSnapshot()));
  const Filter table = skia::lowered(material, kRGBA_8888_SkColorType);
  const Filter shader = skia::lowered(material, kRGBA_F16_SkColorType);
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

namespace {

std::array<float, 4> filterCenter(const sk_sp<SkImageFilter>& filter) {
  const auto pixels = sigil::material::test::bloomThrough(filter, {1, 1, 1, 1});
  EXPECT_EQ(pixels.size(), 64u * 64 * 4);
  if (pixels.size() != 64u * 64 * 4) return {};
  const float* center = sigil::material::test::texel(pixels, 32, 32);
  return {center[0], center[1], center[2], center[3]};
}

void expectFilterCenter(const sk_sp<SkImageFilter>& filter,
                        std::array<float, 4> expected) {
  const auto actual = filterCenter(filter);
  // CPU runtime-filter intermediates can quantize to eight-bit channels
  // before the result is drawn onto the float destination.
  for (size_t channel = 0; channel < actual.size(); ++channel) {
    SCOPED_TRACE(channel);
    EXPECT_NEAR(actual[channel], expected[channel], 1.0f / 255);
  }
}

template <class Write>
void expectStaticFilterRejection(Filter& filter, Write write) {
  const Filter before = filter;
  const auto held = skia::resolvedImageFilter(filter);
  ASSERT_TRUE(held);
  const auto expected = filterCenter(held);
  testing::internal::CaptureStderr();
  write();
  testing::internal::GetCapturedStderr();
  EXPECT_FALSE(filter.isRunning());
  EXPECT_EQ(filter, before);
  EXPECT_EQ(skia::resolvedImageFilter(filter), held);
  EXPECT_EQ(filterCenter(skia::resolvedImageFilter(filter)), expected);
}

}  // namespace

TEST(SkiaEffect, RawInitialScalarsRequireSingleFloatDeclarations) {
  const char* uniforms =
      "uniform int uInteger; uniform float uArray[1]; "
      "uniform float uFloat; uniform half uHalf; ";
  const auto values =
      std::vector<std::pair<std::string, float>>{{"uFloat", .25f},
                                                 {"uHalf", .5f},
                                                 {"uInteger", .75f},
                                                 {"uArray", .75f},
                                                 {"uAbsent", .75f}};
  const std::string shader =
      std::string(uniforms) +
      "uniform shader content; half4 main(float2 p) { return half4(" +
      "uFloat, uHalf, float(uInteger) * 1e-8 + uArray[0], content.eval(p).a); "
      "}";
  auto [program, error] =
      SkRuntimeEffect::MakeForShader(SkString(shader.c_str()));
  ASSERT_TRUE(program) << error.c_str();
  const Filter expected =
      skia::program(program, {{"uFloat", .25f}, {"uHalf", .5f}});
  testing::internal::CaptureStderr();
  const Filter actual = skia::program(program, values);
  const auto said = testing::internal::GetCapturedStderr();
  for (const char* name : {"uInteger", "uArray", "uAbsent"})
    EXPECT_NE(said.find(name), std::string::npos) << said;
  EXPECT_EQ(actual, expected);
  expectFilterCenter(skia::resolvedImageFilter(actual), {.25f, .5f, 0, 1});

  const std::string color =
      std::string(uniforms) +
      "half4 main(half4 c) { return half4(uFloat, uHalf, " +
      "float(uInteger) * 1e-8 + uArray[0], c.a); }";
  auto [map, mapError] =
      SkRuntimeEffect::MakeForColorFilter(SkString(color.c_str()));
  ASSERT_TRUE(map) << mapError.c_str();
  const auto expectedMap =
      skia::Effect::colorProgram(map, {{"uFloat", .25f}, {"uHalf", .5f}});
  testing::internal::CaptureStderr();
  const auto actualMap = skia::Effect::colorProgram(map, values);
  testing::internal::GetCapturedStderr();
  EXPECT_EQ(actualMap, expectedMap);
  expectFilterCenter(actualMap.resolvedImageFilter(), {.25f, .5f, 0, 1});
}

TEST(SkiaEffect, RawTypedUploadsRejectEqualSizedIncompatibleDeclarations) {
  struct Input {
    const char* declaration;
    const char* component;
    size_t count;
    bool integer;
  };
  const Input cases[] = {
      {"int uValue", "float(uValue) * 1e-8", 1, true},
      {"float uValue[1]", "uValue[0]", 1, false},
      {"int2 uValue", "float(uValue.x) * 1e-8", 2, true},
      {"float uValue[2]", "uValue[0]", 2, false},
      {"int4 uValue", "float(uValue.x) * 1e-8", 4, true},
      {"float2x2 uValue", "uValue[0][0]", 4, false},
      {"float4 uValue[1]", "uValue[0].x", 4, false},
  };
  auto live = sigil::motion::animatable(.75f);
  for (const auto& test : cases) {
    SCOPED_TRACE(test.declaration);
    const std::string source =
        std::string("uniform ") + test.declaration +
        "; uniform float uKeep; uniform shader content; " +
        "half4 main(float2 p) { return half4(uKeep, " + test.component +
        ", 0, content.eval(p).a); }";
    auto [program, error] =
        SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
    ASSERT_TRUE(program) << error.c_str();
    Filter filter = skia::program(program, {{"uKeep", .25f}});
    if (!test.integer)
      filter.set("uValue", std::vector<float>(test.count, .125f));
    expectStaticFilterRejection(filter, [&] {
      if (test.count == 1)
        filter.set("uValue", .75f);
      else if (test.count == 2)
        filter.set("uValue", std::array<float, 2>{.75f, .5f});
      else
        filter.set("uValue", std::array<float, 4>{.75f, .5f, .25f, 1});
    });
    expectStaticFilterRejection(filter, [&] { filter.bind("uValue", live); });
    if (test.integer) {
      expectStaticFilterRejection(filter, [&] {
        filter.set("uValue", std::vector<float>(test.count, .75f));
      });
      auto block = std::make_shared<UniformBlock>(test.count);
      std::fill(block->values().begin(), block->values().end(), .75f);
      block->commit();
      expectStaticFilterRejection(filter,
                                  [&] { filter.bind("uValue", block); });
    }
    expectStaticFilterRejection(filter, [&] { filter.set("uAbsent", .75f); });
  }
}

TEST(SkiaEffect, RejectedRawUploadsRetainScalarAndHalfLiveCells) {
  auto [program, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
uniform shader content;
uniform int uInteger;
uniform float uArray[1];
uniform float uFloat;
uniform half uHalf;
half4 main(float2 p) {
  return half4(uFloat, uHalf, uArray[0] + float(uInteger) * 1e-8, content.eval(p).a);
}
)"));
  ASSERT_TRUE(program) << error.c_str();
  Filter filter = skia::program(program);
  filter.set("uFloat", .125f)
      .set("uHalf", .25f)
      .set("uArray", std::vector<float>{.125f});
  auto scalar = sigil::motion::animatable(.25f);
  auto half = sigil::motion::animatable(.5f);
  filter.bind("uFloat", scalar).bind("uHalf", half);
  ASSERT_TRUE(filter.isRunning());
  const Filter copy = filter;
  const auto before = filterCenter(skia::resolvedImageFilter(filter));
  expectFilterCenter(skia::resolvedImageFilter(filter), {.25f, .5f, .125f, 1});
  auto invalidBlock = std::make_shared<UniformBlock>(2);
  invalidBlock->values()[0] = .75f;
  invalidBlock->commit();
  testing::internal::CaptureStderr();
  filter.bind("uInteger", scalar)
      .bind("uArray", half)
      .set("uFloat", std::array<float, 2>{.75f, .75f})
      .set("uHalf", std::vector<float>{.75f, .75f})
      .bind("uFloat", invalidBlock)
      .bind("uHalf", invalidBlock);
  testing::internal::GetCapturedStderr();
  EXPECT_EQ(filterCenter(skia::resolvedImageFilter(filter)), before);
  scalar = .5f;
  half = .25f;
  const auto changed = filterCenter(skia::resolvedImageFilter(filter));
  expectFilterCenter(skia::resolvedImageFilter(filter), {.5f, .25f, .125f, 1});
  EXPECT_EQ(filterCenter(skia::resolvedImageFilter(copy)), changed);
}

TEST(SkiaEffect, RawFloatPacketsPreserveVectorMatrixAndArrayPublication) {
  struct Packet {
    const char* declaration;
    const char* first;
    const char* last;
    size_t count;
  };
  const Packet cases[] = {
      {"float3 uValue", "uValue.x", "uValue.z", 3},
      {"float2x2 uValue", "uValue[0][0]", "uValue[1][1]", 4},
      {"float4 uValue[2]", "uValue[0].x", "uValue[1].w", 8},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.declaration);
    const std::string source =
        std::string("uniform ") + test.declaration +
        "; uniform shader content; uniform float uSibling; " +
        "half4 main(float2 p) { return half4(" + test.first + ", " + test.last +
        ", uSibling, content.eval(p).a); }";
    auto [program, error] =
        SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
    ASSERT_TRUE(program) << error.c_str();
    std::vector<float> values(test.count, .25f);
    values.back() = .75f;
    Filter fixed = skia::program(program, {{"uSibling", .5f}});
    fixed.set("uValue", values);
    expectFilterCenter(skia::resolvedImageFilter(fixed), {.25f, .75f, .5f, 1});
    auto block = std::make_shared<UniformBlock>(test.count);
    std::copy(values.begin(), values.end(), block->values().begin());
    block->commit();
    auto sibling = sigil::motion::animatable(.5f);
    Filter bound = skia::program(program);
    bound.bind("uValue", block).bind("uSibling", sibling);
    expectFilterCenter(skia::resolvedImageFilter(bound), {.25f, .75f, .5f, 1});
    block->values()[0] = .5f;
    sibling = .75f;
    expectFilterCenter(skia::resolvedImageFilter(bound), {.25f, .75f, .75f, 1});
    block->commit();
    expectFilterCenter(skia::resolvedImageFilter(bound), {.5f, .75f, .75f, 1});
  }
}

TEST(SkiaEffect, AutomaticSamplingRadiusRequiresASingleFloatingScalar) {
  struct Radius {
    const char* declaration;
    const char* component;
    bool valid;
  };
  const Radius cases[] = {
      {"float _sampleRadius", "_sampleRadius", true},
      {"half _sampleRadius", "_sampleRadius", true},
      {"int _sampleRadius", "float(_sampleRadius) * 1e-8", false},
      {"float _sampleRadius[1]", "_sampleRadius[0]", false},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.declaration);
    const std::string source =
        std::string("uniform ") + test.declaration +
        "; uniform shader content; half4 main(float2 p) { return half4(" +
        test.component + ", 0, 0, content.eval(p).a); }";
    auto [program, error] =
        SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
    ASSERT_TRUE(program) << error.c_str();
    const auto filter = skia::Effect::shader(program, {}, .75f);
    expectFilterCenter(filter.resolvedImageFilter(),
                       {test.valid ? .75f : 0, 0, 0, 1});
  }
}

TEST(SkiaEffect, DraftArraysStayHiddenWhileScalarBindingsMove) {
  auto [program, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
uniform shader content;
uniform float table[1];
uniform float sibling;
half4 main(float2 p) {
  half coverage = content.eval(p).a;
  return half4(table[0] * coverage, sibling * coverage, 0, coverage);
}
)"));
  ASSERT_NE(program, nullptr) << error.c_str();
  auto block = std::make_shared<UniformBlock>(1);
  block->values()[0] = 0.25f;
  block->commit();
  auto sibling = sigil::motion::animatable(0.25f);
  Filter filter = skia::program(program);
  filter.bind("table", block).bind("sibling", sibling);
  ASSERT_TRUE(filter.isRunning());
  const auto expectChannels = [](const sk_sp<SkImageFilter>& effect, float red,
                                 float green) {
    ASSERT_NE(effect, nullptr);
    const auto pixels =
        sigil::material::test::bloomThrough(effect, {1, 1, 1, 1});
    ASSERT_EQ(pixels.size(), 64u * 64 * 4);
    const float* pixel = sigil::material::test::texel(pixels, 32, 32);
    EXPECT_NEAR(pixel[0], red, 1.0f / 255);
    EXPECT_NEAR(pixel[1], green, 1.0f / 255);
  };

  const sk_sp<SkImageFilter> first = skia::resolvedImageFilter(filter);
  expectChannels(first, 0.25f, 0.25f);
  block->values()[0] = 0.75f;
  sibling = 0.5f;
  expectChannels(skia::resolvedImageFilter(filter), 0.25f, 0.5f);
  Filter fresh = skia::program(program);
  fresh.bind("table", block).bind("sibling", sibling);
  expectChannels(skia::resolvedImageFilter(fresh), 0.25f, 0.5f);
  block->commit();
  expectChannels(skia::resolvedImageFilter(filter), 0.75f, 0.5f);
  expectChannels(first, 0.25f, 0.25f);
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
  EXPECT_TRUE(Filter::glow({0, 1, 1, 1}, 4).then(shaded).usesWorldSpace());
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

TEST(SkiaEffect, ConstantProgramBindingsJoinEqualityAndCopyOnWrite) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform shader content; uniform float gain; "
      "half4 main(float2 p) { return content.eval(p) * half(gain); }"));
  ASSERT_TRUE(effect) << error.c_str();
  const auto make = [&](float gain) {
    Filter filter = skia::program(effect);
    filter.bind("gain", gain);
    return filter;
  };
  const Filter low = make(0.25f);
  Filter high = low;
  high.bind("gain", 0.75f);
  EXPECT_FALSE(low.isRunning());
  EXPECT_FALSE(high.isRunning());
  EXPECT_NE(low, high);
  EXPECT_EQ(low, Filter(low));
  EXPECT_EQ(high, make(0.75f));
  expectFilterCenter(skia::resolvedImageFilter(low), {0.25f, 0.25f, 0.25f, 1});
  expectFilterCenter(skia::resolvedImageFilter(high), {0.75f, 0.75f, 0.75f, 1});
}

TEST(SkiaEffect, ConstantBlurBindingsJoinEquality) {
  for (const bool parametric : {false, true}) {
    SCOPED_TRACE(parametric);
    const auto make = [&](float sigma) {
      Filter filter = parametric ? Filter::blur(Paint::solid({1, 1, 1, 1}), 8)
                                 : Filter::directionalBlur(8, 0);
      filter.bind(parametric ? "maxSigma" : "sigma", sigma);
      return filter;
    };
    const Filter low = make(2);
    const Filter high = make(4);
    EXPECT_FALSE(low.isRunning());
    EXPECT_FALSE(high.isRunning());
    EXPECT_NE(low, high);
    EXPECT_EQ(low, Filter(low));
    EXPECT_EQ(high, make(4));
    const auto lowPixels = sigil::material::test::bloomThrough(
        skia::resolvedImageFilter(low), {1, 1, 1, 1});
    const auto highPixels = sigil::material::test::bloomThrough(
        skia::resolvedImageFilter(high), {1, 1, 1, 1});
    const Filter reference = parametric
                                 ? Filter::blur(Paint::solid({1, 1, 1, 1}), 4)
                                 : Filter::directionalBlur(4, 0);
    const auto expected = sigil::material::test::bloomThrough(
        skia::resolvedImageFilter(reference), {1, 1, 1, 1});
    ASSERT_EQ(lowPixels.size(), highPixels.size());
    ASSERT_EQ(highPixels.size(), expected.size());
    EXPECT_NE(lowPixels, highPixels);
    for (size_t channel = 0; channel < highPixels.size(); ++channel)
      EXPECT_NEAR(highPixels[channel], expected[channel], 1.0f / 255)
          << "channel " << channel;
  }
}

TEST(SkiaEffect, ConstantAndSlotEditsLeaveHeldFilterCopiesUnchanged) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform shader content; uniform shader tint; uniform float gain; "
      "half4 main(float2 p) { return content.eval(p) * tint.eval(p) * "
      "half(gain); }"));
  ASSERT_TRUE(effect) << error.c_str();
  Filter original = skia::program(effect);
  original.set("gain", 0.25f).slot("tint", Paint::solid({1, 1, 1, 1}));
  const Filter held = original;
  const auto heldImage = skia::resolvedImageFilter(held);
  original.set("gain", 0.75f);
  expectFilterCenter(skia::resolvedImageFilter(original),
                     {0.75f, 0.75f, 0.75f, 1});
  original.slot("tint", Paint::solid({1, 0, 0, 1}));
  EXPECT_NE(original, held);
  expectFilterCenter(skia::resolvedImageFilter(original), {0.75f, 0, 0, 1});
  EXPECT_EQ(heldImage, skia::resolvedImageFilter(held));
  expectFilterCenter(heldImage, {0.25f, 0.25f, 0.25f, 1});

  auto gain = sigil::motion::animatable(0.25f);
  Filter live = held;
  live.bind("gain", gain);
  const Filter shared = live;
  gain = 0.5f;
  EXPECT_TRUE(live.isRunning());
  EXPECT_TRUE(shared.isRunning());
  expectFilterCenter(skia::resolvedImageFilter(live), {0.5f, 0.5f, 0.5f, 1});
  expectFilterCenter(skia::resolvedImageFilter(shared), {0.5f, 0.5f, 0.5f, 1});
  expectFilterCenter(heldImage, {0.25f, 0.25f, 0.25f, 1});
}
