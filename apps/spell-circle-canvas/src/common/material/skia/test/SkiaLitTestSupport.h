#pragma once

/** @file
 * Support for the lit-surface cases: a normal map whose halves face
 * opposite ways and the surface made from it, a paint drawn over a box
 * and read back in 8-bit or floating point, the environment reflectance
 * a roughness answers, a picture that counts its reads, and the suite a
 * case about a prepared positioned pass is parameterised over.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilmaterial/advanced/Terms.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilmotion/values/Animatable.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <future>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace sigil::material;

namespace {

/** A normal map whose left half faces left and whose right half faces
 *  right, both tilted 45 degrees out of the page. */
inline sk_sp<SkImage> twoFacedNormals(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, true);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const bool left = x < width / 2;
      // (±0.707, 0, 0.707) encoded as (n + 1) / 2.
      const uint8_t red = left ? 37 : 218;
      *bitmap.getAddr32(x, y) = SkPreMultiplyARGB(255, red, 128, 218);
    }
  bitmap.setImmutable();
  return bitmap.asImage();
}

inline Material relief(int width, int height) {
  return from(Color{0.6f, 0.6f, 0.6f, 1})
      .surface(
          {.roughness = 0.8f, .normal = image(twoFacedNormals(width, height))});
}

/** @p paint drawn over a box, read back. */
inline SkBitmap drawn(const Paint& paint, int width, int height,
                      FrameData frame = {}) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, true);
  SkCanvas canvas(bitmap);
  frame.resolution = {(float)width, (float)height};
  SkPaint fill;
  fill.setShader(skia::shader(paint, frame));
  canvas.drawRect(SkRect::MakeWH((float)width, (float)height), fill);
  return bitmap;
}

inline SkBitmap drawnFloat(const Paint& paint, int width = 8, int height = 8,
                           FrameData frame = {}) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(width, height, kRGBA_F32_SkColorType,
                                       kPremul_SkAlphaType));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  frame.resolution = {float(width), float(height)};
  SkPaint fill;
  if (paint.isSolid()) {
    const Color color = paint.solidColor();
    fill.setColor4f({color.r, color.g, color.b, color.a}, nullptr);
  } else {
    auto shader = skia::shader(paint, frame);
    EXPECT_TRUE(shader);
    fill.setShader(std::move(shader));
  }
  canvas.drawRect(SkRect::MakeWH(float(width), float(height)), fill);
  return bitmap;
}

inline SkColor4f sampled(const Paint& paint) {
  return drawnFloat(paint).getColor4f(4, 4);
}

inline float brightness(const SkBitmap& bitmap, int x, int y) {
  const SkColor colour = bitmap.getColor(x, y);
  return (float)(SkColorGetR(colour) + SkColorGetG(colour) +
                 SkColorGetB(colour));
}

inline float environmentReflectance(float roughness) {
  struct Parameters {
    float uRoughness;
  };
  std::string source(termsSource(Target::SkSL));
  source +=
      "half4 main(float2 p) { return half4(environmentSpecular("
      "float3(1),float3(.04),uRoughness,1),1); }";
  return sampled(skia::paint(shader(source, Parameters{roughness}))).fR;
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

class PreparedPositionedPass : public testing::TestWithParam<LightKind> {};

}  // namespace
