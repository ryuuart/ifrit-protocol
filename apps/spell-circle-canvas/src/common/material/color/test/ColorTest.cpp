/** @file
 * The colour feature: the sRGB curve round-trips, and so does OKLab.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/color/Color.h>

using namespace sigil::material;

TEST(Color, SrgbRoundTrips) {
  for (float v : {0.0f, 0.02f, 0.25f, 0.5f, 0.75f, 1.0f})
    EXPECT_NEAR(linearToSrgb(srgbToLinear(v)), v, 1e-5f);
  const Color mid{0.5f, 0.5f, 0.5f, 1};
  const Color back = fromOklab(toOklab(mid));
  EXPECT_NEAR(back.r, 0.5f, 1e-4f);
  EXPECT_NEAR(back.g, 0.5f, 1e-4f);
}
