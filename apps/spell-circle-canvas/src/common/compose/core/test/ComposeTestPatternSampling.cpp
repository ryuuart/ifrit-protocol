// How a pattern's baked tile samples: as the tile itself says unless the
// pattern states otherwise, so a cloth on a pixel grid stays on it.

#include <sigilcompose/core/Pattern.h>
#include <sigilmaterial/pattern/Weave.h>

#include "support/CoreTestSupport.h"

namespace {

constexpr int kSide = 128;
constexpr material::Color kRed = {1, 0, 0, 1};
constexpr material::Color kBlue = {0, 0, 1, 1};

/** A plain weave of one red and one blue thread each way, two pixels a
 *  thread: a tile whose own sampling is nearest. */
material::pattern::Tile checkedCloth() {
  return material::pattern::clothTile(
      {.warp = {0, 1},
       .weft = {0, 1},
       .shades = {kRed, kBlue},
       .weave = material::pattern::Weave::plain()},
      2);
}

/** How many pixels of a square filled with @p pattern, magnified eight
 *  times, are neither of the cloth's two colours — the blend a linear
 *  filter puts across every thread edge, and nearest never does. */
int blendedPixels(const Pattern& pattern) {
  Host host(kSide, kSide);
  host.composer.render(box().children({box()
                                           .width(kSide)
                                           .height(kSide)
                                           .absolute()
                                           .inset(0)
                                           .fill(pattern.material())}));
  host.frame();
  int blended = 0;
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide; ++x) {
      const SkColor colour = host.pixel(x, y);
      if (colour != SK_ColorRED && colour != SK_ColorBLUE) ++blended;
    }
  return blended;
}

}  // namespace

TEST(ComposePatternSampling, APatternSamplesAsItsTileSays) {
  ASSERT_EQ(checkedCloth().sampling(), material::Sampling::Nearest);
  Pattern cloth(checkedCloth());
  cloth.scale(8);
  EXPECT_EQ(blendedPixels(cloth), 0);
}

TEST(ComposePatternSampling, ASamplingThePatternStatesWins) {
  Pattern cloth(checkedCloth());
  cloth.scale(8).sampling(material::Sampling::Linear);
  EXPECT_GT(blendedPixels(cloth), 0);
}
