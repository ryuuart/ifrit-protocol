#pragma once

/** @file
 * Reading a Graphite surface back to CPU pixels, for any test binary that
 * draws on one. The allocation wrapper uses the library's surface readback
 * so runtime captures and test captures share submission and completion.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>

namespace sigil::skia::test {

/** The pixels of a Graphite surface, read back through the context that
 *  drew it. An empty bitmap says allocation or readback failed. */
inline SkBitmap readGraphiteSurface(GraphiteContext& ctx, SkSurface* surface) {
  SkBitmap bitmap;
  if (!surface || !bitmap.tryAllocPixels(surface->imageInfo()) ||
      !readbackPixels(ctx, *surface, bitmap.pixmap()))
    return {};
  return bitmap;
}

/** The same read narrowed to the one pixel at @p x, @p y, unpremultiplied.
 *  Transparent when the read never completed. */
inline SkColor readGraphitePixel(GraphiteContext& ctx, SkSurface* surface,
                                 int x, int y) {
  const SkBitmap pixels = readGraphiteSurface(ctx, surface);
  return pixels.isNull() ? SK_ColorTRANSPARENT : pixels.getColor(x, y);
}

}  // namespace sigil::skia::test
