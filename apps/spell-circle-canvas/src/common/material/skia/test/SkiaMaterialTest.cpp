/** @file
 * A material built up by composition, lowered to one Skia paint: a colour
 * base with a screened layer, a layer through a mask, a gradient base, and
 * the same build twice compared equal as paints.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkFont.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/utils/SkCustomTypeface.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/values/Animatable.h>

#include <algorithm>
#include <array>
#include <utility>

#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::render;

namespace {

SkColor centre(const Material& material) {
  const Paint paint = skia::paint(material);
  if (paint.isSolid()) return skia::toSkColor(paint.solidColor()).toSkColor();
  const SkBitmap pixels = render(skia::shader(paint, {.resolution = {4, 4}}));
  return pixels.getColor(2, 2);
}

enum class Geometry { Fill, Stroke, Glyph };

SkBitmap drawnFloat(const Paint& source, Geometry geometry = Geometry::Fill,
                    FrameData frame = {}) {
  SkBitmap pixels;
  pixels.allocPixels(
      SkImageInfo::Make(16, 16, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  SkCanvas canvas(pixels);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setAntiAlias(true);
  frame.resolution = {16, 16};
  auto shader =
      source.isSolid() ? skia::shader(source) : skia::shader(source, frame);
  EXPECT_TRUE(shader);
  if (!shader) return pixels;
  paint.setShader(std::move(shader));
  if (geometry == Geometry::Fill) {
    canvas.drawPaint(paint);
  } else if (geometry == Geometry::Stroke) {
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.3f);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    canvas.drawPath(SkPathBuilder()
                        .moveTo(2.2f, 13.1f)
                        .cubicTo(4.4f, 1.2f, 9.6f, 1.8f, 13.4f, 12.3f)
                        .detach(),
                    paint);
  } else {
    static const sk_sp<SkTypeface> face = [] {
      SkCustomTypefaceBuilder builder;
      builder.setGlyph(1, 0.9f,
                       SkPathBuilder()
                           .addRect(SkRect::MakeLTRB(0.1f, -0.8f, 0.3f, 0))
                           .addRect(SkRect::MakeLTRB(0.6f, -0.8f, 0.8f, 0))
                           .addRect(SkRect::MakeLTRB(0.3f, -0.5f, 0.6f, -0.3f))
                           .detach());
      return builder.detach();
    }();
    EXPECT_TRUE(face);
    SkFont font(face, 16);
    font.setEdging(SkFont::Edging::kAntiAlias);
    const std::array<SkGlyphID, 1> glyphs{1};
    const std::array<SkPoint, 1> positions{SkPoint::Make(0, 0)};
    canvas.drawGlyphs({glyphs.data(), glyphs.size()},
                      {positions.data(), positions.size()}, {1.3f, 14.4f}, font,
                      paint);
  }
  return pixels;
}

std::array<float, 4> premul(const SkBitmap& pixels, int x, int y) {
  const auto* pixel = static_cast<const float*>(pixels.pixmap().addr(x, y));
  return {pixel[0], pixel[1], pixel[2], pixel[3]};
}

void expectPremul(const SkBitmap& pixels, int x, int y,
                  const std::array<float, 4>& expected) {
  const auto actual = premul(pixels, x, y);
  for (size_t channel = 0; channel < actual.size(); ++channel)
    EXPECT_NEAR(actual[channel], expected[channel], 0.00002f)
        << "pixel " << x << "," << y << ", channel " << channel;
}

Lighting ambientLighting() { return Light{.intensity = 0.0f, .ambient = 1.0f}; }

}  // namespace

TEST(SkiaMaterial, ALayerBlendsOverItsBaseAtItsOpacity) {
  const Material flat = Color{1, 0, 0, 1};
  EXPECT_EQ(SK_ColorRED, centre(flat));
  const Material covered = from(Color{1, 0, 0, 1}).layer(Color{0, 0, 1, 1});
  EXPECT_EQ(SK_ColorBLUE, centre(covered));
  const Material half =
      from(Color{1, 0, 0, 1}).layer(Color{0, 0, 1, 1}, {.opacity = 0.5f});
  const SkColor mixed = centre(half);
  EXPECT_NEAR(128, SkColorGetR(mixed), 2);
  EXPECT_NEAR(128, SkColorGetB(mixed), 2);
}

TEST(SkiaMaterial, AMaskSaysWhereALayerApplies) {
  const Material nowhere =
      from(Color{1, 0, 0, 1})
          .layer(Color{0, 0, 1, 1},
                 {.mask = Mask{.source = Color{0, 0, 0, 0}}});
  EXPECT_EQ(SK_ColorRED, centre(nowhere));
  const Material inverted =
      from(Color{1, 0, 0, 1})
          .layer(Color{0, 0, 1, 1},
                 {.mask = Mask{.source = Color{0, 0, 0, 0}, .invert = true}});
  EXPECT_EQ(SK_ColorBLUE, centre(inverted));
}

TEST(SkiaMaterial, AGradientIsABaseAndComparesByValue) {
  const auto build = [] {
    return from(linearGradient({0, 0}, {1, 0},
                               {Color{0, 0, 0, 1}, Color{1, 1, 1, 1}}))
        .layer(Color{1, 1, 1, 1}, {.blend = BlendMode::Multiply});
  };
  EXPECT_EQ(build(), build());
  EXPECT_EQ(skia::paint(build()), skia::paint(build()));
  EXPECT_NE(build(), Material(Color{1, 1, 1, 1}));
}

TEST(SkiaMaterial, EffectsAreReadBackAndPartOfTheValue) {
  const Material raised =
      from(Color{0.5f, 0.5f, 0.5f, 1})
          .effects(
              Filter::shadow({0, 0, 0, 0.5f}, {.blur = 4, .offset = {0, 2}})
                  .then(Filter::stroke({1, 1, 1, 1}, {.width = 2})));
  ASSERT_NE(nullptr, raised.effects());
  ASSERT_EQ(2u, raised.effects()->coverage().size());
  EXPECT_EQ(CoverageEffect::Kind::Stroke, raised.effects()->coverage()[1].kind);
  EXPECT_TRUE(raised.effects()->withoutCoverage().isNone());
  EXPECT_NE(raised, raised.base());
}

TEST(SkiaMaterial, TheDesignatedFormIsTheChainInOnePairOfBraces) {
  const Filter shadow = Filter::shadow(Color{0, 0, 0, 0.5f}, {.blur = 4});
  const Material chained =
      from(Color{0.2f, 0.3f, 0.4f, 1})
          .layer(Color{1, 1, 1, 0.5f}, {.blend = BlendMode::Screen})
          .surface({.metallic = 1.0f})
          .effects(shadow);
  const Material designated =
      from({.base = Color{0.2f, 0.3f, 0.4f, 1},
            .layers = {{Color{1, 1, 1, 0.5f}, {.blend = BlendMode::Screen}}},
            .surface = SurfaceOptions{.metallic = 1.0f},
            .effects = shadow});
  EXPECT_EQ(chained, designated);
  ASSERT_TRUE(designated.effects());
  EXPECT_EQ(shadow, *designated.effects());
  EXPECT_FALSE(from({.base = Color{1, 0, 0, 1}}).effects())
      << "no effects stated, none held";
}

TEST(SkiaMaterial, ABlurMapIsWrittenAsAMaterial) {
  const Material falloff =
      linearGradient({0, 0}, {1, 0}, {{0, {0, 0, 0, 1}}, {1, {1, 1, 1, 1}}});
  EXPECT_EQ(Filter::blur(skia::paint(falloff), 12.0f),
            Filter::blur(falloff, 12.0f));
}

TEST(SkiaMaterial, AlphaCutoffRetainsItsBoundaryAndPremultipliedAlpha) {
  const Material plain = Color{0.75f, 0.5f, 0.25f, 0.5f};
  const Material zero = from(plain).surface({.alphaCutoff = 0});
  EXPECT_EQ(skia::paint(plain), skia::paint(zero));
  EXPECT_TRUE(skia::paint(zero).isSolid());
  for (const bool lit : {false, true}) {
    const auto draw = [&](float cutoff) {
      const Material material = from(plain).surface({.alphaCutoff = cutoff});
      return drawnFloat(lit ? skia::lit(material, ambientLighting())
                            : skia::paint(material));
    };
    expectPremul(draw(0.5f), 8, 8, {0.375f, 0.25f, 0.125f, 0.5f});
    expectPremul(draw(0.25f), 8, 8, {0.375f, 0.25f, 0.125f, 0.5f});
    expectPremul(draw(0.5001f), 8, 8, {0, 0, 0, 0});
  }
}

TEST(SkiaMaterial, AlphaCutoffFollowsTheCompleteMaskedLayerStack) {
  const Material stack =
      from(Color{0, 0, 1, 0.25f})
          .layer(
              Color{1, 0, 0, 0.5f},
              {.opacity = 0.5f, .mask = Mask{.source = Color{0, 0, 0, 0.5f}}});
  // SrcOver gives alpha .625; mask and layer opacity mix one quarter of it
  // into the base, whose alpha is .25.
  for (const bool lit : {false, true}) {
    const auto draw = [&](float cutoff) {
      const Material material = from(stack).surface({.alphaCutoff = cutoff});
      return drawnFloat(lit ? skia::lit(material, ambientLighting())
                            : skia::paint(material));
    };
    expectPremul(draw(0.34375f), 8, 8, {0.125f, 0, 0.21875f, 0.34375f});
    expectPremul(draw(0.344f), 8, 8, {0, 0, 0, 0});
  }
}

TEST(SkiaMaterial, AlphaCutoffUsesImageAlphaInLitAndFlatPaints) {
  SkBitmap imagePixels;
  imagePixels.allocPixels(
      SkImageInfo::Make(16, 16, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) {
      const float alpha = float(x / 4) * 0.25f;
      auto* pixel =
          static_cast<float*>(imagePixels.pixmap().writable_addr(x, y));
      const std::array<float, 4> rgba{0.75f * alpha, 0.5f * alpha,
                                      0.25f * alpha, alpha};
      std::copy(rgba.begin(), rgba.end(), pixel);
    }
  imagePixels.setImmutable();
  const Material base = image(imagePixels.asImage());
  for (int mode = 0; mode < 3; ++mode) {
    const Material material =
        from(base).surface({.alphaCutoff = 0.5f, .unlit = mode == 2});
    const SkBitmap pixels =
        drawnFloat(mode == 0 ? skia::paint(material)
                             : skia::lit(material, ambientLighting()));
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x) {
        const float alpha = x < 8 ? 0.0f : float(x / 4) * 0.25f;
        expectPremul(pixels, x, y,
                     {0.75f * alpha, 0.5f * alpha, 0.25f * alpha, alpha});
      }
  }
}

TEST(SkiaMaterial, AlphaCutoffPrecedesStrokeAndGlyphCoverage) {
  SkBitmap imagePixels;
  imagePixels.allocPixels(
      SkImageInfo::Make(16, 16, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  imagePixels.eraseColor(SkColor4f{0.75f, 0.5f, 0.25f, 0.5f});
  imagePixels.setImmutable();
  const Material base = image(imagePixels.asImage());
  for (const Geometry geometry : {Geometry::Stroke, Geometry::Glyph}) {
    for (const bool lit : {false, true}) {
      const Material unchanged = from(base).surface({.alphaCutoff = 0});
      const Material retained = from(base).surface({.alphaCutoff = 0.5f});
      const Material removed = from(base).surface({.alphaCutoff = 0.5001f});
      const SkBitmap reference =
          drawnFloat(lit ? skia::lit(unchanged, ambientLighting())
                         : skia::paint(unchanged),
                     geometry);
      const SkBitmap kept = drawnFloat(
          lit ? skia::lit(retained, ambientLighting()) : skia::paint(retained),
          geometry);
      const SkBitmap cut = drawnFloat(
          lit ? skia::lit(removed, ambientLighting()) : skia::paint(removed),
          geometry);
      int partial = 0, full = 0;
      for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
          const auto expected = premul(reference, x, y);
          partial += expected[3] > 0.0f && expected[3] < 0.5f;
          full += expected[3] == 0.5f;
          expectPremul(kept, x, y, expected);
          expectPremul(cut, x, y, {0, 0, 0, 0});
        }
      EXPECT_GT(partial, 0);
      EXPECT_GT(full, 0);
    }
  }
}

TEST(SkiaMaterial, AlphaCutoffFollowsLiveColorStackAlpha) {
  struct Parameters {
    float alpha;
  };
  auto alpha = sigil::motion::animatable(0.25f);
  Material material = shader(
      "half4 main(float2 p) { return half4(float3(.75,.5,.25)*alpha,alpha); }",
      Parameters{0.25f});
  material.bind("alpha", alpha).surface({.alphaCutoff = 0.5f});
  for (const bool lit : {false, true}) {
    alpha = 0.25f;
    const Paint paint =
        lit ? skia::lit(material, ambientLighting()) : skia::paint(material);
    ASSERT_TRUE(paint.isRunning());
    expectPremul(drawnFloat(paint), 8, 8, {0, 0, 0, 0});
    alpha = 0.5f;
    expectPremul(drawnFloat(paint), 8, 8, {0.375f, 0.25f, 0.125f, 0.5f});
    alpha = 0.75f;
    expectPremul(drawnFloat(paint), 8, 8, {0.5625f, 0.375f, 0.1875f, 0.75f});
  }
}

TEST(SkiaMaterial, AlphaCutoffRetainsOuterLayerStrengthAndCullReserve) {
  Paint source = skia::paint(skia::shader(Paint::solid({1, 0, 0, 0.5f})));
  source.amount(0.25f).bleed(7);
  const Paint kept =
      skia::paint(skia::base(source).surface({.alphaCutoff = 0.25f}));
  EXPECT_FLOAT_EQ(kept.bleed(), 7);
  const auto overBlue = [](const Paint& layer) {
    return drawnFloat(
        Paint::blend({{Paint::solid({0, 0, 1, 1}), BlendMode::Normal},
                      {layer, BlendMode::Source}}));
  };
  expectPremul(overBlue(kept), 8, 8, {0.125f, 0, 0.75f, 0.875f});
  const Paint removed =
      skia::paint(skia::base(source).surface({.alphaCutoff = 0.75f}));
  EXPECT_FLOAT_EQ(removed.bleed(), 7);
  expectPremul(overBlue(removed), 8, 8, {0, 0, 0.75f, 0.75f});

  Paint solid = Paint::solid({1, 0, 0, 0.25f});
  solid.amount(0.25f).bleed(9).worldSpace();
  const Paint removedSolid =
      skia::paint(skia::base(solid).surface({.alphaCutoff = 0.5f}));
  EXPECT_TRUE(removedSolid.isSolid());
  EXPECT_TRUE(removedSolid.usesWorldSpace());
  EXPECT_FLOAT_EQ(removedSolid.bleed(), 9);
  expectPremul(overBlue(removedSolid), 8, 8, {0, 0, 0.75f, 0.75f});

  const Material layered =
      from(Color{0, 0, 1, 1})
          .layer(skia::base(source).surface({.alphaCutoff = 0.25f}),
                 {.opacity = 0.5f});
  expectPremul(drawnFloat(skia::paint(layered)), 8, 8, {0.25f, 0, 0.75f, 1});
}

TEST(SkiaMaterial, AlphaCutoffKeepsRootAnchoringTransparentWhileMoving) {
  Paint source = Paint::linearGradient(
      {0, 0}, {64, 0},
      {Color{0, 0.5f, 0.25f, 0.5f}, Color{1, 0.5f, 0.25f, 0.5f}},
      {.units = GradientUnits::Pixels});
  source.worldSpace();
  const Material base = skia::base(source).worldSpace();
  const Paint plain = skia::paint(base);
  const Paint cutout = skia::paint(from(base).surface({.alphaCutoff = 0.25f}));
  EXPECT_TRUE(cutout.usesWorldSpace());
  for (const float translation : {8.f, 24.f, 8.f}) {
    FrameData frame{.rootResolution = {64, 16}};
    frame.world[2][0] = translation;
    const SkBitmap expected = drawnFloat(plain, Geometry::Fill, frame);
    const SkBitmap actual = drawnFloat(cutout, Geometry::Fill, frame);
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x)
        expectPremul(actual, x, y, premul(expected, x, y));
    expectPremul(actual, 8, 8,
                 {0.5f * (8.5f + translation) / 64.f, 0.25f, 0.125f, 0.5f});
  }
}

TEST(SkiaMaterial, AlphaCutoffRetainsLiveImageOffsetsAndAnchoring) {
  SkBitmap imagePixels;
  imagePixels.allocPixels(
      SkImageInfo::Make(64, 16, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 64; ++x) {
      auto* pixel =
          static_cast<float*>(imagePixels.pixmap().writable_addr(x, y));
      const std::array<float, 4> rgba{float(x) / 128.f, 0.25f, 0.125f, 0.5f};
      std::copy(rgba.begin(), rgba.end(), pixel);
    }
  imagePixels.setImmutable();
  auto offset = sigil::motion::animatable(0.f);
  Paint source = skia::image(imagePixels.asImage());
  source.offset(offset, std::nullopt).worldSpace();
  const Material base = skia::base(source).worldSpace();
  const Paint plain = skia::paint(base);
  const Paint cutout = skia::paint(from(base).surface({.alphaCutoff = 0.25f}));
  EXPECT_TRUE(cutout.isRunning());
  EXPECT_TRUE(cutout.usesWorldSpace());
  FrameData frame{.rootResolution = {64, 16}};
  frame.world[2][0] = 8;
  std::array<float, 3> reds{};
  for (size_t step = 0; step < reds.size(); ++step) {
    offset = float(step) * 2;
    const SkBitmap expected = drawnFloat(plain, Geometry::Fill, frame);
    const SkBitmap actual = drawnFloat(cutout, Geometry::Fill, frame);
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x)
        expectPremul(actual, x, y, premul(expected, x, y));
    reds[step] = premul(actual, 8, 8)[0];
  }
  EXPECT_GT(reds[0] - reds[1], 0.01f);
  EXPECT_GT(reds[1] - reds[2], 0.01f);
}

TEST(SkiaMaterial, AlphaCutoffPreservesImageFitAndRootFlagCopies) {
  SkBitmap imagePixels;
  imagePixels.allocPixels(
      SkImageInfo::Make(64, 16, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 64; ++x) {
      auto* pixel =
          static_cast<float*>(imagePixels.pixmap().writable_addr(x, y));
      const std::array<float, 4> rgba{float(x) / 128.f, float(y) / 32.f, 0.125f,
                                      0.5f};
      std::copy(rgba.begin(), rgba.end(), pixel);
    }
  imagePixels.setImmutable();
  for (const Fit fit : {Fit::Stretch, Fit::Cover, Fit::Contain}) {
    SCOPED_TRACE(static_cast<int>(fit));
    auto offset = sigil::motion::animatable(0.f);
    Paint source = skia::image(imagePixels.asImage());
    source.fit(fit).offset(offset, std::nullopt).worldSpace();
    const Material base = skia::base(source).worldSpace();
    const Paint original = skia::paint(base);
    const Paint retained =
        skia::paint(from(base).surface({.alphaCutoff = 0.25f}));
    ASSERT_TRUE(retained.geometryDependent());
    ASSERT_TRUE(retained.isRunning());
    FrameData frame{.rootResolution = {80, 48}};
    frame.world[2][0] = 2;
    frame.world[2][1] = 3;
    const auto compare = [&](const Paint& actual, const Paint& expected) {
      const SkBitmap reference = drawnFloat(expected, Geometry::Fill, frame);
      const SkBitmap pixels = drawnFloat(actual, Geometry::Fill, frame);
      for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
          expectPremul(pixels, x, y, premul(reference, x, y));
    };
    for (const float pan : {0.f, 2.f, 0.f}) {
      offset = pan;
      compare(retained, original);
    }
    Paint local = retained;
    Paint localReference = original;
    local.worldSpace(false);
    localReference.worldSpace(false);
    EXPECT_NE(local, retained);
    EXPECT_FALSE(local.usesWorldSpace());
    EXPECT_TRUE(retained.usesWorldSpace());
    compare(local, localReference);
    compare(retained, original);
    local.worldSpace();
    EXPECT_TRUE(local.usesWorldSpace());
    compare(local, original);
    compare(retained, original);
  }
}

TEST(SkiaMaterial, AlphaCutoffPreservesNestedAnchorsAndFrameInputs) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
uniform shader uSource;
uniform float2 uResolution;
uniform float3x3 uLocalToSample;
half4 main(float2 p) {
  half4 source = uSource.eval(p);
  float2 step = (uLocalToSample * float3(1, 0, 0)).xy;
  return half4(source.r, step.x / 64.0, uResolution.x / 128.0, .5);
}
)"));
  ASSERT_TRUE(effect) << error.c_str();
  Paint child = Paint::linearGradient(
      {0, 0}, {256, 0}, {Color{0, 0, 0, 0.5f}, Color{1, 0, 0, 0.5f}},
      {.units = GradientUnits::Pixels});
  child.worldSpace();
  Paint source = skia::sksl(effect);
  source.slot("uSource", child).worldSpace();
  const Paint original = skia::paint(skia::base(source));
  const Paint retained =
      skia::paint(skia::base(source).surface({.alphaCutoff = .25f}));
  const auto compare = [&](const Paint& actual, const Paint& expected,
                           const FrameData& frame) {
    const SkBitmap reference = drawnFloat(expected, Geometry::Fill, frame);
    const SkBitmap pixels = drawnFloat(actual, Geometry::Fill, frame);
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x)
        expectPremul(pixels, x, y, premul(reference, x, y));
  };
  FrameData frame{.rootResolution = {64, 16}};
  frame.world[0][0] = 2;
  frame.world[2][0] = 8;
  compare(retained, original, frame);
  expectPremul(drawnFloat(retained, Geometry::Fill, frame), 8, 8,
               {0.5f * 58.f / 256.f, 2.f / 64.f, 0.5f, 0.5f});
  frame.world[2][0] = 12;
  compare(retained, original, frame);
  Paint local = retained;
  Paint localReference = original;
  local.worldSpace(false);
  localReference.worldSpace(false);
  EXPECT_TRUE(local.usesWorldSpace());
  compare(local, localReference, frame);
  expectPremul(drawnFloat(local, Geometry::Fill, frame), 8, 8,
               {0.5f * 29.f / 256.f, 1.f / 64.f, 0.125f, 0.5f});
  compare(retained, original, frame);
  local.worldSpace();
  compare(local, original, frame);
}
