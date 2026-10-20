/** @file
 * THE SHEEN: its table is the integral of the lobe the lights shade with,
 * a sheen over a white surface in a white furnace returns no more than it
 * got, and it brightens the rim more than the face.
 */

#include <cstddef>
#include <string>
#include <vector>

#include "support/TermEvaluation.h"

using namespace sigil::material;
using namespace sigil::material::test;

namespace {

/** THE SHEEN'S DIRECTIONAL ALBEDO, integrated in the shading language:
 *  `directSheen` itself for a white sheen of @p sheenRoughness seen at
 *  @p nDotV, out of a uniform white surround, over an even n-by-n grid of
 *  the hemisphere weighted by solid angle. */
double sheenAlbedoIntegrated(float sheenRoughness, float nDotV, int n) {
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
      "  float3 f = directSheen(float3(0.0, 0.0, 1.0), l, v, float3(1.0), " +
      std::to_string(sheenRoughness) +
      ");\n"
      "  return half4(half(f.r * s), 0.0, 0.0, 1.0);\n"
      "}\n";
  double sum = 0;
  for (const SkColor4f& cell : evaluated(body, n)) sum += cell.fR;
  return sum * 1.5707963267948966 * 6.283185307179586 / ((double)n * (double)n);
}

}  // namespace

TEST(Terms, TheSheenTableIsTheIntegralOfTheLobeTheLightsShadeWith) {
  // The table is integrated on the host; here it is held to the sheen
  // integrated through `directSheen` itself, in the shading language.
  for (const float roughness : {0.2f, 0.5f, 0.8f, 1.0f})
    for (const float nDotV : {0.2f, 0.5f, 0.8f, 1.0f}) {
      const double integrated = sheenAlbedoIntegrated(roughness, nDotV, 256);
      EXPECT_NEAR(sheenAlbedo(roughness, nDotV), integrated, 0.01)
          << roughness << " " << nDotV;
    }
  const std::vector<float>& table = sheenAlbedoTable();
  ASSERT_EQ(table.size(), (size_t)kEnvironmentBrdfSize * kEnvironmentBrdfSize);
  for (const float value : table) {
    EXPECT_GE(value, 0.0f);
    EXPECT_LE(value, 1.0f);
  }
  // Away from grazing the sheen's albedo is under one without the hold.
  for (const float roughness : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    for (const float nDotV : {0.1f, 0.3f, 0.6f, 1.0f})
      EXPECT_LE(sheenAlbedo(roughness, nDotV), 1.0f)
          << roughness << " " << nDotV;
}

TEST(Terms, ASheenOverAWhiteSurfaceInAWhiteFurnaceReturnsNoMoreThanItGot) {
  // A white sheen reflects its albedo of a uniform white surround and the
  // white surface beneath gives up that share: the two together never
  // exceed what arrived, within the table's own rounding, at any
  // roughness of either or any angle.
  for (const float sheenRoughness : {0.0f, 0.3f, 0.6f, 1.0f})
    for (const float roughness : {0.0f, 0.5f, 1.0f})
      for (const float nDotV : {0.1f, 0.5f, 1.0f}) {
        const float albedo = sheenAlbedo(sheenRoughness, nDotV);
        const std::string at = std::to_string(roughness) + ", " +
                               std::to_string(nDotV) + ", " +
                               brdfAt(roughness, nDotV);
        const std::string scaled =
            "sheenScaling(float3(1.0), " + std::to_string(albedo) + ", 0.0)";
        const glm::vec3 total =
            shaded("environmentSheen(float3(1.0), float3(1.0), " +
                   std::to_string(albedo) +
                   ") + (environmentSpecular(float3(1.0), "
                   "float3(0.04), " +
                   at +
                   ") + environmentDiffuse(float3(1.0), "
                   "float3(0.96), float3(0.04), " +
                   at + ")) * " + scaled);
        EXPECT_LE(total.x, 1.005f) << sheenRoughness << " " << at;
        EXPECT_GT(total.x, 0.85f) << sheenRoughness << " " << at;
      }
}

TEST(Terms, ASheenBrightensTheRimMoreThanTheFace) {
  // Lit from behind the eye, a sheen sends back more at a grazing view
  // than head on: that is the soft edge velvet shows.
  const float face = scalar(
      "directSheen(float3(0.0, 0.0, 1.0), float3(0.0, 0.0, 1.0), "
      "float3(0.0, 0.0, 1.0), float3(1.0), 0.5).r");
  const float rim = scalar(
      "directSheen(float3(0.0, 0.0, 1.0), normalize(float3(1.0, 0.0, 0.25)), "
      "normalize(float3(1.0, 0.0, 0.25)), float3(1.0), 0.5).r");
  EXPECT_GT(rim, face * 2.0f);
}
