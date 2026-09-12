/** @file
 * The globe: the disc is inscribed in the node and nothing outside it is
 * painted, the sky stands over the ground, the attitude turns the sphere
 * under a fixed eye, the graticule's pitch is a prop, and the limb
 * darkens the way a ball does — and the reading is one text, crossed, so
 * a device and a raster surface shade the same ball.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilmaterial/kit/Globe.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cmath>
#include <string>

#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::differing;
using sigil::material::test::luminance;
using sigil::material::test::render;

namespace {

/** The globe with its rules silenced, for a case asking about colour or
 *  shading rather than about the graticule. */
kit::GlobeParameters unruled() {
  kit::GlobeParameters p;
  p.minorWeight = 0;
  p.majorWeight = 0;
  p.horizonWeight = 0;
  return p;
}

/** How many pixels of a rendered globe are within a hair of the grid's
 *  own colour — the graticule's own coverage. */
int ruled(const SkBitmap& picture) {
  int n = 0;
  for (int y = 0; y < picture.height(); ++y)
    for (int x = 0; x < picture.width(); ++x)
      n += luminance(picture.getColor(x, y)) > 200;
  return n;
}

}  // namespace

TEST(Globe, TheDiscIsInscribedInTheNodeAndNothingOutsideItIsPainted) {
  const Material globe = kit::globe();
  EXPECT_TRUE(skia::shader(globe, {}));
  EXPECT_TRUE(globe.recipe().reads(FrameInput::Resolution));
  EXPECT_TRUE(globe.geometryDependent());
  // One arithmetic, both languages: the reading is written in Slang and
  // crossed, and each target's body is that reading plus its own entry.
  EXPECT_TRUE(globe.recipe().has(Target::Slang));
  EXPECT_TRUE(globe.recipe().has(Target::SkSL));
  EXPECT_NE(
      globe.recipe().source(Target::SkSL).find("float4 globeAt(float2 p)"),
      std::string::npos);
  EXPECT_NE(
      globe.recipe().source(Target::Slang).find("float4 globeAt(float2 p)"),
      std::string::npos);

  const SkBitmap picture = render(globe, 64, 64);
  // The corners are outside the inscribed disc and carry no coverage;
  // the centre is opaque.
  EXPECT_EQ(SkColorGetA(picture.getColor(0, 0)), 0u);
  EXPECT_EQ(SkColorGetA(picture.getColor(63, 0)), 0u);
  EXPECT_EQ(SkColorGetA(picture.getColor(63, 63)), 0u);
  EXPECT_EQ(SkColorGetA(picture.getColor(32, 32)), 255u);
  // The rim is feathered rather than cut: a pixel just inside the edge
  // is neither clear nor solid.
  const uint32_t rim = SkColorGetA(picture.getColor(1, 32));
  EXPECT_GT(rim, 0u);
  EXPECT_LT(rim, 255u);
}

TEST(Globe, TheSkyStandsOverTheGroundAtALevelAttitude) {
  const SkBitmap picture = render(kit::globe(unruled()), 64, 64);
  const SkColor above = picture.getColor(32, 12);
  const SkColor below = picture.getColor(32, 52);
  // Blue over brown: the upper half is bluest, the lower half reddest.
  EXPECT_GT(SkColorGetB(above), SkColorGetR(above));
  EXPECT_GT(SkColorGetR(below), SkColorGetB(below));
  // Each hemisphere darkens toward its own pole, which is what the two
  // pole colours are for.
  EXPECT_LT(luminance(picture.getColor(32, 6)),
            luminance(picture.getColor(32, 28)));
}

TEST(Globe, RollTurnsThePictureAndPitchTurnsTheSphereUnderIt) {
  kit::GlobeParameters level = unruled();
  kit::GlobeParameters rolled = level;
  rolled.roll = 3.14159265f;
  const SkBitmap upright = render(kit::globe(level), 64, 64);
  const SkBitmap over = render(kit::globe(rolled), 64, 64);
  // Half a turn of roll puts the ground on top.
  const SkColor top = over.getColor(32, 12);
  EXPECT_GT(SkColorGetR(top), SkColorGetB(top));

  // Pitch slides the horizon: pitched a quarter turn, the pole faces the
  // eye and the whole disc reads as one hemisphere.
  kit::GlobeParameters pitched = level;
  pitched.pitch = 1.5707963f;
  const SkBitmap nose = render(kit::globe(pitched), 64, 64);
  EXPECT_GT(SkColorGetB(nose.getColor(32, 12)),
            SkColorGetR(nose.getColor(32, 12)));
  EXPECT_GT(SkColorGetB(nose.getColor(32, 52)),
            SkColorGetR(nose.getColor(32, 52)));
  EXPECT_GT(differing(upright, nose), 500);
}

TEST(Globe, YawSpinsTheGraticuleWithoutMovingTheHorizon) {
  kit::GlobeParameters still;
  kit::GlobeParameters turned = still;
  turned.yaw = 0.0872665f;  // half the fine graticule's pitch
  const SkBitmap first = render(kit::globe(still), 96, 96);
  const SkBitmap second = render(kit::globe(turned), 96, 96);
  EXPECT_GT(differing(first, second), 100);
  // The horizon is the sphere's own equator and yaw turns about the
  // poles, so it has not moved: the row through the centre reads the
  // same brightness either way.
  EXPECT_NEAR(luminance(first.getColor(48, 48)),
              luminance(second.getColor(48, 48)), 2);
}

TEST(Globe, TheGraticulesPitchIsAProp) {
  kit::GlobeParameters fine;
  kit::GlobeParameters coarse = fine;
  coarse.minorDeg = 30.0f;
  const SkBitmap ruledFine = render(kit::globe(fine), 96, 96);
  const SkBitmap ruledCoarse = render(kit::globe(coarse), 96, 96);
  EXPECT_GT(ruled(ruledFine), ruled(ruledCoarse));
  // And silencing the weights removes the rules without touching the
  // hemispheres.
  const SkBitmap bare = render(kit::globe(unruled()), 96, 96);
  EXPECT_LT(ruled(bare), ruled(ruledCoarse));
}

TEST(Globe, TheLimbDarkensTheWayABallDoes) {
  kit::GlobeParameters flat = unruled();
  const Color one = {0.6f, 0.6f, 0.6f, 1};
  flat.sky = flat.skyPole = flat.ground = flat.groundPole = one;
  flat.specular = 0;
  const SkBitmap picture = render(kit::globe(flat), 128, 128);
  // One colour everywhere, so what is left is the falloff alone: the
  // centre keeps ambient + diffuse, a point nine tenths of the way out
  // keeps ambient + diffuse * z.
  const int middle = luminance(picture.getColor(64, 64));
  const int outer = luminance(picture.getColor(64 + 57, 64));
  EXPECT_GT(middle, outer);
  const float z = std::sqrt(1.0f - 0.891f * 0.891f);  // 57 / 64
  EXPECT_NEAR((float)outer / (float)middle,
              (flat.ambient + flat.diffuse * z) / (flat.ambient + flat.diffuse),
              0.03f);
}
