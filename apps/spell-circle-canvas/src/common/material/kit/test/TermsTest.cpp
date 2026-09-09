/** @file
 * THE SHADING TERMS, evaluated: each meets its closed form, the
 * environment falls off with roughness, the panorama's middle is
 * straight ahead, the exposure multiplies before the curve — and the
 * arithmetic a host tier transcribes in C++ answers what this text
 * answers, term for term.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/mesh/render/Shading.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/core/Terms.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cmath>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>

using namespace sigil::material;

namespace {

/** ONE TERM, EVALUATED. The term source is prepended to a body that
 *  returns the expression asked about, the material is compiled through
 *  the Skia backend and drawn over one texel of a float surface, so what
 *  comes back is what the term computed rather than what an 8-bit
 *  channel could hold. */
struct NoParams {
  float unused = 0;
};

/** ONE BODY, EVALUATED: compiled through the Skia backend and drawn over
 *  one texel of a float surface. */
SkColor4f shadeBody(const std::string& body) {
  skia::install();
  static int serial = 0;
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParams>("term." + std::to_string(serial++))
          .body(Target::SkSL, body));
  sk_sp<SkShader> shader = skia::shader(Material(recipe, NoParams{}), {});
  EXPECT_TRUE(shader) << body;
  if (!shader) return {0, 0, 0, 0};
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  SkPaint paint;
  paint.setShader(shader);
  paint.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawPaint(paint);
  float px[4] = {0, 0, 0, 0};
  const SkImageInfo one =
      SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  EXPECT_TRUE(surface->readPixels(SkPixmap(one, px, sizeof(px)), 0, 0));
  return {px[0], px[1], px[2], px[3]};
}

SkColor4f term(const std::string& expression) {
  return shadeBody(termsSource(Target::SkSL) +
                   "half4 main(float2 xy) { return half4(" + expression +
                   "); }");
}

/** The red channel of a term that answers one number. */
float scalar(const std::string& expression) {
  return term(expression + ", 0.0, 0.0, 1.0").fR;
}

/** ONE SHADING TERM AND THE NUMBER IT HAS TO ANSWER. The tolerance is
 *  per row because the transcendentals here are polynomials, so each is
 *  held to what a polynomial of its degree can do rather than to the
 *  standard library's answer. */
struct ClosedForm {
  const char* what;
  const char* expression;
  float expected;
  float tolerance;
};

class ShadingTerm : public testing::TestWithParam<ClosedForm> {};

std::string closedFormOf(const testing::TestParamInfo<ClosedForm>& info) {
  return info.param.what;
}

const float kSqrtHalf = std::sqrt(0.5f);
const float kBlinnAt22Point5 = std::pow(std::cos(3.14159265f / 8.0f), 4.0f);

