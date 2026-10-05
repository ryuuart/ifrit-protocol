/** @file
 * THE SHADING TERMS, evaluated: each meets its closed form, the
 * environment falls off with roughness, the panorama's middle is
 * straight ahead, the exposure multiplies before the curve, the specular
 * lobe neither makes light nor — once the light it scatters more than
 * once is put back — loses it, and the split-sum fit stands within a
 * stated distance of the lobe's own integral.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/Terms.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cmath>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace sigil::material;

namespace {

/** ONE TERM, EVALUATED. The term source is prepended to a body that
 *  returns the expression asked about, the material is compiled through
 *  the Skia backend and drawn over one texel of a float surface, so what
 *  comes back is what the term computed rather than what an 8-bit
 *  channel could hold. */
struct NoParameters {
  float unused = 0;
};

/** ONE BODY, EVALUATED: compiled through the Skia backend and drawn over
 *  one texel of a float surface. */
SkColor4f shadeBody(const std::string& body) {
  static int serial = 0;
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParameters>("term." + std::to_string(serial++))
          .body(Target::SkSL, body));
  sk_sp<SkShader> shader = skia::shader(Material(recipe, NoParameters{}), {});
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
    // THE SPLIT SUM at its one exact point: a mirror seen head on, whose
    // table reads a scale of one and no bias, returns the radiance it was
    // handed.
    {"TheSplitSumIsExactForAMirrorSeenHeadOn",
     "environmentSpecular(float3(1.0, 1.0, 1.0), float3(1.0, 1.0, 1.0), "
     "0.0, 1.0, float2(1.0, 0.0)).r",
     1.0f, 3e-3f},
    // The table is read at its texel centres, its edges included.
    {"TheTableIsReadAtItsFirstCentre", "environmentBrdfUv(0.0, 0.0, 32.0).x",
     0.5f / 32.0f, 1e-4f},
    {"TheTableIsReadAtItsLastCentre", "environmentBrdfUv(1.0, 1.0, 32.0).y",
     31.5f / 32.0f, 2e-4f},
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
    // THE PANORAMA'S CONVENTION: v = 0 is the zenith.
    {"TheZenithIsAtTheTopOfThePanorama",
     "equirectangularUv(float3(0.0, 1.0, 0.0)).y", 0.0f, 2e-3f},
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
    // THE GGX DISTRIBUTION at its peak is one over pi times the width
    // squared, and at full roughness it is flat at one over pi.
    {"TheDistributionPeaksAtOneOverPiAlphaSquared", "distributionGgx(1.0, 0.5)",
     1.0f / (3.14159265f * 0.0625f), 5e-3f},
    {"AtFullRoughnessTheDistributionIsFlat", "distributionGgx(0.3, 1.0)",
     1.0f / 3.14159265f, 2e-3f},
    // THE CORRELATED SMITH VISIBILITY: at full roughness one half over the
    // two cosines' sum, and for a mirror one over four times their product.
    {"VisibilityAtFullRoughness", "visibilityGgx(1.0, 1.0, 1.0)", 0.25f, 2e-3f},
    {"VisibilityOfAMirrorIsOneOverFourCosines", "visibilityGgx(0.5, 0.5, 0.0)",
     1.0f, 3e-3f},
    // FRESNEL between two reflectances: the first head on, the second at
    // the grazing angle, a thirty-second of the way at sixty degrees.
    {"SchlickHeadOnIsTheHeadOnReflectance",
     "fresnelSchlick(float3(0.04), float3(1.0), 1.0).r", 0.04f, 2e-3f},
    {"SchlickAtTheGrazingAngleIsTheGrazingReflectance",
     "fresnelSchlick(float3(0.04), float3(0.5), 0.0).r", 0.5f, 2e-3f},
    {"SchlickAtSixtyDegrees", "fresnelSchlick(float3(0.0), float3(1.0), 0.5).r",
     1.0f / 32.0f, 2e-3f},
    {"EveryRealMaterialReachesWhiteAtTheRim", "reflectance90(float3(0.04)).r",
     1.0f, 2e-3f},
    {"AReflectanceUnderTwoPerCentFallsShortOfWhite",
     "reflectance90(float3(0.01)).r", 0.5f, 2e-3f},
    {"ADielectricsDiffuseIsWhatHeadOnFresnelLeaves",
     "diffuseColor(float3(1.0), 0.0).r", 0.96f, 2e-3f},
    {"AMetalHasNoDiffuse", "diffuseColor(float3(1.0), 1.0).r", 0.0f, 2e-3f},
    // ONE LIGHT HEAD ON, a white matte dielectric: what the four per cent
    // Fresnel leaves of the diffuse colour, over pi.
    {"AWhiteMatteSurfaceHeadOnShowsItsDiffuseOverPi",
     "directDiffuse(float3(0.0, 0.0, 1.0), float3(0.0, 0.0, 1.0), "
     "float3(0.0, 0.0, 1.0), float3(0.96), float3(0.04), float3(1.0)).r",
     0.96f * 0.96f / 3.14159265f, 2e-3f},
    {"NoLightReachesFromBehind",
     "directSpecular(float3(0.0, 0.0, 1.0), float3(0.0, 0.0, -1.0), "
     "float3(0.0, 0.0, 1.0), float3(1.0), float3(1.0), 0.5).r",
     0.0f, 2e-3f},
    // THE INVERSE SQUARE at two distances, the window at its range.
    {"AtTwiceTheDistanceAQuarter", "distanceFalloff(2.0, 0.0)", 0.25f, 2e-3f},
    {"AtTenTimesTheDistanceAHundredth", "distanceFalloff(10.0, 0.0) * 100.0",
     1.0f, 2e-3f},
    {"HalfWayToTheRangeTheWindowIsFifteenSixteenthsSquared",
     "distanceFalloff(50.0, 100.0) * 2500.0", (15.0f / 16) * (15.0f / 16),
     3e-3f},
    {"AtTheRangeNothing", "distanceFalloff(100.0, 100.0)", 0.0f, 1e-6f},
    {"PastTheRangeNothing", "distanceFalloff(150.0, 100.0)", 0.0f, 1e-6f},
    // THE CONE'S EDGES: everything at the inner cosine, nothing at the
    // outer, the square of where it stands between.
    {"InsideTheInnerConeEverything", "coneFalloff(0.99, 0.8, 0.95)", 1.0f,
     2e-3f},
    {"AtTheOuterConeNothing", "coneFalloff(0.8, 0.8, 0.95)", 0.0f, 2e-3f},
    {"HalfWayBetweenTheConesAQuarter", "coneFalloff(0.875, 0.8, 0.95)", 0.25f,
     2e-3f},
    // THE HEMISPHERE: all sky facing up, all ground facing down, half of
    // each at the horizon.
    {"FacingUpTheHemisphereIsTheSky",
     "hemisphereIrradiance(float3(0.0, 1.0, 0.0), float3(0.0, 1.0, 0.0), "
     "float3(0.8), float3(0.2)).r",
     0.8f, 2e-3f},
    {"FacingDownTheHemisphereIsTheGround",
     "hemisphereIrradiance(float3(0.0, -1.0, 0.0), float3(0.0, 1.0, 0.0), "
     "float3(0.8), float3(0.2)).r",
     0.2f, 2e-3f},
    {"AtTheHorizonTheHemisphereIsHalfOfEach",
     "hemisphereIrradiance(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), "
     "float3(0.8), float3(0.2)).r",
     0.5f, 2e-3f},
    // A COAT passes all but its head-on four per cent, and no coat passes
    // everything.
    {"AFullCoatHeadOnPassesNinetySixPerCent",
     "clearcoatTransmittance(1.0, 1.0)", 0.96f, 2e-3f},
    {"NoCoatPassesEverything", "clearcoatTransmittance(0.0, 0.3)", 1.0f, 2e-3f},
    // THE NEUTRAL CURVE: black stays black, a mid value loses only its
    // offset, and a hundred times white lands just under one.
    {"TheNeutralCurveLeavesBlackBlack", "toneMapNeutral(float3(0.0), 1.0).r",
     0.0f, 2e-3f},
    {"TheNeutralCurveTakesOnlyItsOffsetOffAMidValue",
     "toneMapNeutral(float3(0.5), 1.0).r", 0.46f, 2e-3f},
    {"TheNeutralCurveHoldsAHundredTimesWhiteUnderOne",
     "toneMapNeutral(float3(100.0), 1.0).r", 0.9994f, 2e-3f},
    {"NoCurveIsTheExposedRadiance", "toneMapLinear(float3(0.25), 2.0).r", 0.5f,
     2e-3f},
    {"NoCurveAnswersANegativeRadianceWithBlack",
     "toneMapLinear(float3(-1.0), 1.0).r", 0.0f, 2e-3f},
    {"CurveZeroIsReinhard", "toneCurve(float3(1.0), 1.0, 0.0).r", 0.5f, 2e-3f},
    {"CurveOneIsNeutral", "toneCurve(float3(0.5), 1.0, 1.0).r", 0.46f, 2e-3f},
    {"CurveTwoIsNone", "toneCurve(float3(0.5), 1.0, 2.0).r", 0.5f, 2e-3f},
    // THE DISPLAY ENCODING: a mid code is a fifth of the light, the
    // straight segment near black, and the two directions undo each other.
    {"AMidCodeIsAFifthOfTheLight", "linearFromSrgbChannel(0.5)", 0.2140411f,
     2e-3f},
    {"NearBlackTheEncodingIsStraight", "linearFromSrgbChannel(0.02) * 100.0",
     2.0f / 12.92f, 2e-3f},
    {"WhiteDecodesToWhite", "linearFromSrgbChannel(1.0)", 1.0f, 2e-3f},
    {"AFifthOfTheLightEncodesToAMidCode", "srgbFromLinearChannel(0.2140411)",
     0.5f, 2e-3f},
    {"EncodingAndDecodingUndoEachOther",
     "srgbFromLinear(linearFromSrgb(float3(0.3, 0.6, 0.9))).g", 0.6f, 2e-3f},
    {"TheEncodingHoldsNothingAboveWhite", "srgbFromLinearChannel(4.0)", 1.0f,
     2e-3f},
    // THE SHEEN'S DISTRIBUTION is brightest with the half vector along the
    // surface: (2 + 1/a) / 2pi there, a = 0.25 at a roughness of one half,
    // and next to nothing along the normal.
    {"TheSheenDistributionAlongTheSurface", "distributionSheen(0.0, 0.5)",
     6.0f / 6.2831853f, 2e-3f},
    {"TheSheenDistributionAlongTheNormalIsNearlyNothing",
     "distributionSheen(1.0, 0.5) * 100.0", 0.00583f, 2e-3f},
    // THE SHEEN NEVER SHOWS MORE THAN ITS FIBRES SEND: its visibility is
    // held to one even at a grazing pair of cosines.
    {"TheSheenVisibilityIsHeldToOne", "visibilitySheen(0.001, 0.001, 0.3)",
     1.0f, 2e-3f},
    // WHAT A SHEEN LEAVES BENEATH IT: a white sheen whose albedo is a
    // quarter at the eye and a half at the light takes the larger half.
    {"TheSheenTakesTheLargerOfItsTwoLosses",
     "sheenScaling(float3(1.0), 0.25, 0.5)", 0.5f, 2e-3f},
    {"NoSheenTakesNothing", "sheenScaling(float3(0.0), 0.9, 0.9)", 1.0f, 2e-3f},
    {"AColouredSheenTakesByItsStrongestChannel",
     "sheenScaling(float3(0.2, 0.8, 0.4), 0.5, 0.25)", 0.6f, 2e-3f},
    {"TheSheenOfLightFromEverywhere",
     "environmentSheen(float3(2.0), float3(0.5), 0.25).r", 0.25f, 2e-3f},
    // THE ANISOTROPIC DISTRIBUTION with its two widths equal is the
    // isotropic one: at a half vector 30 degrees off the normal, a = 0.25.
    {"AnisotropicGgxWithEqualWidthsIsGgx",
     "distributionGgxAnisotropic(0.8660254, 0.5, 0.0, 0.25, 0.25) - "
     "distributionGgx(0.8660254, 0.5)",
     0.0f, 2e-3f},
    {"AnisotropicSmithWithEqualWidthsIsSmith",
     "visibilityGgxAnisotropic(0.6, 0.8, 0.8, 0.0, 0.0, 0.6, 0.25, 0.25) - "
     "visibilityGgx(0.6, 0.8, 0.5)",
     0.0f, 2e-3f},
    {"NoBrushingWidensNothing", "anisotropicWidth(0.5, 0.0)", 0.25f, 2e-3f},
    {"AFullBrushingWidensToOne", "anisotropicWidth(0.5, 1.0)", 1.0f, 2e-3f},
    {"AnUnbrushedSurfaceMirrorsAboutItsOwnNormal",
     "anisotropicBentNormal(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), "
     "normalize(float3(0.3, 0.2, 1.0)), 0.4, 0.0).z",
     1.0f, 2e-3f},
    // THE BRUSHING'S TANGENT, turned a quarter turn from u, runs along the
    // surface's other axis.
    {"AQuarterTurnBrushesAlongTheOtherAxis",
     "anisotropyTangent(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), "
     "float3(0.0, 1.0, 0.0), float2(1.0, 0.0), 90.0, "
     "float3(1.0, 0.0, 0.0)).y",
     1.0f, 2e-3f},
    {"ACollapsedFrameBrushesAlongTheFallback",
     "anisotropyTangent(float3(0.0, 0.0, 1.0), float3(0.0), float3(0.0), "
     "float2(1.0, 0.0), 0.0, float3(0.0, 1.0, 0.0)).y",
     1.0f, 2e-3f},
    // SCHLICK BACKWARD: the reflectance that rises to the one asked about.
    {"FresnelRunBackwardFindsItsStart",
     "fresnelToF0(0.5, fresnelSchlick(float3(0.2), float3(1.0), 0.5), "
     "float3(1.0)).r",
     0.2f, 2e-3f},
    {"GlassAgainstAirReflectsFourPerCent", "reflectanceFromIor(1.5, 1.0)",
     0.04f, 2e-3f},
    // A FILM OF NO WEIGHT OR NO THICKNESS IS NO FILM.
    {"AFilmOfNoWeightLeavesTheReflectance",
     "iridescentF0(float3(0.04), 0.7, 0.0, 1.3, 400.0).r", 0.04f, 1e-5f},
    {"AFilmOfNoThicknessLeavesTheReflectance",
     "iridescentF0(float3(0.04), 0.7, 1.0, 1.3, 0.0).r", 0.04f, 1e-5f},
    // THE DIELECTRIC'S SPECULAR at its stock strength and colour is the
    // four per cent every dielectric here reflects, rising to white, with
    // the diffuse side the plain terms give it; at no strength it reflects
    // nothing at any angle.
    {"StockSpecularIsFourPerCent",
     "specularColorFrom(float3(0.5), 0.0, "
     "dielectricReflectance(float3(1.0), 1.0)).g",
     0.04f, 1e-6f},
    {"StockSpecularRisesToWhite", "reflectance90From(0.0, 1.0).g", 1.0f, 1e-6f},
    {"StockSpecularLeavesTheDiffuseItHad",
     "diffuseColorFrom(float3(0.5), 0.3, "
     "dielectricReflectance(float3(1.0), 1.0)).r - "
     "diffuseColor(float3(0.5), 0.3).r",
     0.0f, 1e-6f},
    {"NoSpecularReflectsNothingHeadOn",
     "dielectricReflectance(float3(1.0), 0.0).r", 0.0f, 1e-6f},
    {"NoSpecularReflectsNothingAtTheRim", "reflectance90From(0.0, 0.0).r", 0.0f,
     1e-6f},
    {"AMetalIgnoresTheDielectricsSpecular",
     "specularColorFrom(float3(0.9, 0.6, 0.3), 1.0, "
     "dielectricReflectance(float3(1.0), 0.0)).g",
     0.6f, 1e-6f},
    {"ATintedSpecularIsTinted",
     "dielectricReflectance(float3(1.0, 0.5, 0.25), 1.0).b", 0.01f, 1e-6f},
};

}  // namespace

