/** @file
 * A material built up by composition, lowered to one Skia paint: a colour
 * base with a screened layer, a layer through a mask, a gradient base, and
 * the same build twice compared equal as paints.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::render;

namespace {

SkColor centre(const Material& material) {
  const Paint paint = skia::paint(material);
  if (paint.isSolid()) return skia::toSkColor(paint.solidColor()).toSkColor();
  const SkBitmap pixels = render(skia::shader(paint, {.resolution = {4, 4}}));
  return pixels.getColor(2, 2);
}

}  // namespace

TEST(SkiaMaterial, ALayerBlendsOverItsBaseAtItsOpacity) {
  const Material flat = Color{1, 0, 0, 1};
  EXPECT_EQ(SK_ColorRED, centre(flat));
  const Material covered = from(Color{1, 0, 0, 1}).layer(Color{0, 0, 1, 1});
  EXPECT_EQ(SK_ColorBLUE, centre(covered));
  const Material half =
      from(Color{1, 0, 0, 1}).layer(Color{0, 0, 1, 1}, {.opacity = 0.5f});
  const SkColor mixed = centre(half);
  EXPECT_NEAR(128, SkColorGetR(mixed), 2);
  EXPECT_NEAR(128, SkColorGetB(mixed), 2);
}

TEST(SkiaMaterial, AMaskSaysWhereALayerApplies) {
  const Material nowhere = from(Color{1, 0, 0, 1})
                               .layer(Color{0, 0, 1, 1},
                                      {.mask = Mask{.source = Color{0, 0, 0, 0}}});
  EXPECT_EQ(SK_ColorRED, centre(nowhere));
  const Material inverted =
      from(Color{1, 0, 0, 1})
          .layer(Color{0, 0, 1, 1},
                 {.mask = Mask{.source = Color{0, 0, 0, 0}, .invert = true}});
  EXPECT_EQ(SK_ColorBLUE, centre(inverted));
}

TEST(SkiaMaterial, AGradientIsABaseAndComparesByValue) {
  const auto build = [] {
    return from(linearGradient({0, 0}, {1, 0},
                               {Color{0, 0, 0, 1}, Color{1, 1, 1, 1}}))
        .layer(Color{1, 1, 1, 1}, {.blend = BlendMode::Multiply});
  };
  EXPECT_EQ(build(), build());
  EXPECT_EQ(skia::paint(build()), skia::paint(build()));
  EXPECT_NE(build(), Material(Color{1, 1, 1, 1}));
}

TEST(SkiaMaterial, EffectsAreReadBackAndPartOfTheValue) {
  const Material raised =
      from(Color{0.5f, 0.5f, 0.5f, 1})
          .effects(Filter::shadow({0, 0, 0, 0.5f}, {.blur = 4, .offset = {0, 2}})
                       .then(Filter::stroke({1, 1, 1, 1}, {.width = 2})));
  ASSERT_NE(nullptr, raised.effects());
  ASSERT_EQ(2u, raised.effects()->coverage().size());
  EXPECT_EQ(CoverageEffect::Kind::Stroke, raised.effects()->coverage()[1].kind);
  EXPECT_TRUE(raised.effects()->withoutCoverage().isNone());
  EXPECT_NE(raised, raised.base());
}

TEST(SkiaMaterial, TheDesignatedFormIsTheChainInOnePairOfBraces) {
  const Filter shadow = Filter::shadow(Color{0, 0, 0, 0.5f}, {.blur = 4});
  const Material chained = from(Color{0.2f, 0.3f, 0.4f, 1})
                               .layer(Color{1, 1, 1, 0.5f},
                                      {.blend = BlendMode::Screen})
                               .surface({.metallic = 1.0f})
                               .effects(shadow);
  const Material designated =
      from({.base = Color{0.2f, 0.3f, 0.4f, 1},
            .layers = {{Color{1, 1, 1, 0.5f}, {.blend = BlendMode::Screen}}},
            .surface = SurfaceOptions{.metallic = 1.0f},
            .effects = shadow});
  EXPECT_EQ(chained, designated);
  ASSERT_TRUE(designated.effects());
  EXPECT_EQ(shadow, *designated.effects());
  EXPECT_FALSE(from({.base = Color{1, 0, 0, 1}}).effects())
      << "no effects stated, none held";
}

TEST(SkiaMaterial, ABlurMapIsWrittenAsAMaterial) {
  const Material falloff =
      linearGradient({0, 0}, {1, 0}, {{0, {0, 0, 0, 1}}, {1, {1, 1, 1, 1}}});
  EXPECT_EQ(Filter::blur(skia::paint(falloff), 12.0f),
            Filter::blur(falloff, 12.0f));
}
