#pragma once

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Composer.h>
#include <sigilmotion/clock/Engine.h>

#include <new>
#include <stdexcept>

namespace sigil::compose::test {

/** A retained scene drawn on a readable CPU raster. The caller supplies
 *  the font context and advances time explicitly; no window or device is
 *  required. Keep the font context alive for the scene's lifetime. */
struct Scene {
  motion::Engine engine;
  Composer composer;
  sk_sp<SkSurface> surface;

  explicit Scene(weave::FontContext& fonts, int width = 200, int height = 200)
      : composer(engine, fonts) {
    if (width <= 0 || height <= 0)
      throw std::invalid_argument("A test scene needs positive dimensions.");
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
    if (!surface) throw std::bad_alloc();
    composer.setSize({static_cast<float>(width), static_cast<float>(height)});
    surface->getCanvas()->clear(SK_ColorBLACK);
  }

  /** Advance by the stated step, clear the surface and draw one frame. */
  void frame(double seconds = 0.0, SkColor ground = SK_ColorBLACK) {
    if (seconds > 0)
      engine.advance(engine.elapsed() + motion::Duration(seconds));
    surface->getCanvas()->clear(ground);
    composer.draw(*surface->getCanvas());
  }

  /** Read one pixel without copying the surface. Outside is transparent. */
  SkColor pixel(int x, int y) const {
    SkPixmap pixels;
    if (!surface->peekPixels(&pixels))
      throw std::runtime_error("The test scene has no readable raster pixels.");
    if (x < 0 || y < 0 || x >= pixels.width() || y >= pixels.height())
      return SK_ColorTRANSPARENT;
    return pixels.getColor(x, y);
  }

  /** An owned snapshot, unaffected by later drawing. */
  SkBitmap pixels() const {
    SkBitmap result;
    if (!result.tryAllocPixels(surface->imageInfo())) throw std::bad_alloc();
    if (!surface->readPixels(result.pixmap(), 0, 0))
      throw std::runtime_error(
          "The test scene could not copy its raster pixels.");
    return result;
  }
};

}  // namespace sigil::compose::test
