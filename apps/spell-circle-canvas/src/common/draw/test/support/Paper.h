#pragma once

/** @file
 * A pen over a raster surface with the pixels readable back: the fixture
 * every test of this library draws on. Text is shaped against the
 * machine's system fonts.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <sigildraw/Pen.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace sigil::draw::testing {

inline sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

/** A pen over a raster surface, with the pixels readable back. */
struct Paper {
  explicit Paper(int w = 100, int h = 100, SkColor ground = SK_ColorTRANSPARENT)
      : surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h))),
        width(w),
        height(h) {
    surface->getCanvas()->clear(ground);
  }

  void begin(int frame = 1, double seconds = 0.0) {
    Frame f;
    f.width = (float)width;
    f.height = (float)height;
    f.seconds = seconds;
    f.deltaSeconds = 1.0 / 60.0;
    f.frameCount = frame;
    f.fonts = &fonts();
    pen.begin(*surface->getCanvas(), f);
  }
  void end() { pen.end(); }

  /** Every pixel, read back once. */
  SkBitmap pixels() {
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  }

  SkColor pixel(int x, int y) { return pixels().getColor(x, y); }

  /** The columns and rows that hold any ink at all. */
  SkIRect inked() {
    const SkBitmap bitmap = pixels();
    SkIRect box = SkIRect::MakeEmpty();
    bool any = false;
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
        if (SkColorGetA(bitmap.getColor(x, y)) > 0) {
          if (!any) {
            box = SkIRect::MakeXYWH(x, y, 1, 1);
            any = true;
          } else {
            box.join(SkIRect::MakeXYWH(x, y, 1, 1));
          }
        }
    return box;
  }

  sk_sp<SkSurface> surface;
  Pen pen;
  int width;
  int height;
};

}  // namespace sigil::draw::testing
