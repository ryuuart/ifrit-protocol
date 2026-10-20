/** @file
 * THE LIGHT FROM EVERYWHERE: the environment falls off with roughness,
 * the panorama's middle is straight ahead, the specular lobe never makes
 * light, the split-sum table stands within a stated distance of the
 * lobe's own integral wherever it is read, and once the light a rough
 * metal scatters more than once is put back, a white furnace returns the
 * light it was lit by.
 */

#include <cstddef>
#include <string>
#include <vector>

#include "support/TermEvaluation.h"

using namespace sigil::material;
using namespace sigil::material::test;

TEST(Terms, ARoughSurfaceTakesLessOfTheEnvironmentThanASmoothOne) {
  EXPECT_LT(scalar("environmentSpecular(float3(1.0, 1.0, 1.0), "
                   "float3(0.04, 0.04, 0.04), 1.0, 1.0, " +
                   brdfAt(1.0f, 1.0f) + ").r"),
            scalar("environmentSpecular(float3(1.0, 1.0, 1.0), "
                   "float3(0.04, 0.04, 0.04), 0.0, 1.0, " +
                   brdfAt(0.0f, 1.0f) + ").r"));
}

TEST(Terms, LookingAlongMinusZIsTheMiddleOfThePanorama) {
  const SkColor4f forward =
      term("equirectangularUv(float3(0.0, 0.0, -1.0)), 0.0, 1.0");
  EXPECT_NEAR(forward.fR, 0.5f, 2e-3f);
  EXPECT_NEAR(forward.fG, 0.5f, 2e-3f);
}

namespace {

/** THE SPECULAR LOBE'S DIRECTIONAL ALBEDO, integrated: the light a
 *  surface of @p roughness reflects toward an eye at @p nDotV out of a
 *  uniform white surround, through `directSpecular` itself, with Fresnel
 *  running from @p f0 to one. The hemisphere is cut into n² cells even in
 *  the polar angle and the azimuth, each weighted by the solid angle it
 *  covers, which keeps a narrow lobe several cells wide. */
double specularAlbedo(float roughness, float nDotV, float f0, int n) {
  const std::string cells = std::to_string(n) + ".0";
  const std::string body =
      "half4 main(float2 xy) {\n"
      "  float theta = 1.57079633 * xy.y / " +
      cells +
      ";\n"
      "  float phi = 6.28318531 * xy.x / " +
      cells +
      ";\n"
      "  float s = sin(theta);\n"
      "  float3 l = float3(s * cos(phi), s * sin(phi), cos(theta));\n"
      "  float nv = " +
      std::to_string(nDotV) +
      ";\n"
      "  float3 v = float3(sqrt(1.0 - nv * nv), 0.0, nv);\n"
      "  float3 f = directSpecular(float3(0.0, 0.0, 1.0), l, v, float3(" +
      std::to_string(f0) + "), float3(1.0), " + std::to_string(roughness) +
      ");\n"
      "  return half4(half(f.r * s), 0.0, 0.0, 1.0);\n"
      "}\n";
  double sum = 0;
  for (const SkColor4f& cell : evaluated(body, n)) sum += cell.fR;
  return sum * 1.5707963267948966 * 6.283185307179586 / ((double)n * (double)n);
}

}  // namespace

TEST(Terms, TheSpecularLobeNeverReflectsMoreThanItReceives) {
  // The single-bounce lobe loses light as it roughens — what leaves after
  // a second bounce is not in it — and it never makes any.
  for (const float roughness : {0.35f, 0.6f, 1.0f})
    for (const float nDotV : {0.25f, 0.6f, 1.0f}) {
      const double albedo = specularAlbedo(roughness, nDotV, 1.0f, 512);
      EXPECT_LE(albedo, 1.01) << roughness << " " << nDotV;
      EXPECT_GT(albedo, 0.25) << roughness << " " << nDotV;
    }
}

