#include <gtest/gtest.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/testing/Scene.h>
#include <sigilmaterial/color/Color.h>

#include "Fonts.h"

namespace compose = sigil::compose;
namespace material = sigil::material;

TEST(ComposeTestScene, AReadbackKeepsItsPixelsAfterTheNextDescription) {
  compose::test::Scene scene(sigil::test::fonts(), 40, 30);
  const auto square = [](material::Color color) {
    return compose::box().width(10).height(10).fill(color);
  };
  scene.composer.render(square(material::hexColor(0xff0000)));
  scene.frame();
  const SkBitmap first = scene.pixels();
  EXPECT_EQ(scene.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(scene.pixel(20, 20), SK_ColorBLACK);

  scene.composer.render(square(material::hexColor(0x0000ff)));
  scene.frame(0.25);
  EXPECT_EQ(scene.pixel(5, 5), SK_ColorBLUE);
  EXPECT_EQ(first.getColor(5, 5), SK_ColorRED);
}

TEST(ComposeTestScene, AFrameUsesOnlyTheTimeTheCallerAdvances) {
  compose::test::Scene scene(sigil::test::fonts(), 40, 30);
  double elapsed = -1;
  scene.composer.render(
      compose::custom("clock", [&](auto&,
                                   const compose::PaintContext& context) {
        elapsed = context.elapsedSeconds;
      }).cache(compose::Cache::None));
  scene.frame();
  EXPECT_DOUBLE_EQ(elapsed, 0);
  scene.frame(0.25);
  EXPECT_DOUBLE_EQ(elapsed, 0.25);
  scene.frame();
  EXPECT_DOUBLE_EQ(elapsed, 0.25);
  scene.frame(0.5);
  EXPECT_DOUBLE_EQ(elapsed, 0.75);
}
