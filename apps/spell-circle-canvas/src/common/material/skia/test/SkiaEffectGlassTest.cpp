#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../EffectInternal.h"

using namespace sigil::material;

namespace {

enum class Coverage { Opaque, Checker, TransparentLeft };

// Raster runtime filters quantize intermediate color to eight bits even
// when this fixture supplies F16 images. Compare identity to the same path;
// coordinate-ramp readback allows one channel code value of rounding.
constexpr float kRasterColorStep = 1.0f / 255;

const Filter& unchangedLayer() {
  static const Filter filter = [] {
    auto [program, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
uniform shader content;
half4 main(float2 p) { return content.eval(p); }
)"));
    return skia::program(std::move(program));
  }();
  return filter;
}

std::vector<float> rampThrough(const Filter& filter,
                               const FrameData* frame = nullptr,
                               Coverage coverage = Coverage::Opaque,
                               bool clip = false, float scale = 1,
                               float gain = 1) {
  const int extent = static_cast<int>(64 * scale);
  const auto info = SkImageInfo::Make(extent, extent, kRGBA_F32_SkColorType,
                                      kPremul_SkAlphaType);
  auto surface = SkSurfaces::Raster(info.makeColorType(kRGBA_F16_SkColorType));
  if (!surface) return {};
  SkBitmap ramp;
  ramp.allocPixels(
      SkImageInfo::Make(64, 64, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  for (int y = 0; y < 64; ++y) {
    float* row = static_cast<float*>(ramp.getAddr(0, y));
    for (int x = 0; x < 64; ++x) {
      const float alpha =
          coverage == Coverage::Checker ? ((x / 8 + y / 8) % 2 ? 0.25f : 0.75f)
          : coverage == Coverage::TransparentLeft && x < 32 ? 0.0f
                                                            : 1.0f;
      row[4 * x] = (x + 0.5f) / 64 * alpha * gain;
      row[4 * x + 1] = (y + 0.5f) / 64 * alpha;
      row[4 * x + 2] = 0.25f * alpha;
      row[4 * x + 3] = alpha;
    }
  }
  SkBitmap source;
  source.allocPixels(ramp.info().makeColorType(kRGBA_F16_SkColorType));
  if (!ramp.readPixels(source.pixmap(), 0, 0)) return {};
  source.setImmutable();
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.scale(scale, scale);
  if (clip) canvas.clipRect(SkRect::MakeXYWH(32, 0, 16, 64));
  SkPaint layer;
  layer.setImageFilter(skia::resolvedImageFilter(filter, frame));
  canvas.saveLayer(
      SkCanvas::SaveLayerRec(nullptr, &layer, SkCanvas::kF16ColorType));
  canvas.drawImage(source.asImage(), 0, 0);
  canvas.restore();
  std::vector<float> pixels(static_cast<size_t>(extent) * extent * 4);
  if (!surface->readPixels(
          SkPixmap(info, pixels.data(), extent * 4 * sizeof(float)), 0, 0))
    return {};
  return pixels;
}

const float* pixel(const std::vector<float>& pixels, int x = 32, int y = 32,
                   int extent = 64) {
  return pixels.data() + (static_cast<size_t>(y) * extent + x) * 4;
}

testing::AssertionResult pixelsNear(const std::vector<float>& actual,
                                    const std::vector<float>& expected,
                                    float tolerance, bool alphaOnly = false) {
  size_t checked = 0;
  size_t mismatches = 0;
  size_t worst = 0;
  double worstRank = -1;
  double worstDifference = 0;
  for (size_t at = alphaOnly ? 3 : 0; at < expected.size();
       at += alphaOnly ? 4 : 1) {
    ++checked;
    const double difference =
        std::abs(static_cast<double>(actual[at]) - expected[at]);
    if (actual[at] == expected[at] || difference <= tolerance) continue;
    ++mismatches;
    const double rank = std::isnan(difference)
                            ? std::numeric_limits<double>::infinity()
                            : difference;
    if (rank > worstRank) {
      worst = at;
      worstRank = rank;
      worstDifference = difference;
    }
  }
  if (!mismatches) return testing::AssertionSuccess();
  constexpr std::array<const char*, 4> channels{"R", "G", "B", "A"};
  return testing::AssertionFailure()
         << mismatches << " of " << checked
         << " channel values differ; worst at pixel (" << (worst / 4) % 64
         << ", " << (worst / 4) / 64 << "), channel " << channels[worst % 4]
         << " (index " << worst << "): actual=" << actual[worst]
         << ", expected=" << expected[worst]
         << ", absolute difference=" << worstDifference
         << ", tolerance=" << tolerance;
}

Material tiltedNormal(float x = 0.6f, float y = 0) {
  return Color{(x + 1) * 0.5f, (y + 1) * 0.5f,
               (std::sqrt(1 - x * x - y * y) + 1) * 0.5f, 1};
}

float snellOffset(float normalX, float normalZ, float ior, float thickness,
                  float radius) {
  const float eta = 1 / ior;
  const float bend =
      std::sqrt(1 - eta * eta * (1 - normalZ * normalZ)) - eta * normalZ;
  return std::clamp(-thickness * bend * normalX / (eta + bend * normalZ),
                    -radius, radius);
}

struct RampParameters {
  float gain = 1;
  float alpha = 1;
};

// Procedural children isolate the stock kernel from image-filter storage.
std::vector<float> kernelRampThrough(const GlassOptions& options,
                                     RampParameters channels = {}) {
  const auto& program = skia::effectProgram(skia::EffectProgram::Glass);
  if (!program) return {};
  const Material ramp = shader(R"(
half4 main(float2 p) {
  return half4(p.x / 64 * gain * alpha, p.y / 64 * alpha, 0.25 * alpha, alpha);
})",
                               channels);
  SkRuntimeShaderBuilder builder(program);
  builder.uniform("ior") = options.ior;
  builder.uniform("thickness") = options.thickness;
  builder.uniform("normalDirectX") = options.normalDirectX ? 1.0f : 0.0f;
  builder.uniform("_sampleRadius") = options.sampleRadius;
  builder.child("normal") = skia::shader(
      skia::paint(options.normal.value_or(Material(Color{0.5f, 0.5f, 1, 1}))));
  builder.child("content") = skia::shader(skia::paint(ramp));
  SkPaint paint;
  paint.setShader(builder.makeShader());
  if (!paint.getShader()) return {};
  const auto info =
      SkImageInfo::Make(64, 64, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  auto surface = SkSurfaces::Raster(info);
  if (!surface) return {};
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  surface->getCanvas()->drawPaint(paint);
  std::vector<float> pixels(64 * 64 * 4);
  if (!surface->readPixels(
          SkPixmap(info, pixels.data(), 64 * 4 * sizeof(float)), 0, 0))
    return {};
  return pixels;
}

}  // namespace

struct IdentityCase {
  const char* name;
  GlassOptions options;
};

class SkiaGlassIdentity : public testing::TestWithParam<IdentityCase> {};

TEST_P(SkiaGlassIdentity, PreservesTranslucentPixels) {
  ASSERT_FALSE(unchangedLayer().isNone());
  const auto original =
      rampThrough(unchangedLayer(), nullptr, Coverage::Checker);
  ASSERT_FALSE(original.empty());
  const Filter glass = Filter::glass(GetParam().options);
  ASSERT_FALSE(glass.isNone());
  const auto filtered = rampThrough(glass, nullptr, Coverage::Checker);
  ASSERT_EQ(filtered.size(), original.size());
  EXPECT_TRUE(pixelsNear(filtered, original, 0.0001f));
}

INSTANTIATE_TEST_SUITE_P(
    Disabled, SkiaGlassIdentity,
    testing::Values(
        IdentityCase{"FlatNormal", {}},
        IdentityCase{"IndexZero", {.ior = 0, .normal = tiltedNormal()}},
        IdentityCase{"IndexOne", {.ior = 1, .normal = tiltedNormal()}},
        IdentityCase{"IndexNegative", {.ior = -1, .normal = tiltedNormal()}},
        IdentityCase{"IndexNonfinite",
                     {.ior = std::numeric_limits<float>::quiet_NaN(),
                      .normal = tiltedNormal()}},
        IdentityCase{"DepthZero", {.thickness = 0, .normal = tiltedNormal()}},
        IdentityCase{"DepthNegative",
                     {.thickness = -1, .normal = tiltedNormal()}},
        IdentityCase{"DepthNonfinite",
                     {.thickness = std::numeric_limits<float>::quiet_NaN(),
                      .normal = tiltedNormal()}},
        IdentityCase{"RadiusZero",
                     {.sampleRadius = 0, .normal = tiltedNormal()}},
        IdentityCase{"DegenerateNormal",
                     {.normal = Color{0.5f, 0.5f, 0.5f, 1}}},
        IdentityCase{"BackFacingNormal", {.normal = Color{0.5f, 0.5f, 0, 1}}},
        IdentityCase{"TranslucentNormal",
                     {.normal = Color{0.8f, 0.5f, 0.9f, 0.5f}}}),
    [](const testing::TestParamInfo<IdentityCase>& info) {
      return info.param.name;
    });

struct TiltCase {
  const char* name;
  float x = 0.6f;
  float y = 0;
  float thickness = 12;
  float radius = 32;
  bool directX = false;
};

class SkiaGlassTilt : public testing::TestWithParam<TiltCase> {};

TEST_P(SkiaGlassTilt, MatchesBoundedSnellDisplacement) {
  const TiltCase& input = GetParam();
  const GlassOptions options{.thickness = input.thickness,
                             .sampleRadius = input.radius,
                             .normal = tiltedNormal(input.x, input.y),
                             .normalDirectX = input.directX};
  const auto filtered = kernelRampThrough(options);
  ASSERT_FALSE(filtered.empty());
  const float z = std::sqrt(1 - input.x * input.x - input.y * input.y);
  const float x = snellOffset(input.x, z, 1.5f, input.thickness, input.radius);
  const float y = snellOffset(input.directX ? input.y : -input.y, z, 1.5f,
                              input.thickness, input.radius);
  EXPECT_NEAR(pixel(filtered)[0], (32.5f + x) / 64, 0.001f);
  EXPECT_NEAR(pixel(filtered)[1], (32.5f + y) / 64, 0.001f);
}

INSTANTIATE_TEST_SUITE_P(
    Analytic, SkiaGlassTilt,
    testing::Values(TiltCase{"SubpixelCeiling", .6f, 0, 12, .25f},
                    TiltCase{"PixelCeiling", .6f, 0, 12, 1},
                    TiltCase{"Unclamped", .6f, 0, 12, 8},
                    TiltCase{"LargeFiniteDepth", .6f, 0,
                             std::numeric_limits<float>::max(), 8},
                    TiltCase{"GreenUp", 0, .6f},
                    TiltCase{"GreenDown", 0, .6f, 12, 32, true}),
    [](const testing::TestParamInfo<TiltCase>& info) {
      return info.param.name;
    });

TEST(SkiaGlass,
     RefractionKeepsIncomingAlphaAndFallsBackFromTransparentLookups) {
  const Filter glass =
      Filter::glass({.thickness = 40, .normal = tiltedNormal()});
  ASSERT_FALSE(glass.isNone());
  const auto original =
      rampThrough(unchangedLayer(), nullptr, Coverage::Checker);
  ASSERT_FALSE(original.empty());
  const auto filtered = rampThrough(glass, nullptr, Coverage::Checker);
  ASSERT_EQ(filtered.size(), original.size());
  EXPECT_TRUE(pixelsNear(filtered, original, 0.0001f, true));
  EXPECT_GT(std::abs(pixel(filtered)[0] - pixel(original)[0]), 0.02f);

  const auto transparent =
      rampThrough(glass, nullptr, Coverage::TransparentLeft);
  const auto plain =
      rampThrough(unchangedLayer(), nullptr, Coverage::TransparentLeft);
  ASSERT_EQ(transparent.size(), plain.size());
  for (int channel = 0; channel < 4; ++channel)
    EXPECT_NEAR(pixel(transparent, 35)[channel], pixel(plain, 35)[channel],
                0.0001f);
  const auto hdrIncoming =
      rampThrough(unchangedLayer(), nullptr, Coverage::Opaque, false, 1, 8);
  ASSERT_FALSE(hdrIncoming.empty());
  RecordProperty("raster_incoming_alpha", std::to_string(pixel(original)[3]));
  RecordProperty("raster_incoming_hdr_red",
                 std::to_string(pixel(hdrIncoming)[0]));
}

TEST(SkiaGlass, TheKernelPreservesHdrColorAndPremultipliedCoverage) {
  const GlassOptions options{.thickness = 40, .normal = tiltedNormal()};
  const auto hdr = kernelRampThrough(options, {.gain = 8, .alpha = .75f});
  ASSERT_FALSE(hdr.empty());
  const float offset = snellOffset(.6f, .8f, 1.5f, 40, 32);
  EXPECT_NEAR(pixel(hdr)[0], (32.5f + offset) / 64 * 8 * .75f, 0.002f);
  EXPECT_GT(pixel(hdr)[0], 1.0f);
  EXPECT_FLOAT_EQ(pixel(hdr)[3], .75f);
}

TEST(SkiaGlass, SamplingReachSurvivesWritesBindingsAndClippedReconstruction) {
  Filter glass = Filter::glass(
      {.thickness = 40, .sampleRadius = 8, .normal = tiltedNormal()});
  const Filter original = glass;
  auto radius = sigil::motion::animatable(100.0f);
  auto block = std::make_shared<UniformBlock>(1);
  block->values()[0] = 100;
  block->commit();
  glass.set("_sampleRadius", 100.0f)
      .set("_sampleRadius", std::array<float, 2>{100, 100})
      .set("_sampleRadius", std::array<float, 4>{100, 100, 100, 100})
      .set("_sampleRadius", std::vector<float>{100})
      .bind("_sampleRadius", radius)
      .bind("_sampleRadius", block);
  EXPECT_EQ(glass, original);
  EXPECT_FALSE(glass.isRunning());
  auto depth = sigil::motion::animatable(40.0f);
  glass.bind("thickness", depth);
  EXPECT_TRUE(glass.isRunning());
  FrameData frame;
  frame.resolution = {64, 64};
  for (const float thickness : {40.0f, 80.0f}) {
    depth = thickness;
    const auto filter = skia::resolvedImageFilter(glass, &frame);
    ASSERT_NE(filter, nullptr);
    const SkIRect requested = SkIRect::MakeXYWH(32, 8, 8, 8);
    SkIRect expected = requested;
    expected.outset(8, 8);
    EXPECT_EQ(
        filter->filterBounds(requested, SkMatrix::I(),
                             SkImageFilter::kReverse_MapDirection, nullptr),
        expected);
    const auto filtered = rampThrough(glass, &frame, Coverage::Opaque, true);
    ASSERT_FALSE(filtered.empty());
    EXPECT_NEAR(pixel(filtered, 32)[0], 24.5f / 64, kRasterColorStep);
  }
}

TEST(SkiaGlass, CopiesRemainIndependentAndRadiusParticipatesInEquality) {
  const GlassOptions options{.normal = tiltedNormal()};
  const Filter original = Filter::glass(options);
  ASSERT_FALSE(original.isNone());
  EXPECT_EQ(original, Filter::glass(options));
  EXPECT_NE(original,
            Filter::glass({.sampleRadius = 31, .normal = tiltedNormal()}));
  Filter changed = original;
  changed.set("thickness", 0.0f);
  EXPECT_NE(changed, original);
  const auto plain = rampThrough(unchangedLayer());
  const auto flat = rampThrough(changed);
  const auto tilted = rampThrough(original);
  ASSERT_FALSE(plain.empty());
  ASSERT_EQ(flat.size(), plain.size());
  ASSERT_EQ(tilted.size(), plain.size());
  EXPECT_NEAR(pixel(flat)[0], pixel(plain)[0], 0.0001f);
  EXPECT_GT(pixel(plain)[0] - pixel(tilted)[0], 0.02f);
}

struct RadiusCase {
  const char* name;
  float radius;
};

class SkiaGlassRadius : public testing::TestWithParam<RadiusCase> {};

TEST_P(SkiaGlassRadius, RejectsAnInvalidSamplingDeclaration) {
  EXPECT_TRUE(Filter::glass({.sampleRadius = GetParam().radius}).isNone());
}

INSTANTIATE_TEST_SUITE_P(
    Invalid, SkiaGlassRadius,
    testing::Values(
        RadiusCase{"Negative", -1},
        RadiusCase{"Infinite", std::numeric_limits<float>::infinity()},
        RadiusCase{"NotANumber", std::numeric_limits<float>::quiet_NaN()}),
    [](const testing::TestParamInfo<RadiusCase>& info) {
      return info.param.name;
    });

TEST(SkiaGlass, AnOrdinaryProgramRetainsItsUniformNamespace) {
  auto [program, error] = SkRuntimeEffect::MakeForShader(SkString(R"(
uniform shader content;
uniform float _sampleRadius;
half4 main(float2 p) { return content.eval(p) * half(_sampleRadius); }
)"));
  ASSERT_NE(program, nullptr) << error.c_str();
  Filter ordinary = skia::program(program, {{"_sampleRadius", 1}});
  ordinary.set("_sampleRadius", 0.5f);
  const auto half = rampThrough(ordinary);
  ASSERT_FALSE(half.empty());
  EXPECT_NEAR(pixel(half)[3], 0.5f, kRasterColorStep);
  auto gain = sigil::motion::animatable(0.25f);
  ordinary.bind("_sampleRadius", gain);
  EXPECT_TRUE(ordinary.isRunning());
  const auto quarter = rampThrough(ordinary);
  ASSERT_FALSE(quarter.empty());
  EXPECT_NEAR(pixel(quarter)[3], 0.25f, kRasterColorStep);
}

TEST(SkiaGlass,
     AChildNormalReadsTheCurrentFrameAtFirstPaintAfterSeekAndResize) {
  const Material normal = shader(R"(
half4 main(float2 p) {
  float x = 0.6 * sin(uTime) * p.x / max(uResolution.x, 1.0);
  return half4(0.5 + x * 0.5, 0.5, 0.5 + 0.5 * sqrt(max(1.0 - x*x, 0.0)), 1);
})");
  const Filter glass = Filter::glass({.thickness = 24, .normal = normal});
  ASSERT_TRUE(glass.isRunning());
  FrameData frame;
  frame.resolution = {64, 64};
  frame.seconds = 1.5707963267948966;
  const auto first = rampThrough(glass, &frame);
  ASSERT_FALSE(first.empty());
  frame.seconds = 0;
  const auto sought = rampThrough(glass, &frame);
  ASSERT_EQ(sought.size(), first.size());
  EXPECT_EQ(sought, rampThrough(unchangedLayer()));
  EXPECT_GT(pixel(sought)[0] - pixel(first)[0], 0.02f);
  frame.seconds = 1.5707963267948966;
  const auto resumed = rampThrough(glass, &frame);
  ASSERT_EQ(resumed.size(), first.size());
  EXPECT_EQ(resumed, first);
  frame.resolution = {128, 64};
  const auto resized = rampThrough(glass, &frame);
  ASSERT_EQ(resized.size(), first.size());
  EXPECT_GT(pixel(resized)[0] - pixel(first)[0], 0.01f);
}

class SkiaGlassScale : public testing::TestWithParam<float> {};

TEST_P(SkiaGlassScale, DisplacementStaysInLogicalPixels) {
  const Filter glass =
      Filter::glass({.thickness = 12, .normal = tiltedNormal()});
  ASSERT_FALSE(glass.isNone());
  const float offset = snellOffset(0.6f, 0.8f, 1.5f, 12, 32);
  const float scale = GetParam();
  FrameData frame;
  frame.resolution = {64, 64};
  frame.contentScale = scale;
  const auto filtered =
      rampThrough(glass, &frame, Coverage::Opaque, false, scale);
  ASSERT_FALSE(filtered.empty());
  const int extent = static_cast<int>(64 * scale);
  const int center = static_cast<int>(32 * scale);
  EXPECT_NEAR(pixel(filtered, center, center, extent)[0],
              ((center + 0.5f) / scale + offset) / 64, kRasterColorStep);
}

INSTANTIATE_TEST_SUITE_P(Density, SkiaGlassScale,
                         testing::Values(.5f, 1.0f, 2.0f),
                         [](const testing::TestParamInfo<float>& info) {
                           return info.param == .5f ? "Half"
                                  : info.param == 1 ? "Native"
                                                    : "Double";
                         });