TEST(Terms, TheSplitSumTableIsTheIntegralOfTheLobeTheLightsShadeWith) {
  // The table is integrated on the host by importance sampling; here it
  // is held to the same lobe integrated through `directSpecular` itself,
  // evaluated in the shading language over the whole hemisphere. Its
  // scale plus bias is the light a white surface of that roughness keeps
  // in one bounce, and its bias alone what a black one keeps at its rim.
  for (const float roughness : {0.35f, 0.5f, 0.75f, 1.0f})
    for (const float nDotV : {0.2f, 0.5f, 0.8f, 1.0f}) {
      const glm::vec2 table = environmentBrdf(roughness, nDotV);
      const double white = specularAlbedo(roughness, nDotV, 1.0f, 512);
      const double black = specularAlbedo(roughness, nDotV, 0.0f, 512);
      EXPECT_NEAR(table.x + table.y, white, 0.015) << roughness << " " << nDotV;
      EXPECT_NEAR(table.y, black, 0.015) << roughness << " " << nDotV;
    }
  // A mirror seen head on keeps everything and adds nothing at its rim.
  EXPECT_NEAR(environmentBrdf(0.0f, 1.0f).x, 1.0f, 0.01f);
  EXPECT_NEAR(environmentBrdf(0.0f, 1.0f).y, 0.0f, 0.01f);
}

TEST(Terms, TheTableReadBetweenItsTexelsStaysWithinTwoHundredthsOfTheIntegral) {
  // Read halfway between texels — the worst place for a bilinear reading —
  // the table still stands within two hundredths of the integral.
  const std::vector<glm::vec2>& table = environmentBrdfTable();
  const int size = kEnvironmentBrdfSize;
  ASSERT_EQ(table.size(), (size_t)size * size);
  const float last = (float)(size - 1);
  for (const int y : {3, 10, 20, 29})
    for (const int x : {1, 6, 15, 27}) {
      const glm::vec2 read =
          (table[(size_t)y * size + x] + table[(size_t)y * size + x + 1] +
           table[(size_t)(y + 1) * size + x] +
           table[(size_t)(y + 1) * size + x + 1]) *
          0.25f;
      const glm::vec2 exact =
          environmentBrdf(((float)y + 0.5f) / last, ((float)x + 0.5f) / last);
      EXPECT_NEAR(read.x, exact.x, 0.02f) << x << " " << y;
      EXPECT_NEAR(read.y, exact.y, 0.02f) << x << " " << y;
    }
}

TEST(Terms, AWhiteMetalInAWhiteFurnaceReturnsTheLightItWasLitBy) {
  // With the light it scatters more than once put back, a white metal
  // under a uniform white surround returns all of it at every roughness
  // and every angle, and a white dielectric no more than that.
  for (const float roughness : {0.0f, 0.4f, 0.7f, 1.0f})
    for (const float nDotV : {0.2f, 0.6f, 1.0f}) {
      const std::string at = std::to_string(roughness) + ", " +
                             std::to_string(nDotV) + ", " +
                             brdfAt(roughness, nDotV);
      const glm::vec3 metal = shaded(
          "environmentSpecular(float3(1.0), float3(1.0), " + at +
          ") + environmentDiffuse(float3(1.0), float3(0.0), float3(1.0), " +
          at + ")");
      EXPECT_NEAR(metal.x, 1.0f, 5e-3f) << at;
      const glm::vec3 dielectric = shaded(
          "environmentSpecular(float3(1.0), float3(0.04), " + at +
          ") + environmentDiffuse(float3(1.0), float3(0.96), float3(0.04), " +
          at + ")");
      EXPECT_LE(dielectric.x, 1.005f) << at;
      EXPECT_GT(dielectric.x, 0.9f) << at;
    }
}

TEST(Terms, TheLightARoughMetalScattersMoreThanOnceIsPutBack) {
  // One bounce alone darkens a rough white metal; the composition puts
  // back what it lost, and a mirror loses nothing to put back.
  const float oneBounce =
      scalar("environmentSpecular(float3(1.0), float3(1.0), 1.0, 0.5, " +
             brdfAt(1.0f, 0.5f) + ").r");
  const float putBack = scalar(
      "environmentDiffuse(float3(1.0), float3(0.0), float3(1.0), 1.0, 0.5, " +
      brdfAt(1.0f, 0.5f) + ").r");
  EXPECT_LT(oneBounce, 0.9f);
  EXPECT_NEAR(oneBounce + putBack, 1.0f, 5e-3f);
  EXPECT_LT(scalar("environmentDiffuse(float3(1.0), float3(0.0), "
                   "float3(1.0), 0.0, 1.0, " +
                   brdfAt(0.0f, 1.0f) + ").r"),
            0.02f);
}