const ClosedForm kClosedForms[] = {
    {"Atan2InTheFirstQuadrant", "atan2P(1.0, 1.0)", 0.78539816f, 2e-3f},
    {"Atan2InTheSecondQuadrant", "atan2P(1.0, -1.0)", 2.35619449f, 2e-3f},
    {"Atan2BelowTheAxis", "atan2P(-0.3, 0.7)", -0.40489179f, 2e-3f},
    {"AcosOfAHalf", "acosP(0.5)", 1.04719755f, 2e-3f},
    {"AcosNearTheEndOfItsRange", "acosP(-0.9)", 2.69056584f, 2e-3f},
    // LAMBERT at 45 degrees is the cosine of 45 degrees; head on it is
    // all of the light and edge on none of it.
    {"LambertAtFortyFiveDegrees",
     "lambert(float3(0.0, 0.0, 1.0), normalize(float3(1.0, 0.0, 1.0)))",
     kSqrtHalf, 2e-3f},
    {"LambertHeadOn", "lambert(float3(0.0, 0.0, 1.0), float3(0.0, 0.0, 1.0))",
     1.0f, 2e-3f},
    {"LambertEdgeOn", "lambert(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0))",
     0.0f, 2e-3f},
    // BLINN with the light and the eye together: the half vector is the
    // normal and the highlight is at its peak whatever the exponent.
    {"BlinnAtItsPeak",
     "blinn(float3(0.0, 0.0, 1.0), float3(0.0, 0.0, 1.0), "
     "float3(0.0, 0.0, 1.0), 48.0)",
     1.0f, 2e-3f},
    {"BlinnWithTheHalfVectorAtTwentyTwoAndAHalf",
     "blinn(float3(0.0, 0.0, 1.0), normalize(float3(1.0, 0.0, 1.0)), "
     "float3(0.0, 0.0, 1.0), 4.0)",
     kBlinnAt22Point5, 3e-3f},
    // FRESNEL: a dielectric's four per cent head on, and white at the rim.
    {"FresnelHeadOn", "fresnel(float3(0.04, 0.04, 0.04), 1.0).r", 0.04f, 2e-3f},
    {"FresnelAtTheRim", "fresnel(float3(0.04, 0.04, 0.04), 0.0).r", 1.0f,
     2e-3f},
    // A metal's reflectance IS its base colour; a dielectric's is four
    // per cent whatever colour it is.
    {"AMetalsReflectanceIsItsColour",
     "specularColor(float3(0.9, 0.6, 0.3), 1.0).g", 0.6f, 2e-3f},
    {"ADielectricsReflectanceIsFourPerCent",
     "specularColor(float3(0.9, 0.6, 0.3), 0.0).g", 0.04f, 2e-3f},
    // THE SPLIT SUM at its one exact point: a mirror seen head on returns
    // the radiance it was handed, scale plus bias summing to one.
    {"TheSplitSumIsExactForAMirrorSeenHeadOn",
     "environmentSpecular(float3(1.0, 1.0, 1.0), float3(1.0, 1.0, 1.0), "
     "0.0, 1.0).r",
     1.0f, 3e-3f},
    // ADDITIVE reflection is the radiance at its weight and nothing else.
    {"AdditiveReflectionIsTheRadianceAtItsWeight",
     "environmentReflection(float3(0.5, 0.5, 0.5), 0.6).r", 0.3f, 2e-3f},
    // BEER-LAMBERT over half a unit of a medium that takes one per unit,
    // and nothing absorbed is everything through.
    {"AttenuationOverHalfAUnit",
     "attenuate(float3(1.0, 1.0, 1.0), float3(1.0, 1.0, 1.0), 0.5).r",
     0.60653066f, 2e-3f},
    {"AttenuationThroughNothingAtAll",
     "attenuate(float3(0.8, 0.8, 0.8), float3(0.0, 0.0, 0.0), 100.0).r", 0.8f,
     2e-3f},
    {"OcclusionBelievedInFull", "occlusion(0.5, 1.0)", 0.5f, 2e-3f},
    {"OcclusionNotBelievedAtAll", "occlusion(0.5, 0.0)", 1.0f, 2e-3f},
    {"EmissionIsTheColourTimesTheMapTimesTheStrength",
     "emission(float3(0.5, 0.5, 0.5), 2.0, float3(0.4, 0.4, 0.4)).r", 0.4f,
     2e-3f},
    // REFRACTION through no change of index is the ray it was given, and
    // past total internal reflection there is no refracted ray at all.
    {"RefractionThroughNoChangeOfIndex",
     "-refraction(float3(0.0, 0.0, -1.0), float3(0.0, 0.0, 1.0), 1.0).z", 1.0f,
     2e-3f},
    {"RefractionPastTotalInternalReflection",
     "length(refraction(normalize(float3(1.0, 0.0, -0.05)), "
     "float3(0.0, 0.0, 1.0), 1.6))",
     0.0f, 2e-3f},
    // THE PANORAMA'S CONVENTION: v = 0 is the zenith, and a direction and
    // a coordinate round trip.
    {"TheZenithIsAtTheTopOfThePanorama", "equirectUv(float3(0.0, 1.0, 0.0)).y",
     0.0f, 2e-3f},
    {"ADirectionAndAPanoramaCoordinateRoundTrip",
     "equirectDirection(equirectUv(normalize(float3(0.3, 0.5, -0.8)))).y",
     0.50507627f, 4e-3f},
    // A roughness reads across the chain it was prefiltered into.
    {"HalfRoughnessIsHalfwayUpTheChain", "roughnessLevel(0.5, 9.0)", 4.0f,
     2e-3f},
    {"FullRoughnessIsTheTopOfTheChain", "roughnessLevel(1.0, 9.0)", 8.0f,
     2e-3f},
    // LUMINANCE: the three weights sum to one, so a grey reads at its own
    // value and a colour reads between its channels.
    {"LuminanceOfWhiteIsOne", "luminance(float3(1.0, 1.0, 1.0))", 1.0f, 2e-3f},
    {"LuminanceOfGreenIsItsWeight", "luminance(float3(0.0, 1.0, 0.0))",
     0.715160f, 2e-3f},
    // THE TONE CURVE. Black stays black, and a grey lands at its own
    // value over one plus itself — white at a half.
    {"TheToneCurveLeavesBlackBlack", "toneMap(float3(0.0, 0.0, 0.0), 1.0).r",
     0.0f, 2e-3f},
    {"TheToneCurvePutsWhiteAtAHalf", "toneMap(float3(1.0, 1.0, 1.0), 1.0).r",
     0.5f, 2e-3f},
    {"TheToneCurvePutsAQuarterAtAFifth",
     "toneMap(float3(0.25, 0.25, 0.25), 1.0).r", 0.2f, 2e-3f},
    // NOTHING CLIPS: a radiance a hundred times over white lands just
    // under one rather than flat on it, which is the whole point of the
    // curve. And a negative radiance answers black rather than a negative
    // colour.
    {"AHundredTimesWhiteLandsJustUnderOne",
     "toneMap(float3(100.0, 100.0, 100.0), 1.0).r", 100.0f / 101.0f, 2e-3f},
    {"ANegativeRadianceAnswersBlack",
     "toneMap(float3(-1.0, -1.0, -1.0), 1.0).r", 0.0f, 2e-3f},
};

}  // namespace

