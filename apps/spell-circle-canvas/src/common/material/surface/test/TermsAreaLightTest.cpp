/** @file
 * LIGHTS WITH A SIZE: a rectangle's cosine-weighted and plain solid
 * angles meet their closed forms, far off it delivers what a point of its
 * area would, half below the horizon it delivers its upper half, a sphere
 * of radius zero is a point light, and the representative point is where
 * the ray meets the light, held inside it.
 */

#include <cmath>
#include <string>

#include "support/TermEvaluation.h"

using namespace sigil::material;
using namespace sigil::material::test;

namespace {

/** A float3 spelled for a term. */
std::string vec(float x, float y, float z) {
  return "float3(" + std::to_string(x) + ", " + std::to_string(y) + ", " +
         std::to_string(z) + ")";
}

/** The four corners of a rectangle centred @p d above the origin, square
 *  to +z, @p halfWidth along x and @p halfHeight along y, in order. */
std::string corners(float halfWidth, float halfHeight, float d) {
  return vec(-halfWidth, -halfHeight, d) + ", " +
         vec(halfWidth, -halfHeight, d) + ", " + vec(halfWidth, halfHeight, d) +
         ", " + vec(-halfWidth, halfHeight, d);
}

/** THE COSINE-WEIGHTED SOLID ANGLE OF A RECTANGLE square to the normal
 *  and centred on it at distance @p d: four times the closed form for a
 *  rectangle with one corner on the normal. */
double rectangleOnItsAxis(double halfWidth, double halfHeight, double d) {
  const double x = halfWidth / d;
  const double y = halfHeight / d;
  const double sx = std::sqrt(1.0 + x * x);
  const double sy = std::sqrt(1.0 + y * y);
  return 2.0 * (x / sx * std::atan(y / sx) + y / sy * std::atan(x / sy));
}

}  // namespace

TEST(Terms, ARectanglesCosineWeightedSolidAngleMeetsItsClosedForm) {
  const float sizes[][3] = {
      {1.0f, 0.5f, 1.0f}, {1.0f, 0.5f, 3.0f}, {0.2f, 2.0f, 0.5f}};
  for (const auto& size : sizes) {
    const double expected = rectangleOnItsAxis(size[0], size[1], size[2]);
    const float got =
        scalar("quadIrradiance(" + corners(size[0], size[1], size[2]) +
               ", float3(0.0, 0.0, 1.0))");
    EXPECT_NEAR(got, expected, 2e-3 * expected)
        << size[0] << " x " << size[1] << " at " << size[2];
    // The corners taken the other way round answer the same.
    const float reversed =
        scalar("quadIrradiance(" + vec(-size[0], size[1], size[2]) + ", " +
               vec(size[0], size[1], size[2]) + ", " +
               vec(size[0], -size[1], size[2]) + ", " +
               vec(-size[0], -size[1], size[2]) + ", float3(0.0, 0.0, 1.0))");
    EXPECT_NEAR(reversed, got, 1e-5f);
  }
}

TEST(Terms, ARectFarOffDeliversWhatAPointOfItsAreaWould) {
  // A panel of area A and radiance L, far off and facing, delivers what a
  // point of intensity L * A does: A over the distance squared per unit
  // of radiance — within one per cent at fifty times its size.
  const float distance = 50.0f;
  const float area = 1.0f * 0.5f;
  const float got = scalar("quadIrradiance(" + corners(0.5f, 0.25f, distance) +
                           ", float3(0.0, 0.0, 1.0))");
  EXPECT_NEAR(got, area / (distance * distance),
              1e-2f * area / (distance * distance));
}

TEST(Terms, ARectHalfBelowTheHorizonDeliversItsUpperHalf) {
  // A panel standing upright beside the point, its middle on the
  // horizon: what reaches the surface is exactly what the half above the
  // horizon alone delivers, and it agrees with the integral taken over
  // the panel's area directly.
  const std::string n = "float3(0.0, 0.0, 1.0)";
  const float whole =
      scalar("quadIrradiance(" + vec(1, -1, -1) + ", " + vec(1, 1, -1) + ", " +
             vec(1, 1, 1) + ", " + vec(1, -1, 1) + ", " + n + ")");
  const float upper =
      scalar("quadIrradiance(" + vec(1, -1, 0) + ", " + vec(1, 1, 0) + ", " +
             vec(1, 1, 1) + ", " + vec(1, -1, 1) + ", " + n + ")");
  EXPECT_NEAR(whole, upper, 1e-4f);
  // The integral of cos(surface) cos(panel) / r² over the upper half:
  // the panel stands at x = 1 and faces the origin.
  double integral = 0;
  const int steps = 400;
  for (int i = 0; i < steps; ++i)
    for (int j = 0; j < steps; ++j) {
      const double y = -1.0 + 2.0 * (i + 0.5) / steps;
      const double z = (j + 0.5) / steps;
      const double r2 = 1.0 + y * y + z * z;
      integral += (z / std::sqrt(r2)) * (1.0 / std::sqrt(r2)) / r2 *
                  (2.0 / steps) * (1.0 / steps);
    }
  EXPECT_NEAR(whole, integral, 2e-3 * integral);
  // A panel wholly below the horizon delivers nothing.
  EXPECT_FLOAT_EQ(
      scalar("quadIrradiance(" + vec(1, -1, -1) + ", " + vec(1, 1, -1) + ", " +
             vec(1, 1, -0.1f) + ", " + vec(1, -1, -0.1f) + ", " + n + ")"),
      0.0f);
}

