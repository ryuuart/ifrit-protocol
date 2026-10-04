#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/Terms.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilmotion/values/Animatable.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "../Environment.h"
#include "../PaintInternal.h"

#if defined(__APPLE__)
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include "GraphiteReadback.h"
#endif

using namespace sigil::material;

namespace {

SkBitmap draw(const sk_sp<SkShader>& shader, int width = 512, int height = 1) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(width, height, kRGBA_F32_SkColorType,
                                       kPremul_SkAlphaType));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setShader(shader);
  canvas.drawPaint(paint);
  return bitmap;
}

sk_sp<SkRuntimeEffect> compile(std::string source) {
  auto [effect, error] =
      SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
  EXPECT_TRUE(effect) << error.c_str();
  return effect;
}

SkBitmap bandProfile(const sk_sp<SkShader>& atlas, float level, float v) {
  const auto effect = compile(R"(
    uniform shader atlas;
    uniform float level;
    uniform float latitude;
    half4 main(float2 p) {
      float3 band;
      if (level < 1.5) band = float3(256,128,0);
      else if (level < 2.5) band = float3(128,64,130);
      else if (level < 3.5) band = float3(64,32,196);
      else band = float3(32,16,230+(level-4)*18);
      return atlas.eval(float2(p.x / 512.0 * band.x + 1.0,
                              latitude * band.y + band.z + 1.0));
    })");
  if (!effect || !atlas) return {};
  SkRuntimeShaderBuilder builder(effect);
  builder.child("atlas") = atlas;
  builder.uniform("level") = level;
  builder.uniform("latitude") = v;
  return draw(builder.makeShader());
}

int halfPeakWidth(const SkBitmap& bitmap) {
  float peak = 0;
  for (int x = 0; x < bitmap.width(); ++x)
    peak = std::max(peak, bitmap.getColor4f(x, 0).fR);
  int width = 0;
  for (int x = 0; x < bitmap.width(); ++x)
    width += bitmap.getColor4f(x, 0).fR >= peak * .5f;
  return peak > 0 ? width : 0;
}

Lighting calibrated(Environment around) {
  return Lighting(Light{.intensity = 0, .ambient = 0}, std::move(around));
}

SkColor4f sample(const Paint& paint, FrameData frame = {}) {
  frame.resolution = {8, 8};
  return draw(skia::shader(paint, frame), 8, 8).getColor4f(4, 4);
}

Material metal(float roughness, float coat = 0) {
  return from(Color{1, 1, 1, 1})
      .surface({.metallic = 1.f, .roughness = roughness, .clearcoat = coat});
}

struct CountedPicture {
  std::shared_ptr<int> reads;
  sk_sp<SkImage> picture;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    ++*reads;
    sigil::media::Frame frame;
    frame.image = picture;
    return frame;
  }
  bool isRunning() const { return false; }
  SkISize size() const { return picture->dimensions(); }
  bool operator==(const CountedPicture& other) const {
    return reads == other.reads;
  }
};

}  // namespace

TEST(SkiaEnvironment, EverySphericalBandPreservesAConstantHdrSky) {
  skia::EnvironmentPreparation preparation;
  const auto source = SkShaders::Color(SkColor4f{4, 2, 1, 1}, nullptr);
  const auto atlas = preparation.shader(source, {64, 32}, nullptr);
  ASSERT_TRUE(atlas);
  for (float level = 1; level <= 9; ++level)
    for (float latitude : {0.f, .5f, 1.f}) {
      const auto profile = bandProfile(atlas, level, latitude);
      for (int x : {0, 1, 127, 255, 510, 511}) {
        const SkColor4f pixel = profile.getColor4f(x, 0);
        EXPECT_NEAR(pixel.fR, 4, .0001f);
        EXPECT_NEAR(pixel.fG, 2, .0001f);
        EXPECT_NEAR(pixel.fB, 1, .0001f);
        EXPECT_FLOAT_EQ(pixel.fA, 1);
      }
    }
}