TEST_P(ShadingTerm, MeetsItsClosedForm) {
  EXPECT_NEAR(scalar(GetParam().expression), GetParam().expected,
              GetParam().tolerance);
}

INSTANTIATE_TEST_SUITE_P(EveryTerm, ShadingTerm,
                         testing::ValuesIn(kClosedForms), closedFormOf);

TEST(Terms, ARoughSurfaceTakesLessOfTheEnvironmentThanASmoothOne) {
  EXPECT_LT(scalar("environmentSpecular(float3(1.0, 1.0, 1.0), "
                   "float3(0.04, 0.04, 0.04), 1.0, 1.0).r"),
            scalar("environmentSpecular(float3(1.0, 1.0, 1.0), "
                   "float3(0.04, 0.04, 0.04), 0.0, 1.0).r"));
}

TEST(Terms, LookingAlongMinusZIsTheMiddleOfThePanorama) {
  const SkColor4f forward =
      term("equirectUv(float3(0.0, 0.0, -1.0)), 0.0, 1.0");
  EXPECT_NEAR(forward.fR, 0.5f, 2e-3f);
  EXPECT_NEAR(forward.fG, 0.5f, 2e-3f);
}

TEST(Terms, TheExposureMultipliesTheRadianceBeforeTheCurve) {
  // Twice the exposure over half the radiance is the same colour, which
  // is what makes the dial an exposure rather than a gain on the result.
  EXPECT_NEAR(scalar("toneMap(float3(0.5, 0.5, 0.5), 2.0).r"),
              scalar("toneMap(float3(1.0, 1.0, 1.0), 1.0).r"), 2e-3f);
}

TEST(Terms, TheToneCurveNeverReachesWhite) {
  EXPECT_LT(scalar("toneMap(float3(100.0, 100.0, 100.0), 1.0).r"), 1.0f);
}

TEST(Terms, ABodyWrittenWithAtan2CrossesIntoTheTwoArgumentAtan) {
  // Slang spells the four-quadrant arctangent `atan2(y, x)` and SkSL
  // spells it `atan(y, x)`: the same two arguments in the same order, so
  // the crossing is the rename and nothing else.
  const std::string slang =
      "public float bearing(float2 v) { return atan2(v.y, v.x); }\n";
  const std::string sksl = skSLFromSlang(slang);
  EXPECT_EQ(sksl, "float bearing(float2 v) { return atan(v.y, v.x); }\n");

  // And the crossed text compiles and answers the angle: the second
  // quadrant is three quarters of a half turn.
  const SkColor4f answer = shadeBody(
      sksl +
      "half4 main(float2 xy) { return half4("
      "half(bearing(float2(-1.0, 1.0)) / 3.14159274), 0.0, 0.0, 1.0); }");
  EXPECT_NEAR(answer.fR, 0.75f, 2e-3f);

  // Whole identifiers only, which is what lets one table serve the terms
  // as well: their own polynomial arctangent — written out because a
  // library `atan2` is two pieces of code on two targets and an equirect
  // lookup that disagreed would seam a reflection — keeps its name.
  EXPECT_NE(termsSource(Target::SkSL).find("float atan2P(float y, float x)"),
            std::string::npos);
}