TEST(Terms, ARectanglesSolidAngleMeetsItsClosedForm) {
  const float halfWidth = 1.0f;
  const float halfHeight = 0.5f;
  const float d = 2.0f;
  const double expected =
      4.0 * std::asin(halfWidth * halfHeight /
                      std::sqrt((halfWidth * halfWidth + d * d) *
                                (halfHeight * halfHeight + d * d)));
  EXPECT_NEAR(
      scalar("quadSolidAngle(" + corners(halfWidth, halfHeight, d) + ")"),
      expected, 2e-3 * expected);
  // A ball's: nothing at radius zero, pi r² / d² when small, a hemisphere
  // from inside.
  EXPECT_FLOAT_EQ(scalar("sphereSolidAngle(4.0, 0.0)"), 0.0f);
  EXPECT_NEAR(scalar("sphereSolidAngle(10.0, 0.1)"),
              3.14159265f * 0.01f / 100.0f, 1e-6f);
  EXPECT_NEAR(scalar("sphereSolidAngle(0.5, 1.0)"), 2.0f * 3.14159265f, 1e-4f);
  // A disk's on its axis: pi r² / (d² + r²).
  EXPECT_NEAR(scalar("diskSolidAngle(2.0, 1.0, 1.0)"), 3.14159265f / 5.0f,
              1e-5f);
}

TEST(Terms, ASphereOfRadiusZeroIsAPointLight) {
  // The representative point is the centre, and the highlight keeps all
  // of its peak: the sized path reduces to the point light's.
  const std::string centre = vec(1.0f, 2.0f, 3.0f);
  const std::string ray = "normalize(float3(0.3, 0.2, 1.0))";
  for (const char axis : {'x', 'y', 'z'})
    EXPECT_FLOAT_EQ(
        scalar("spherePoint(" + centre + ", 0.0, " + ray + ")." + axis),
        scalar(centre + "." + axis));
  for (const float rough : {0.0f, 0.2f, 0.7f})
    EXPECT_FLOAT_EQ(
        scalar("areaNormalization(" + std::to_string(rough) + ", 0.0)"), 1.0f);
  // A larger light keeps less of a smooth surface's peak, and a rough
  // lobe, already as wide as it goes, keeps it all.
  EXPECT_LT(scalar("areaNormalization(0.2, 0.1)"),
            scalar("areaNormalization(0.2, 0.01)"));
  EXPECT_FLOAT_EQ(scalar("areaNormalization(1.0, 0.5)"), 1.0f);
}

TEST(Terms, TheRepresentativePointIsWhereTheRayMeetsTheLightHeldInsideIt) {
  // A rect 2 wide and 1 tall, 2 above, facing down.
  const std::string rect =
      "float3(0.0, 0.0, 2.0), float3(0.0, 0.0, -1.0), "
      "float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), "
      "1.0, 0.5, ";
  // A ray that meets the panel answers where it meets it…
  EXPECT_NEAR(
      scalar("rectPoint(" + rect + "normalize(float3(0.5, 0.25, 2.0))).x"),
      0.5f, 1e-5f);
  EXPECT_NEAR(
      scalar("rectPoint(" + rect + "normalize(float3(0.5, 0.25, 2.0))).y"),
      0.25f, 1e-5f);
  // …and one that passes beside it, the nearest edge.
  EXPECT_NEAR(
      scalar("rectPoint(" + rect + "normalize(float3(3.0, 0.25, 2.0))).x"),
      1.0f, 1e-5f);
  EXPECT_NEAR(
      scalar("rectPoint(" + rect + "normalize(float3(0.0, 3.0, 2.0))).y"), 0.5f,
      1e-5f);
  // A disk's is held inside its radius.
  const float x = scalar(
      "diskPoint(float3(0.0, 0.0, 2.0), float3(0.0, 0.0, -1.0), 0.5, "
      "normalize(float3(3.0, 4.0, 2.0))).x");
  const float y = scalar(
      "diskPoint(float3(0.0, 0.0, 2.0), float3(0.0, 0.0, -1.0), 0.5, "
      "normalize(float3(3.0, 4.0, 2.0))).y");
  EXPECT_NEAR(std::sqrt(x * x + y * y), 0.5f, 1e-5f);
  EXPECT_NEAR(x / y, 0.75f, 1e-4f);
}
