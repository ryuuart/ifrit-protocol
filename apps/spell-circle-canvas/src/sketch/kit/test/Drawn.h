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
#include <sigilcompose/core/Selector.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/kit/Theme.h>

#include <cstring>
#include <utility>

#include "support/Fixtures.h"

namespace sigil::sketch::kit::test {

/** The surface every comparison is drawn on. Wide enough that a card, a
 *  table and a bar all stand inside it without being clipped, which a
 *  pixel comparison would otherwise read as agreement. */
inline constexpr int kWide = 420;
inline constexpr int kTall = 300;

/** @p look's custom properties alone — the spacing and the palette a
 *  component reads where it lands — with none of its role rules, so a
 *  component's own fallback registers are what is drawn. */
inline compose::StyleSheet propertiesOf(const kit::Theme& look) {
  return look.properties();
}

/** @p tree with @p look's custom properties set on it, standing over the
 *  house properties every drawing here is made under. */
inline compose::Element withProperties(
    compose::Element tree, const kit::Theme& look = kit::houseTheme()) {
  return std::move(tree.applyStyleSheet(propertiesOf(look)));
}

/** A composer over a raster surface, so a tree can be compared to another
 *  tree by the pixels the two produce. */
struct Drawn {
  sigil::motion::Engine engine;
  compose::Composer composer{engine, sigil::sketch::test::fonts()};
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWide, kTall));

  /** Under the house theme's properties, as a sketch under a stage that
   *  names no theme stands; a tree that applies another theme's sheet
   *  shadows them. */
  explicit Drawn(compose::Element tree) {
    composer.setStyleSheet(propertiesOf(kit::houseTheme()));
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
  return compose::box().width(60).height(40).fill(
      compose::Fill::color({0.9f, 0.3f, 0.4f, 1}));
}

}  // namespace sigil::sketch::kit::test
