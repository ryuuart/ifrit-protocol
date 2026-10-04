#pragma once

/** @file
 * A pen over a raster surface with the pixels readable back: the fixture
 * every test of this library draws on. Its pen sets text in the
 * instrument face, so a width or a seat read off the ink is the same on
 * every machine; `useMachineFace()` gives that up for the one case whose
 * claim is the resolution itself.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <sigildraw/Pen.h>
#include <sigilweave/fonts/FontContext.h>

#include "Fonts.h"

namespace sigil::draw::testing {

using sigil::test::fonts;

/** A pen over a raster surface, with the pixels readable back. */
struct Paper {
  explicit Paper(int w = 100, int h = 100, SkColor ground = SK_ColorTRANSPARENT)
      : surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h))),
        width(w),
        height(h) {
    surface->getCanvas()->clear(ground);
  }

  /** Opens the frame with the pen set in the instrument that puts every
   *  letter on one known advance, punctuation on another and the space on
   *  a third, so what the ink measures is arithmetic rather than whatever
   *  face the machine has installed. */
  void begin(int frame = 1, double seconds = 0.0) {
    beginWith(frame, &fonts(), seconds);
    pen.textFont(sigil::test::instrument::sans());
  }

  /** Opens the frame with the given font context, or without text when
   *  @p context is null. The pen's selected text style is left intact. */
  void beginWith(int frame, weave::FontContext* context, double seconds = 0.0) {
    Frame f;
    f.width = (float)width;
    f.height = (float)height;
    f.seconds = seconds;
    f.deltaSeconds = 1.0 / 60.0;
    f.frameCount = frame;
    f.fonts = context;
    pen.begin(*surface->getCanvas(), f);
  }

  /** Takes the instrument back off, leaving the pen naming no face at
   *  all: the font context resolves the machine's default and the pen
   *  matches families through the machine's manager. For a case whose
   *  claim IS that resolution, which carries the `fonts` label because a
   *  runner without faces has nothing for it. */
  void useMachineFace() { pen.textFont(sk_sp<SkTypeface>()); }

  void end() { pen.end(); }

  /** Every pixel, read back once. */
  SkBitmap pixels() {
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  }

  SkColor pixel(int x, int y) {
    SkPixmap raster;
    if (!surface->peekPixels(&raster)) {
      ADD_FAILURE() << "Raster pixels are unavailable.";
      return SK_ColorTRANSPARENT;
    }
    if (!raster.bounds().contains(x, y)) {
      ADD_FAILURE() << "Pixel coordinate is outside the surface.";
      return SK_ColorTRANSPARENT;
    }
    return raster.getColor(x, y);
  }

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
