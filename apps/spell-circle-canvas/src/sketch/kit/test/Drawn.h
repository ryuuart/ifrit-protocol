#pragma once

/** @file
 * WHAT EVERY CASE IN THIS DIRECTORY ASSERTS WITH: a composer over a
 * raster surface, and the comparison of two trees by the pixels they
 * produce.
 *
 * The claim the kit rests on is a claim about PICTURES — a component
 * draws exactly what the compose kit spelled by hand with the same values
 * draws — so the assertion is pixels and not structure, and every file
 * here makes it the same way.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilmotion/clock/Ticker.h>

#include <cstring>
#include <utility>

#include "support/Fixtures.h"

namespace sigil::sketch::kit::test {

/** The surface every comparison is drawn on. Wide enough that a card, a
 *  table and a bar all stand inside it without being clipped, which a
 *  pixel comparison would otherwise read as agreement. */
inline constexpr int kWide = 420;
inline constexpr int kTall = 300;

/** A composer over a raster surface, so a tree can be compared to another
 *  tree by the pixels the two produce. */
struct Drawn {
  sigil::motion::Ticker ticker;
  compose::Composer composer{ticker, sigil::sketch::test::fonts()};
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWide, kTall));

  explicit Drawn(compose::Element tree) {
    composer.setSize({(float)kWide, (float)kTall});
    composer.render(std::move(tree));
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
  }

  SkBitmap pixels() {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWide, kTall));
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  }
};

/** Every pixel of one tree against every pixel of the other. */
inline bool sameDrawing(compose::Element left, compose::Element right) {
  SkBitmap a = Drawn(std::move(left)).pixels();
  SkBitmap b = Drawn(std::move(right)).pixels();
  for (int y = 0; y < kTall; ++y)
    if (std::memcmp(a.getAddr32(0, y), b.getAddr32(0, y), (size_t)kWide * 4) !=
        0)
      return false;
  return true;
}

/** A subject with nothing of the theme in it, so a difference between two
 *  drawings is a difference in what the kit put around it. */
inline compose::Element subject() {
  return compose::box()
      .width(compose::Dimension(60))
      .height(compose::Dimension(40))
      .fill(compose::Fill::color({0.9f, 0.3f, 0.4f, 1}));
}

}  // namespace sigil::sketch::kit::test
