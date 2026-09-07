#pragma once

/** @file
 * WHAT A MATERIAL LOOKS LIKE, for a test that has to read it.
 *
 * A material is a value until something shades it, so nearly every case
 * in this library ends by drawing one onto a raster surface and reading
 * pixels back. Two ways of drawing are meant here and they are not the
 * same claim: `render` paints the shader over the whole surface, which
 * is what asks what a body computes at each point, and `shade` fills a
 * path with it over a black ground, which is what asks what a caller
 * gets when they paint a shape. The readings beside them — is this the
 * same picture, how bright is this pixel, how many pixels moved — are
 * the ones a case makes rather than pinning a byte.
 *
 * A file that takes only the readings links no backend: the shading
 * calls are inline and a binary that never makes one never asks for the
 * compiler.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cstring>

namespace sigil::material::test {

/** @p shader painted over the whole of a @p width × @p height surface. */
inline SkBitmap render(const sk_sp<SkShader>& shader, int width = 4,
                       int height = 4) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setShader(shader);
  canvas.drawPaint(paint);
  return bitmap;
}

/** …and @p m compiled through the Skia backend and painted the same way,
 *  with the surface's own size as the resolution the body reads. */
inline SkBitmap render(const Material& m, int width, int height) {
  skia::install();
  return render(skia::shader(m, {.resolution = {(float)width, (float)height}}),
                width, height);
}

/** @p m FILLING a rect over a black ground, which is what a caller who
 *  paints a shape with a material gets. */
inline SkBitmap shade(const Material& m, int width, int height) {
  skia::install();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  surface->getCanvas()->clear(SK_ColorBLACK);
  skia::fill(*surface->getCanvas(),
             SkPath::Rect(SkRect::MakeWH((float)width, (float)height)), m);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  surface->makeImageSnapshot()->readPixels(nullptr, bitmap.pixmap(), 0, 0);
  return bitmap;
}

/** One flat colour, as an image a texture can stand on. */
inline sk_sp<SkImage> solid(SkColor colour, int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  bitmap.eraseColor(colour);
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** Are these the same picture, byte for byte? */
inline bool identical(const SkBitmap& a, const SkBitmap& b) {
  return a.computeByteSize() == b.computeByteSize() &&
         std::memcmp(a.getPixels(), b.getPixels(), a.computeByteSize()) == 0;
}

/** …and how many of their pixels are not, for a case whose claim is that
 *  something moved rather than what it moved to. */
inline int differing(const SkBitmap& a, const SkBitmap& b) {
  int n = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x)
      n += a.getColor(x, y) != b.getColor(x, y);
  return n;
}

/** How bright @p c is, on the Rec.601 weights, in 0..255. */
inline int luminance(SkColor c) {
  return ((int)SkColorGetR(c) * 299 + (int)SkColorGetG(c) * 587 +
          (int)SkColorGetB(c) * 114) /
         1000;
}

}  // namespace sigil::material::test