TEST(SkiaEnvironment, PanoramaCoverageKeepsItsPremultipliedContribution) {
  skia::EnvironmentPreparation preparation;
  const auto source = SkShaders::Color(SkColor4f{8, 4, 2, .25f}, nullptr);
  const auto atlas = preparation.shader(source, {0, 0}, nullptr);
  ASSERT_TRUE(atlas);
  for (float level : {1.f, 4.f, 8.f, 9.f}) {
    const auto pixel = bandProfile(atlas, level, .5f).getColor4f(256, 0);
    EXPECT_NEAR(pixel.fR, 2, .0001f);
    EXPECT_NEAR(pixel.fG, 1, .0001f);
    EXPECT_NEAR(pixel.fB, .5f, .0001f);
    EXPECT_FLOAT_EQ(pixel.fA, 1);
  }
}

TEST(SkiaEnvironment, FailedInputsClearTheAtlasAndRemainRetryable) {
  skia::EnvironmentPreparation preparation;
  const auto source = SkShaders::Color(SkColor4f{2, 1, .5f, 1}, nullptr);
  const auto first = preparation.shader(source, {64, 32}, nullptr);
  ASSERT_TRUE(first);
  EXPECT_EQ(preparation.shader(source, {64, 32}, nullptr), first);
  for (SkSize invalid : {SkSize{64, -1}, SkSize{-1, 32},
                         SkSize{64, std::numeric_limits<float>::infinity()},
                         SkSize{std::numeric_limits<float>::quiet_NaN(), 32}}) {
    EXPECT_FALSE(preparation.shader(source, invalid, nullptr));
    EXPECT_FALSE(preparation.retainedShader());
    const auto retried = preparation.shader(source, {64, 32}, nullptr);
    EXPECT_TRUE(retried);
    EXPECT_NE(retried, first);
  }
  EXPECT_FALSE(preparation.shader(nullptr, {64, 32}, nullptr));
  EXPECT_FALSE(preparation.retainedShader());
  EXPECT_TRUE(preparation.shader(source, {64, 32}, nullptr));
}

