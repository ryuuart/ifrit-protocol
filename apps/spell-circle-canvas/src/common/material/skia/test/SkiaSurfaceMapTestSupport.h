#pragma once

/** @file
 * Support for the surface map cases: a paint drawn over a box and read
 * back in floating point, and the colour at its middle.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/skia/SurfaceMaps.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>

#include <cmath>
#include <glm/mat3x3.hpp>

using namespace sigil::material;

namespace {

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

}  // namespace
