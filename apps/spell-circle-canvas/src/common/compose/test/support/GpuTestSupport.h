#pragma once
// What every Graphite case shares: the one device the binary draws on, a
// composer frame drawn on it or on a raster surface and read back, the
// count of pixels two read-backs disagree on, and the skip a machine
// without a device takes.

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/typography/Presets.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>
#include <sigilweave/choreograph/GlyphBatches.h>
#include <sigilweave/choreograph/PlacedGlyph.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paint/Paint.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Decoration.h>
#include <sigilweave/style/PaintLayer.h>
#include <sigilweave/style/TextStyle.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "Fonts.h"

using namespace sigil::compose;

namespace geometry = sigil::geometry;

namespace sigil::compose::graphiteTesting {

using sigil::test::fonts;

/** The one Graphite context every case draws on, or null on a machine
 *  without a device. */
inline sigil::skia::GraphiteContext* graphite() {
  static std::unique_ptr<sigil::core::hardware::GpuDevice> device =
      sigil::core::hardware::GpuDevice::createOwned();
  static std::unique_ptr<sigil::skia::GraphiteContext> context =
      device ? sigil::skia::GraphiteContext::create(*device) : nullptr;
  return context.get();
}

/** Allocates the requested pixel format and reads the Graphite surface. */
inline SkBitmap readbackGpu(sigil::skia::GraphiteContext& context,
                            SkSurface& surface, const SkImageInfo& info) {
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info) ||
      !sigil::skia::readbackPixels(context, surface, bitmap.pixmap()))
    bitmap.reset();
  return bitmap;
}

/** Draws one composer frame on a Graphite surface and reads it back. */
inline SkBitmap drawOnGpu(Composer& composer,
                          sigil::skia::GraphiteContext& context, int width,
                          int height) {
  const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(context.recorder(), info);
  if (!surface) return {};
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  return readbackGpu(context, *surface, info);
}

inline SkBitmap drawOnGpu(Composer& composer, int width, int height) {
  sigil::skia::GraphiteContext* context = graphite();
  return context ? drawOnGpu(composer, *context, width, height) : SkBitmap{};
}

/** Draws one composer frame on a raster surface and reads it back. */
inline SkBitmap drawOnRaster(Composer& composer, int width, int height) {
  const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  SkBitmap pixels;
  if (!surface) return pixels;
  surface->getCanvas()->clear(SK_ColorBLACK);
  composer.draw(*surface->getCanvas());
  pixels.allocPixels(info);
  if (!surface->readPixels(pixels, 0, 0)) pixels.reset();
  return pixels;
}

inline size_t mismatchedPixels(const SkBitmap& expected,
                               const SkBitmap& actual) {
  size_t mismatched = 0;
  for (int y = 0; y < expected.height(); ++y)
    for (int x = 0; x < expected.width(); ++x)
      if (expected.getColor(x, y) != actual.getColor(x, y)) ++mismatched;
  return mismatched;
}

}  // namespace sigil::compose::graphiteTesting

#define REQUIRE_GPU()                \
  if (!graphite()) {                 \
    GTEST_SKIP() << "no GPU device"; \
  }