TEST(SkiaEnvironment, ASeamStripSpreadsWithRoughnessInsteadOfOnlyDimming) {
  const Paint source = skia::paint(shader(R"(
    half4 main(float2 p) {
      float u = p.x / 64.0;
      float distance = min(u, 1.0-u);
      return half4(distance < .025 ? 8.0 : 0.0, 0, 0, 1);
    })"));
  skia::EnvironmentPreparation preparation;
  const auto atlas =
      preparation.shader(skia::staticShader(source), {64, 32}, nullptr);
  ASSERT_TRUE(atlas);
  const auto sharp = bandProfile(atlas, 1, .5f);
  const auto satin = bandProfile(atlas, 4, .5f);
  const auto rough = bandProfile(atlas, 8, .5f);
  EXPECT_GT(halfPeakWidth(satin), halfPeakWidth(sharp) * 2);
  EXPECT_GT(halfPeakWidth(rough), halfPeakWidth(satin));
  for (const auto* profile : {&sharp, &satin, &rough}) {
    const float left = profile->getColor4f(0, 0).fR;
    const float right = profile->getColor4f(511, 0).fR;
    EXPECT_GT(left, 0);
    EXPECT_NEAR(left, right, std::max(.02f, left * .04f));
  }
  const auto diffuse = bandProfile(atlas, 9, .5f);
  EXPECT_GT(diffuse.getColor4f(0, 0).fR, 0);
  EXPECT_FLOAT_EQ(diffuse.getColor4f(0, 0).fG, 0);
}

TEST(SkiaEnvironment, APolarSourceRemainsFiniteAndSpreadsAcrossLatitude) {
  const auto source = skia::staticShader(skia::paint(shader(R"(
    half4 main(float2 p) {
      return half4(p.y < 2.0 ? 6.0 : 0.0, 0, 0, 1);
    })")));
  skia::EnvironmentPreparation preparation;
  const auto atlas = preparation.shader(source, {64, 32}, nullptr);
  ASSERT_TRUE(atlas);
  const auto broad = bandProfile(atlas, 8, .25f);
  const auto diffuse = bandProfile(atlas, 9, .25f);
  const auto sharp = bandProfile(atlas, 1, .25f);
  EXPECT_GT(broad.getColor4f(256, 0).fR, sharp.getColor4f(256, 0).fR + .01f);
  for (int x = 0; x < broad.width(); ++x)
    EXPECT_EQ(broad.getColor4f(x, 0), diffuse.getColor4f(x, 0));
  for (float level : {1.f, 4.f, 8.f, 9.f})
    for (float latitude : {0.f, .0001f, .9999f, 1.f}) {
      const auto row = bandProfile(atlas, level, latitude);
      for (int x = 0; x < row.width(); ++x) {
        const auto pixel = row.getColor4f(x, 0);
        EXPECT_TRUE(std::isfinite(pixel.fR));
        EXPECT_GE(pixel.fR, 0);
        EXPECT_LE(pixel.fR, 6.001f);
        EXPECT_FLOAT_EQ(pixel.fA, 1);
      }
    }
}

TEST(SkiaEnvironment, SurfaceCopiesRetainOneAtlasAcrossLookupOnlyChanges) {
  auto reads = std::make_shared<int>(0);
  SkBitmap pixels;
  pixels.allocN32Pixels(64, 32, true);
  pixels.eraseColor(SkColorSetRGB(128, 64, 32));
  pixels.setImmutable();
  auto rotation = sigil::motion::animatable(0.f);
  Environment around = environment(
      image(sigil::media::PixelSource(CountedPicture{reads, pixels.asImage()})),
      {.rotation = rotation, .size = {64, 32}});
  skia::LitSurface surface(metal(.45f));
  const skia::LitSurface copied = surface;
  Paint first = surface.under(calibrated(around));
  const auto preparation = skia::PaintAccess::environmentPreparation(first);
  ASSERT_TRUE(preparation);
  EXPECT_TRUE(first.geometryDependent());
  EXPECT_FALSE(preparation->retainedShader());
  const int readOnce = *reads;
  ASSERT_GT(readOnce, 0);
  sample(first);
  const auto retained = preparation->retainedShader();
  ASSERT_TRUE(retained);
  for (int i = 0; i < 8; ++i) {
    rotation = float(i * 27);
    around.options.intensity = .5f + i * .1f;
    Lighting lighting = calibrated(around);
    lighting.lights.front().kind = LightKind::Point;
    lighting.lights.front().position = {float(i), 4, 20};
    Paint changed = copied.under(lighting);
    EXPECT_EQ(skia::PaintAccess::environmentPreparation(changed), preparation);
    EXPECT_EQ(preparation->retainedShader(), retained);
    const auto result = sample(changed);
    EXPECT_GT(result.fR, 0);
    EXPECT_EQ(preparation->retainedShader(), retained);
    EXPECT_EQ(*reads, readOnce);
  }
}

TEST(SkiaEnvironment, IndependentLoweringsKeepTheSolidPanoramaDescription) {
  const auto surface = metal(.45f);
  const auto lighting = calibrated(environment(from(Color{4, 2, 1, 1})));
  const auto first = skia::lit(surface, lighting);
  const auto second = skia::lit(surface, lighting);
  EXPECT_EQ(first, second);
  EXPECT_EQ(sample(first), sample(second));
  EXPECT_EQ(first, second);
}

TEST(SkiaEnvironment, HeldLayeredSourcesReusePreparationAndLiveEditsReplaceIt) {
  struct Parameters {
    Color uTint;
  };
  auto tint = sigil::motion::animatable(Color{4, 0, 0, 1});
  const Material overlay =
      shader("half4 main(float2 p) { return half4(uTint); }", Parameters{})
          .bind("uTint", tint);
  const Material source =
      from(Color{0, .2f, 0, 1}).layer(overlay, {.opacity = .5f});
  const auto lighting = calibrated(environment(source, {.size = {64, 32}}));
  skia::LitSurface surface(metal(.5f));
  const auto copied = surface;
  Paint paint = surface.under(lighting);
  const auto red = sample(paint);
  const auto preparation = skia::PaintAccess::environmentPreparation(paint);
  ASSERT_TRUE(preparation);
  const auto retained = preparation->retainedShader();
  ASSERT_TRUE(retained);
  EXPECT_GT(red.fR, 1);
  EXPECT_FLOAT_EQ(red.fB, 0);
  for (int i = 1; i <= 4; ++i) {
    FrameData frame{.seconds = double(i)};
    EXPECT_EQ(sample(paint, frame), red);
    EXPECT_EQ(preparation->retainedShader(), retained);
    const Paint described = copied.under(lighting);
    EXPECT_EQ(sample(described, frame), red);
    EXPECT_EQ(preparation->retainedShader(), retained);
  }
  tint = Color{0, 0, 4, 1};
  const auto blue = sample(paint);
  EXPECT_FLOAT_EQ(blue.fR, 0);
  EXPECT_GT(blue.fB, 1);
  EXPECT_FLOAT_EQ(blue.fG, red.fG);
  EXPECT_NE(preparation->retainedShader(), retained);
  const auto edited = preparation->retainedShader();
  EXPECT_EQ(sample(paint), blue);
  EXPECT_EQ(preparation->retainedShader(), edited);
}

TEST(SkiaEnvironment, LiveSourceColorAndFrameInputsReplacePreparedPixels) {
  struct Parameters {
    Color tint;
  };
  auto color = sigil::motion::animatable(Color{4, 0, 0, 1});
  const Material source = shader(R"(
    half4 main(float2 p) {
      return half4(tint.rgb * (1.0+uTime*.1+uResolution.x*.01+
                               uContentScale*.01+uWorld[2].x*.01),1);
    })",
                                 Parameters{})
                              .bind("tint", color);
  const Environment around = environment(source, {.size = {64, 32}});
  skia::LitSurface surface(metal(1));
  Paint paint = surface.under(calibrated(around));
  EXPECT_TRUE(paint.isRunning());
  FrameData frame;
  const auto red = sample(paint, frame);
  const auto preparation = skia::PaintAccess::environmentPreparation(paint);
  const auto initial = preparation->retainedShader();
  ASSERT_TRUE(initial);
  EXPECT_GT(red.fR, 1);
  EXPECT_FLOAT_EQ(red.fB, 0);
  color = Color{0, 0, 4, 1};
  const auto blue = sample(paint, frame);
  EXPECT_GT(blue.fB, 1);
  EXPECT_FLOAT_EQ(blue.fR, 0);
  EXPECT_NE(preparation->retainedShader(), initial);
  const auto held = preparation->retainedShader();
  sample(paint, frame);
  EXPECT_EQ(preparation->retainedShader(), held);
  frame.seconds = 3;
  const auto later = sample(paint, frame);
  EXPECT_GT(later.fB, blue.fB);
  EXPECT_NE(preparation->retainedShader(), held);
  auto previous = preparation->retainedShader();
  frame.resolution = {16, 8};
  const auto resized = draw(skia::shader(paint, frame), 8, 8).getColor4f(4, 4);
  EXPECT_GT(resized.fB, later.fB);
  EXPECT_NE(preparation->retainedShader(), previous);
  previous = preparation->retainedShader();
  frame.contentScale = 2;
  const auto denser = draw(skia::shader(paint, frame), 8, 8).getColor4f(4, 4);
  EXPECT_GT(denser.fB, resized.fB);
  EXPECT_NE(preparation->retainedShader(), previous);
  previous = preparation->retainedShader();
  frame.world[2].x = 16;
  const auto moved = draw(skia::shader(paint, frame), 8, 8).getColor4f(4, 4);
  EXPECT_GT(moved.fB, denser.fB);
  EXPECT_NE(preparation->retainedShader(), previous);
}

TEST(SkiaEnvironment, UploadedPanoramaSizeOwnsPreparationAndBlockPublication) {
  const Material source = shader(R"(
    half4 main(float2 p) { return half4(.2+p.x*.02,.3+p.y*.02,.1,1); }
  )");
  skia::LitSurface surface(metal(1));
  auto context = [](const Material& image, glm::vec2 size) {
    return calibrated(environment(image, {.size = size}));
  };
  Paint paint = surface.under(context(source, {32, 16}));
  const auto preparation = skia::PaintAccess::environmentPreparation(paint);
  const auto assertSize = [&](glm::vec2 size) {
    const auto actual = sample(paint);
    const auto expected = sample(skia::lit(metal(1), context(source, size)));
    EXPECT_NEAR(actual.fR, expected.fR, .0001f);
    EXPECT_NEAR(actual.fG, expected.fG, .0001f);
    EXPECT_NEAR(actual.fB, expected.fB, .0001f);
    return actual;
  };
  const auto first = assertSize({32, 16});
  paint.set("uEnvironmentSize", std::array<float, 2>{64, 32});
  const auto typed = assertSize({64, 32});
  EXPECT_GT(typed.fR, first.fR);
  paint.set("uEnvironmentSize", std::vector<float>{96, 48});
  assertSize({96, 48});
  const auto held = preparation->retainedShader();
  paint.set("uEnvironmentSize", Color{1, 2, 3, 1});
  assertSize({96, 48});
  EXPECT_EQ(preparation->retainedShader(), held);
  auto block = std::make_shared<UniformBlock>(2);
  block->values()[0] = 48;
  block->values()[1] = 24;
  block->commit();
  paint.bind("uEnvironmentSize", block);
  assertSize({48, 24});
  const auto published = preparation->retainedShader();
  block->values()[0] = 80;
  block->values()[1] = 40;
  assertSize({48, 24});
  EXPECT_EQ(preparation->retainedShader(), published);
  block->commit();
  assertSize({80, 40});
  EXPECT_NE(preparation->retainedShader(), published);
  const auto committed = preparation->retainedShader();
  paint.set("uEnvironmentSize", std::vector<float>{1});
  assertSize({80, 40});
  EXPECT_EQ(preparation->retainedShader(), committed);
  Paint invalid = surface.under(context(source, {32, 16}));
  for (std::array<float, 2> size :
       {std::array<float, 2>{-1, 16},
        std::array<float, 2>{std::numeric_limits<float>::quiet_NaN(), 16},
        std::array<float, 2>{32, std::numeric_limits<float>::infinity()}}) {
    invalid.set("uEnvironmentSize", size);
    const auto framed = sample(invalid);
    const auto snapshot =
        draw(skia::staticShader(invalid), 8, 8).getColor4f(4, 4);
    EXPECT_EQ(framed, snapshot);
    EXPECT_FLOAT_EQ(framed.fR, 0);
    EXPECT_FLOAT_EQ(framed.fG, 0);
    EXPECT_FLOAT_EQ(framed.fB, 0);
    EXPECT_FLOAT_EQ(framed.fA, 1);
  }
  invalid.set("uEnvironmentSize", std::array<float, 2>{32, 16});
  EXPECT_NEAR(sample(invalid).fR, first.fR, .0001f);
}

TEST(SkiaEnvironment, FullyRoughMetalAndIndependentCoatingRemainReflective) {
  const Lighting lighting =
      calibrated(environment(from(Color{4, 2, 1, 1}), {.size = {1, 1}}));
  const auto rough = sample(skia::lit(metal(1), lighting));
  EXPECT_GT(rough.fR, 1);
  EXPECT_GT(rough.fG, 0);
  EXPECT_GT(rough.fB, 0);
  const auto coated = sample(skia::lit(metal(1, 1), lighting));
  EXPECT_TRUE(std::isfinite(coated.fR));
  EXPECT_NE(coated.fR, rough.fR);
  const Paint paint = skia::lit(metal(.5f), lighting);
  const auto framed = sample(paint);
  const auto frameless = draw(skia::staticShader(paint), 8, 8).getColor4f(4, 4);
  EXPECT_NEAR(framed.fR, frameless.fR, .0001f);
  EXPECT_NEAR(framed.fG, frameless.fG, .0001f);
  EXPECT_NEAR(framed.fB, frameless.fB, .0001f);
}

TEST(SkiaEnvironment, RoughnessMapsSelectTheSameLobesAsScalarChannels) {
  const Material map = shader(R"(
    half4 main(float2 p) {
      float roughness = p.x < 4.0 ? .25 : 1.0;
      return half4(roughness,roughness,roughness,1);
    })");
  const auto source = shader(R"(
    half4 main(float2 p) {
      float u = p.x/64.0;
      return half4(min(u,1.0-u) < .04 ? 4.0 : .05,0,0,1);
    })");
  const Lighting lighting = calibrated(environment(source, {.size = {64, 32}}));
  const Paint mapped = skia::lit(
      from(Color{1, 1, 1, 1}).surface({.metallic = 1.f, .roughness = map}),
      lighting);
  const auto pixels =
      draw(skia::shader(mapped, FrameData{.resolution = {8, 8}}), 8, 8);
  const auto smooth = sample(skia::lit(metal(.25f), lighting));
  const auto rough = sample(skia::lit(metal(1), lighting));
  EXPECT_NEAR(pixels.getColor4f(2, 4).fR, smooth.fR, .0001f);
  EXPECT_NEAR(pixels.getColor4f(6, 4).fR, rough.fR, .0001f);
  EXPECT_GT(smooth.fR, rough.fR);
}

#if defined(__APPLE__)
TEST(SkiaEnvironmentGpu, AtlasPreservesHdrAndDoesNotPingPongThroughSnapshots) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto graphite = sigil::skia::GraphiteContext::create(*device);
  if (!graphite) GTEST_SKIP() << "no Graphite context";
  auto rotation = sigil::motion::animatable(0.f);
  Environment around = environment(from(Color{4, 2, 1, 1}),
                                   {.rotation = rotation, .size = {1, 1}});
  skia::LitSurface surface(metal(1));
  const skia::LitSurface copied = surface;
  const auto initial = surface.under(calibrated(around));
  const auto preparation = skia::PaintAccess::environmentPreparation(initial);
  ASSERT_TRUE(preparation);
  EXPECT_FALSE(preparation->retainedShader(graphite->recorder()));
  sk_sp<SkShader> retained;
  for (int i = 0; i < 4; ++i) {
    rotation = float(i * 25);
    around.options.intensity = 1.f + float(i) * .25f;
    const Paint paint = copied.under(calibrated(around));
    EXPECT_EQ(preparation->retainedShader(graphite->recorder()), retained);
    auto target = SkSurfaces::RenderTarget(
        graphite->recorder(),
        SkImageInfo::Make(8, 8, kRGBA_F16_SkColorType, kPremul_SkAlphaType));
    ASSERT_TRUE(target);
    FrameData frame{.resolution = {8, 8}, .recorder = graphite->recorder()};
    SkPaint ink;
    ink.setShader(skia::shader(paint, frame));
    ASSERT_TRUE(ink.getShader());
    target->getCanvas()->drawPaint(ink);
    const auto pixels =
        sigil::skia::test::readGraphiteSurface(*graphite, target.get());
    ASSERT_FALSE(pixels.isNull());
    const auto value = pixels.getColor4f(4, 4);
    EXPECT_GT(value.fR, 1);
    EXPECT_NEAR(value.fR / value.fG, 2, .01f);
    EXPECT_NEAR(value.fG / value.fB, 2, .01f);
    if (i == 0) retained = preparation->retainedShader(graphite->recorder());
    EXPECT_EQ(preparation->retainedShader(graphite->recorder()), retained);
    const auto held = skia::shader(paint, frame);
    EXPECT_EQ(held, skia::shader(paint, frame));
  }
  sample(initial);
  EXPECT_TRUE(preparation->retainedShader());
  EXPECT_EQ(preparation->retainedShader(graphite->recorder()), retained);
}

TEST(SkiaEnvironmentGpu,
     RasterAndDeviceDrawsReuseAtlasesAndObserveSourceAndRecorderChanges) {
  auto device = sigil::core::hardware::GpuDevice::createOwned();
  if (!device) GTEST_SKIP() << "no GPU device";
  auto graphite = sigil::skia::GraphiteContext::create(*device);
  if (!graphite) GTEST_SKIP() << "no Graphite context";
  const auto deviceSample = [](sigil::skia::GraphiteContext& context,
                               const Paint& paint) {
    auto target = SkSurfaces::RenderTarget(
        context.recorder(),
        SkImageInfo::Make(8, 8, kRGBA_F16_SkColorType, kPremul_SkAlphaType));
    EXPECT_TRUE(target);
    if (!target) return SkColor4f{};
    SkPaint ink;
    ink.setShader(skia::shader(
        paint,
        FrameData{.resolution = {8, 8}, .recorder = context.recorder()}));
    EXPECT_TRUE(ink.getShader());
    if (!ink.getShader()) return SkColor4f{};
    target->getCanvas()->drawPaint(ink);
    const auto pixels =
        sigil::skia::test::readGraphiteSurface(context, target.get());
    EXPECT_FALSE(pixels.isNull());
    return pixels.isNull() ? SkColor4f{} : pixels.getColor4f(4, 4);
  };
  struct Parameters {
    Color tint;
  };
  auto tint = sigil::motion::animatable(Color{4, 0, 0, 1});
  const Material source =
      shader("half4 main(float2 p) { return half4(tint); }", Parameters{})
          .bind("tint", tint);
  skia::LitSurface surface(metal(1));
  const auto copied = surface;
  const Lighting lighting = calibrated(environment(source, {.size = {64, 32}}));
  const Paint paint = surface.under(lighting);
  const auto preparation = skia::PaintAccess::environmentPreparation(paint);
  ASSERT_TRUE(preparation);

  const auto red = sample(paint);
  EXPECT_GT(red.fR, 1);
  EXPECT_FLOAT_EQ(red.fB, 0);
  const auto rasterRed = preparation->retainedShader();
  ASSERT_TRUE(rasterRed);
  EXPECT_NEAR(deviceSample(*graphite, paint).fR, red.fR, .002f);
  const auto deviceRed = preparation->retainedShader(graphite->recorder());
  ASSERT_TRUE(deviceRed);
  for (int i = 0; i < 3; ++i) {
    const Paint described = copied.under(lighting);
    EXPECT_EQ(sample(described), red);
    EXPECT_EQ(preparation->retainedShader(), rasterRed);
    EXPECT_NEAR(deviceSample(*graphite, described).fR, red.fR, .002f);
    EXPECT_EQ(preparation->retainedShader(graphite->recorder()), deviceRed);
  }

  tint = Color{0, 0, 4, 1};
  const auto blue = sample(paint);
  EXPECT_FLOAT_EQ(blue.fR, 0);
  EXPECT_GT(blue.fB, 1);
  const auto rasterBlue = preparation->retainedShader();
  EXPECT_NE(rasterBlue, rasterRed);
  EXPECT_EQ(preparation->retainedShader(graphite->recorder()), deviceRed);
  const auto gpuBlue = deviceSample(*graphite, paint);
  EXPECT_FLOAT_EQ(gpuBlue.fR, 0);
  EXPECT_NEAR(gpuBlue.fB, blue.fB, .002f);
  const auto deviceBlue = preparation->retainedShader(graphite->recorder());
  EXPECT_NE(deviceBlue, deviceRed);
  EXPECT_EQ(sample(paint), blue);
  EXPECT_EQ(preparation->retainedShader(), rasterBlue);

  Paint invalid = paint;
  invalid.set("uEnvironmentSize", std::array<float, 2>{-1, 32});
  EXPECT_EQ(sample(invalid), (SkColor4f{0, 0, 0, 1}));
  EXPECT_FALSE(preparation->retainedShader());
  EXPECT_NEAR(deviceSample(*graphite, paint).fB, blue.fB, .002f);
  EXPECT_EQ(preparation->retainedShader(graphite->recorder()), deviceBlue);
  EXPECT_EQ(sample(paint), blue);
  const auto retriedRaster = preparation->retainedShader();
  EXPECT_TRUE(retriedRaster);
  EXPECT_NE(retriedRaster, rasterBlue);
  EXPECT_EQ(deviceSample(*graphite, invalid), (SkColor4f{0, 0, 0, 1}));
  EXPECT_FALSE(preparation->retainedShader(graphite->recorder()));
  EXPECT_EQ(sample(paint), blue);
  EXPECT_EQ(preparation->retainedShader(), retriedRaster);
  EXPECT_NEAR(deviceSample(*graphite, paint).fB, blue.fB, .002f);
  const auto retriedDevice = preparation->retainedShader(graphite->recorder());
  EXPECT_TRUE(retriedDevice);
  EXPECT_NE(retriedDevice, deviceBlue);

  auto other = sigil::skia::GraphiteContext::create(*device);
  ASSERT_TRUE(other);
  EXPECT_NEAR(deviceSample(*other, paint).fB, blue.fB, .002f);
  const auto otherAtlas = preparation->retainedShader(other->recorder());
  EXPECT_TRUE(otherAtlas);
  EXPECT_NE(otherAtlas, retriedDevice);
  EXPECT_EQ(sample(paint), blue);
  EXPECT_EQ(preparation->retainedShader(), retriedRaster);
  EXPECT_NEAR(deviceSample(*graphite, paint).fB, blue.fB, .002f);
  EXPECT_NE(preparation->retainedShader(graphite->recorder()), otherAtlas);
}
#endif