TEST_P(ShadingTerm, MeetsItsClosedForm) {
  EXPECT_NEAR(scalar(GetParam().expression), GetParam().expected,
              GetParam().tolerance);
}

INSTANTIATE_TEST_SUITE_P(EveryTerm, ShadingTerm,
                         testing::ValuesIn(kClosedForms), closedFormOf);

namespace {

/** The split sum's table at @p roughness and @p nDotV, spelled as the
 *  argument a term takes it as. */
std::string brdfAt(float roughness, float nDotV) {
  const glm::vec2 ab = environmentBrdf(roughness, nDotV);
  return "float2(" + std::to_string(ab.x) + ", " + std::to_string(ab.y) + ")";
}

}  // namespace

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
  // library `atan2` is two pieces of code on two targets and an equirectangular
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

namespace {

/** EVERY PIXEL OF ONE BODY over an @p n by @p n float surface, row by
 *  row: what a term answers at each of n² arguments the body reads off
 *  its pixel's coordinates. */
std::vector<SkColor4f> evaluated(const std::string& body, int n) {
  static int serial = 0;
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParameters>("terms.grid." + std::to_string(serial++))
          .body(Target::SkSL, termsSource(Target::SkSL) + body));
  sk_sp<SkShader> shader = skia::shader(Material(recipe, NoParameters{}), {});
  EXPECT_TRUE(shader) << body;
  std::vector<SkColor4f> out((size_t)n * n, SkColor4f{0, 0, 0, 0});
  if (!shader) return out;
  const SkImageInfo info =
      SkImageInfo::Make(n, n, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  SkPaint paint;
  paint.setShader(shader);
  paint.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawPaint(paint);
  EXPECT_TRUE(surface->readPixels(
      SkPixmap(info, out.data(), (size_t)n * sizeof(SkColor4f)), 0, 0));
  return out;
}

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
