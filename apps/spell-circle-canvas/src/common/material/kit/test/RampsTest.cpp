/** @file
 * The named ramps: each begins and ends on the colour its published
 * table begins and ends on, the sequential ones climb in lightness the
 * whole way, the rainbow does not, the diverging pair is lightest in the
 * middle, and the helix is generated from its props.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/kit/Ramps.h>

#include <cmath>
#include <vector>

using namespace sigil::material;

namespace {

/** The lightness of the ramp at 33 evenly spaced positions. */
std::vector<float> climb(const Ramp& ramp) {
  std::vector<float> lightness;
  for (int i = 0; i < 33; ++i)
    lightness.push_back(toOklab(ramp.at((float)i / 32.0f)).L);
  return lightness;
}

void expectRising(const Ramp& ramp) {
  const std::vector<float> lightness = climb(ramp);
  for (size_t i = 1; i < lightness.size(); ++i)
    EXPECT_GT(lightness[i], lightness[i - 1]) << "at step " << i;
}

}  // namespace

TEST(Ramps, TheEndsAreTheColoursTheTablesArePublishedWith) {
  EXPECT_EQ(kit::viridis().at(0.0f), rgb(0x440154));
  EXPECT_EQ(kit::viridis().at(1.0f), rgb(0xfde725));
  EXPECT_EQ(kit::magma().at(0.0f), rgb(0x000004));
  EXPECT_EQ(kit::magma().at(1.0f), rgb(0xfcfdbf));
  EXPECT_EQ(kit::inferno().at(0.0f), rgb(0x000004));
  EXPECT_EQ(kit::inferno().at(1.0f), rgb(0xfcffa4));
  EXPECT_EQ(kit::plasma().at(0.0f), rgb(0x0d0887));
  EXPECT_EQ(kit::plasma().at(1.0f), rgb(0xf0f921));
  EXPECT_EQ(kit::turbo().at(0.0f), rgb(0x30123b));
  EXPECT_EQ(kit::turbo().at(1.0f), rgb(0x7a0403));
  // The middle of viridis is its teal, which is the sample every
  // reproduction of the map is checked against.
  EXPECT_LT(deltaE(kit::viridis().at(0.5f), rgb(0x21918c)), 1.0f);
}

TEST(Ramps, TheSequentialMapsClimbInLightnessTheWholeWay) {
  // The property that makes them worth naming: a difference in the data
  // is a difference an eye reports, everywhere along the map, with no
  // false edge where the hue turns.
  expectRising(kit::viridis());
  expectRising(kit::magma());
  expectRising(kit::inferno());
  expectRising(kit::plasma());
}

TEST(Ramps, TheRainbowIsBrightestInItsMiddleAndSaysNothingAboutMore) {
  const std::vector<float> lightness = climb(kit::turbo());
  EXPECT_GT(lightness[16], lightness.front());
  EXPECT_GT(lightness[16], lightness.back());
  // Which is the whole caution: two values either side of the middle
  // read as equally bright, so the map cannot be used for a quantity.
  EXPECT_NEAR(lightness[8], lightness[24], 0.25f);
}

TEST(Ramps, TheDivergingPairIsPaleInTheMiddleAndOpposedAtItsEnds) {
  for (const Ramp& ramp : {kit::redBlue(), kit::brownTeal()}) {
    const std::vector<float> lightness = climb(ramp);
    EXPECT_GT(lightness[16], lightness.front());
    EXPECT_GT(lightness[16], lightness.back());
    // The middle is where the value is neither, so it carries almost no
    // colour at all.
    EXPECT_LT(toOklch(ramp.at(0.5f)).chroma, 0.02f);
    // And the two ends are two different colours, not two depths of one.
    const float low = toOklch(ramp.at(0.0f)).hueDegrees;
    const float high = toOklch(ramp.at(1.0f)).hueDegrees;
    float apart = std::fmod(std::abs(low - high), 360.0f);
    if (apart > 180.0f) apart = 360.0f - apart;
    EXPECT_GT(apart, 90.0f);
  }
}

TEST(Ramps, TheHelixIsItsPropsAndStillReadsAsGrey) {
  const Ramp helix = kit::cubehelix();
  EXPECT_EQ(helix.stops.size(), 32u);
  EXPECT_NEAR(helix.at(0.0f).r, 0.0f, 1e-6f);
  EXPECT_NEAR(helix.at(1.0f).r, 1.0f, 1e-6f);
  // What it was built for: printed in grey it is still a ramp, because
  // the lightness climbs whatever the hue is doing.
  expectRising(helix);
  // No chroma at all is the grey ramp, exactly.
  const Ramp grey = kit::cubehelix({.hue = 0.0f});
  for (int i = 0; i < 8; ++i) {
    const Color c = grey.at((float)i / 7.0f);
    EXPECT_FLOAT_EQ(c.r, c.g);
    EXPECT_FLOAT_EQ(c.g, c.b);
  }
  // The props are the value: two helices asked for differently are two
  // different ramps, and two asked for alike are the same one.
  EXPECT_EQ(kit::cubehelix(), kit::cubehelix());
  EXPECT_NE(kit::cubehelix({.rotations = 1.0f}), kit::cubehelix());
}
