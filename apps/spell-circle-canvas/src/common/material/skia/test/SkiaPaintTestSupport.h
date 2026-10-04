#pragma once

/** @file
 * Support for the paint-value cases: the effects that declare each
 * volatility tier, a height normal compiled as a raw effect and the
 * normal it should sample to, and the suites the uniform, placement and
 * publication cases are parameterised over.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkM44.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkString.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmotion/values/Animatable.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <cmath>
#include <cstdint>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::identical;
using sigil::material::test::render;

namespace {

struct TwoParameters {
  float uScale;
  Color uColor;
};

constexpr const char* kBody =
    "half4 main(float2 p) { return half4(uColor * uScale); }";

}  // namespace

namespace {

inline sk_sp<SkRuntimeEffect> effectFor(const char* src) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(src));
  return effect;
}

/** A constants-only effect: nothing about it changes between draws. */
inline sk_sp<SkRuntimeEffect> constantEffect() {
  static sk_sp<SkRuntimeEffect> fx = effectFor(
      "uniform float uK;\n"
      "half4 main(float2 p) { return half4(half(uK), 0, 0, 1); }");
  return fx;
}

/** One that reads the clock, which is the LIVE declaration. */
inline sk_sp<SkRuntimeEffect> timeEffect() {
  static sk_sp<SkRuntimeEffect> fx = effectFor(
      "uniform float uTime;\n"
      "half4 main(float2 p) { return half4(half(uTime), 0, 0, 1); }");
  return fx;
}

/** One that reads the box, which is the GEOMETRY declaration. */
inline sk_sp<SkRuntimeEffect> resolutionEffect() {
  static sk_sp<SkRuntimeEffect> fx = effectFor(
      "uniform float2 uResolution;\n"
      "half4 main(float2 p) { return half4(half(p.x / uResolution.x), 0, 0, "
      "1); }");
  return fx;
}

inline sk_sp<SkRuntimeEffect> worldEffect() {
  static sk_sp<SkRuntimeEffect> fx = effectFor(
      "uniform float4x4 uWorld;\n"
      "half4 main(float2 p) {\n"
      "  float4 root = uWorld * float4(p, 0, 1);\n"
      "  return half4(root.xy / root.w / 128, 0, 1);\n"
      "}");
  return fx;
}

inline Paint rawHeightNormal(const Material& normal, const Material& height) {
  const std::string source = normal.recipe().source(Target::SkSL);
  Paint paint = skia::sksl(effectFor(source.c_str()),
                           {{"depth", 4}, {"sampleDistance", 1}, {"green", 1}});
  paint.slot("height", skia::paint(height));
  return paint;
}

inline void expectSamplingNormal(const sk_sp<SkShader>& shader, float x,
                                 float y) {
  ASSERT_TRUE(shader);
  const SkColor pixel = render(shader, 8, 8).getColor(2, 3);
  const float length = std::sqrt(x * x + y * y + 1);
  EXPECT_NEAR(SkColorGetR(pixel), (x / length * .5f + .5f) * 255, 1);
  EXPECT_NEAR(SkColorGetG(pixel), (y / length * .5f + .5f) * 255, 1);
  EXPECT_NEAR(SkColorGetB(pixel), (1 / length * .5f + .5f) * 255, 1);
  EXPECT_EQ(SkColorGetA(pixel), 255u);
}

struct WorldPlacementCase {
  const char* name;
  glm::mat3 world;
};

class WorldPlacement : public testing::TestWithParam<WorldPlacementCase> {};

enum class MatrixOwnership { Constant, Bound };

class ExplicitWorldMatrix : public testing::TestWithParam<MatrixOwnership> {};

struct WorldUniformCase {
  const char* name;
  const char* source;
};

class UnsupportedWorldUniform
    : public testing::TestWithParam<WorldUniformCase> {};

enum class UniformPaint { DirectPaint, BackedMaterial };

class UniformPublication : public testing::TestWithParam<UniformPaint> {};
class WorldChildPlacement : public testing::TestWithParam<UniformPaint> {};
class ColorUniform : public testing::TestWithParam<UniformPaint> {
 protected:
  Paint describe() const {
    static const auto effect = effectFor(
        "uniform float uScale; uniform float4 uColor; "
        "half4 main(float2 p) { return half4(uColor.rgb * uScale, uColor.a); "
        "}");
    static const Material prototype = shader(
        "half4 main(float2 p) { return half4(uColor.rgb * uScale, uColor.a); }",
        TwoParameters{1, Color{0, 0, 0, 1}});
    return GetParam() == UniformPaint::DirectPaint
               ? skia::sksl(effect, {{"uScale", 1}})
               : Paint::recipe(prototype);
  }
  static void channels(const sk_sp<SkShader>& shader, int red, int green,
                       int blue) {
    ASSERT_TRUE(shader);
    const SkColor pixel = render(shader).getColor(1, 1);
    EXPECT_NEAR(SkColorGetR(pixel), red, 1);
    EXPECT_NEAR(SkColorGetG(pixel), green, 1);
    EXPECT_NEAR(SkColorGetB(pixel), blue, 1);
    EXPECT_EQ(SkColorGetA(pixel), 255);
  }
};

}  // namespace
