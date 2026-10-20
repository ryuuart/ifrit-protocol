/** @file
 * THE TONE CURVE: the exposure multiplies the radiance before the curve,
 * and the curve never reaches white.
 */

#include "support/TermEvaluation.h"

using namespace sigil::material;
using namespace sigil::material::test;

TEST(Terms, TheExposureMultipliesTheRadianceBeforeTheCurve) {
  // Twice the exposure over half the radiance is the same colour, which
  // is what makes the dial an exposure rather than a gain on the result.
  EXPECT_NEAR(scalar("toneMap(float3(0.5, 0.5, 0.5), 2.0).r"),
              scalar("toneMap(float3(1.0, 1.0, 1.0), 1.0).r"), 2e-3f);
}

TEST(Terms, TheToneCurveNeverReachesWhite) {
  EXPECT_LT(scalar("toneMap(float3(100.0, 100.0, 100.0), 1.0).r"), 1.0f);
}
