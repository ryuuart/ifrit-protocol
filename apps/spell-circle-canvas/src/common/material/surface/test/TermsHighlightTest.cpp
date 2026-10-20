/** @file
 * THE SPECULAR HIGHLIGHT'S OWN DIALS: unbrushed it is the isotropic one,
 * brushed it stretches along its tangent, a thin film changes its colour
 * with its thickness and the angle, and no specular strength removes a
 * dielectric's highlight.
 */

#include <cmath>
#include <string>

#include "support/TermEvaluation.h"

using namespace sigil::material;
using namespace sigil::material::test;

TEST(Terms, AnUnbrushedHighlightIsTheIsotropicOne) {
  // At no strength the brushed specular is `directSpecular` exactly,
  // whichever way the tangent lies.
  for (const char* tangent :
       {"float3(1.0, 0.0, 0.0)", "normalize(float3(1.0, 1.0, 0.0))"})
    for (const char* light : {"normalize(float3(0.4, 0.1, 1.0))",
                              "normalize(float3(-0.6, 0.5, 0.7))"}) {
      const std::string common =
          std::string("float3(0.0, 0.0, 1.0), ") + "%T" + light +
          ", normalize(float3(0.1, -0.3, 1.0)), float3(0.04), float3(1.0), "
          "0.35";
      std::string brushed = "directSpecularAnisotropic(" + common + ", 0.0)";
      std::string plain = "directSpecular(" + common + ")";
      brushed.replace(brushed.find("%T"), 2, std::string(tangent) + ", ");
      plain.replace(plain.find("%T"), 2, "");
      EXPECT_NEAR(scalar(brushed + ".r"), scalar(plain + ".r"), 1e-4f)
          << tangent << " " << light;
    }
}

TEST(Terms, ABrushedHighlightStretchesAlongItsTangent) {
  // The half vector tipped toward the tangent stays in the highlight long
  // after the same tip toward the bitangent has left it.
  const std::string a = "anisotropicWidth(0.3, 0.8), microfacetWidth(0.3)";
  const float alongTangent = scalar(
      "distributionGgxAnisotropic(0.9396926, 0.3420201, 0.0, " + a + ")");
  const float alongBitangent = scalar(
      "distributionGgxAnisotropic(0.9396926, 0.0, 0.3420201, " + a + ")");
  EXPECT_GT(alongTangent, alongBitangent * 4.0f);
}

TEST(Terms, AFilmChangesItsColourWithItsThicknessAndTheAngle) {
  // The film tints a dielectric's reflection, and the tint is not the
  // same at two thicknesses or at two angles.
  const glm::vec3 thin =
      shaded("iridescenceFresnel(1.3, 0.9, 250.0, float3(0.04))");
  const glm::vec3 thick =
      shaded("iridescenceFresnel(1.3, 0.9, 500.0, float3(0.04))");
  const glm::vec3 grazing =
      shaded("iridescenceFresnel(1.3, 0.3, 250.0, float3(0.04))");
  const auto hue = [](const glm::vec3& c) {
    return glm::vec3(c.x, c.y, c.z) / std::fmax(c.x + c.y + c.z, 1e-6f);
  };
  const auto apart = [&](const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 d = hue(a) - hue(b);
    return std::fabs(d.x) + std::fabs(d.y) + std::fabs(d.z);
  };
  EXPECT_GT(apart(thin, thick), 0.05f);
  EXPECT_GT(apart(thin, grazing), 0.05f);
  for (const glm::vec3& c : {thin, thick, grazing}) {
    EXPECT_GE(std::fmin(c.x, std::fmin(c.y, c.z)), 0.0f);
    EXPECT_LE(std::fmax(c.x, std::fmax(c.y, c.z)), 1.0f);
  }
}

TEST(Terms, NoSpecularRemovesTheDielectricHighlight) {
  // A dielectric at no specular strength shows no highlight under a light
  // at any angle, and at its stock strength the plain one.
  const std::string vectors =
      "float3(0.0, 0.0, 1.0), normalize(float3(0.5, 0.0, 1.0)), "
      "normalize(float3(-0.5, 0.0, 1.0)), ";
  EXPECT_NEAR(scalar("directSpecular(" + vectors +
                     "specularColorFrom(float3(0.5), 0.0, "
                     "dielectricReflectance(float3(1.0), 0.0)), "
                     "reflectance90From(0.0, 0.0), 0.3).g"),
              0.0f, 1e-6f);
  EXPECT_NEAR(scalar("directSpecular(" + vectors +
                     "specularColorFrom(float3(0.5), 0.0, "
                     "dielectricReflectance(float3(1.0), 1.0)), "
                     "reflectance90From(0.0, 1.0), 0.3).g"),
              scalar("directSpecular(" + vectors +
                     "specularColor(float3(0.5), 0.0), "
                     "reflectance90(specularColor(float3(0.5), 0.0)), 0.3).g"),
              1e-6f);
}