namespace {

/** The four channels a term answers, as the shader computes them. */
glm::vec3 shaded(const std::string& expression) {
  const SkColor4f c = term(expression + ", 1.0");
  return {c.fR, c.fG, c.fB};
}

}  // namespace

TEST(Terms, TheHostTiersTranscriptionAnswersWhatTheShaderTextAnswers) {
  // A host tier has no shading language, so the same arithmetic is
  // written twice: once here as the text every shader compiles, once in
  // C++ where a vertex-shaded mesh evaluates it. Two spellings of one
  // piece of arithmetic drift silently — a constant edited on one side
  // only — and what stops that is asking both the same questions.
  //
  // The tolerance is the shader's: the transcendentals in this text are
  // polynomials, and the C++ side writes the same polynomials, so what
  // is left is the float precision of a half-precision return.
  namespace host = sigil::geometry::mesh::render;
  constexpr float kTol = 2e-3f;

  for (float metal : {0.0f, 0.35f, 1.0f}) {
    const glm::vec3 base{0.9f, 0.6f, 0.2f};
    const glm::vec3 want = host::specularColor(base, metal);
    const glm::vec3 got = shaded("specularColor(float3(0.9, 0.6, 0.2), " +
                                 std::to_string(metal) + ")");
    EXPECT_NEAR(got.x, want.x, kTol) << metal;
    EXPECT_NEAR(got.y, want.y, kTol) << metal;
    EXPECT_NEAR(got.z, want.z, kTol) << metal;
  }

  for (float rough : {0.0f, 0.5f, 1.0f})
    for (float cosTheta : {0.05f, 0.5f, 1.0f}) {
      const glm::vec3 f0{0.04f, 0.04f, 0.04f};
      const glm::vec3 want = host::fresnelRough(f0, cosTheta, rough);
      const glm::vec3 got =
          shaded("fresnelRough(float3(0.04, 0.04, 0.04), " +
                 std::to_string(cosTheta) + ", " + std::to_string(rough) + ")");
      EXPECT_NEAR(got.x, want.x, kTol) << rough << " " << cosTheta;
      EXPECT_NEAR(got.y, want.y, kTol) << rough << " " << cosTheta;
    }

  for (float rough : {0.1f, 0.6f, 1.0f})
    for (float nDotV : {0.1f, 0.75f}) {
      const glm::vec2 want = host::environmentBrdf(rough, nDotV);
      const SkColor4f got = term("environmentBrdf(" + std::to_string(rough) +
                                 ", " + std::to_string(nDotV) + "), 0.0, 1.0");
      EXPECT_NEAR(got.fR, want.x, kTol) << rough << " " << nDotV;
      EXPECT_NEAR(got.fG, want.y, kTol) << rough << " " << nDotV;
    }

  for (float rough : {0.2f, 0.9f}) {
    const glm::vec3 radiance{0.8f, 0.5f, 0.3f};
    const glm::vec3 f0{0.9f, 0.6f, 0.2f};
    const glm::vec3 want = host::environmentSpecular(radiance, f0, rough, 0.6f);
    const glm::vec3 got = shaded(
        "environmentSpecular(float3(0.8, 0.5, 0.3), "
        "float3(0.9, 0.6, 0.2), " +
        std::to_string(rough) + ", 0.6)");
    EXPECT_NEAR(got.x, want.x, kTol) << rough;
    EXPECT_NEAR(got.y, want.y, kTol) << rough;
    EXPECT_NEAR(got.z, want.z, kTol) << rough;
  }

  for (float thickness : {0.0f, 0.5f, 2.0f}) {
    const glm::vec3 want =
        host::attenuate({1.0f, 1.0f, 1.0f}, {0.2f, 0.6f, 1.2f}, thickness);
    const glm::vec3 got = shaded(
        "attenuate(float3(1.0, 1.0, 1.0), "
        "float3(0.2, 0.6, 1.2), " +
        std::to_string(thickness) + ")");
    EXPECT_NEAR(got.x, want.x, kTol) << thickness;
    EXPECT_NEAR(got.y, want.y, kTol) << thickness;
    EXPECT_NEAR(got.z, want.z, kTol) << thickness;
  }

  const glm::vec3 colour{0.3f, 0.7f, 0.9f};
  EXPECT_NEAR(scalar("luminance(float3(0.3, 0.7, 0.9))"),
              host::luminance(colour), kTol);
}
