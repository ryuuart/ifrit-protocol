/** @file
 * EVERY SHADING TERM MEETS ITS CLOSED FORM: one row per term, the
 * expression it is asked, the number it has to answer and the tolerance a
 * polynomial of its degree can hold, each row a case of its own.
 */

#include <cmath>
#include <string>

#include "support/TermEvaluation.h"

using namespace sigil::material;
using namespace sigil::material::test;

namespace {

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
    // THE PATH THROUGH A VOLUME. Seen at forty-five degrees through a
    // face of index one and a half, the ray leaves Snell's law's sine —
    // the sine of forty-five degrees over one and a half — across the
    // face per unit carried; at an index of one it carries straight on,
    // and through no thickness it goes nowhere.
    {"TheRayThroughGlassBendsBySnellsLaw",
     "-refractedTravel(normalize(float3(1.0, 0.0, 1.0)), "
     "float3(0.0, 0.0, 1.0), 1.5, 1.0).x",
     0.70710678f / 1.5f, 2e-3f},
    {"TheRayThroughIndexOneCarriesStraightOn",
     "-refractedTravel(normalize(float3(1.0, 0.0, 1.0)), "
     "float3(0.0, 0.0, 1.0), 1.0, 2.0).x",
     2.0f * 0.70710678f, 2e-3f},
    {"TheRayThroughNoThicknessGoesNowhere",
     "length(refractedTravel(normalize(float3(1.0, 0.0, 1.0)), "
     "float3(0.0, 0.0, 1.0), 1.5, 0.0))",
     0.0f, 2e-3f},
    // A THICKER PATH bends further across at the same index.
    {"ThreeUnitsOfGlassCarryThreeTimesAsFarAcross",
     "refractedTravel(normalize(float3(1.0, 0.0, 1.0)), "
     "float3(0.0, 0.0, 1.0), 1.5, 3.0).x / "
     "refractedTravel(normalize(float3(1.0, 0.0, 1.0)), "
     "float3(0.0, 0.0, 1.0), 1.5, 1.0).x",
     3.0f, 2e-3f},
    // THE VOLUME: nothing is taken over no distance, the stated colour is
    // what is left at the stated distance, its square at twice it, and a
    // distance of zero takes nothing at all.
    {"NoAttenuationOverNoDistance",
     "volumeTransmittance(float3(0.2, 0.5, 0.8), 3.0, 0.0).r", 1.0f, 2e-3f},
    {"TheAttenuationColourAtTheAttenuationDistance",
     "volumeTransmittance(float3(0.2, 0.5, 0.8), 3.0, 3.0).g", 0.5f, 2e-3f},
    {"TheSquareOfTheColourAtTwiceTheDistance",
     "volumeTransmittance(float3(0.2, 0.5, 0.8), 3.0, 6.0).b", 0.64f, 2e-3f},
    {"AZeroDistanceTakesNothing",
     "volumeTransmittance(float3(0.2, 0.5, 0.8), 0.0, 10.0).r", 1.0f, 2e-3f},
    // ENERGY: the transmitted share is what the reflection left, at the
    // weight stated, and the two never sum past one.
    {"AFullWeightTransmitsWhatTheReflectionLeft",
     "transmittedShare(float3(0.04, 0.04, 0.04), 1.0).r", 0.96f, 2e-3f},
    {"ReflectedAndTransmittedSumToOneAtFullWeight",
     "0.3 + transmittedShare(float3(0.3, 0.3, 0.3), 1.0).r", 1.0f, 2e-3f},
    {"AHalfWeightTransmitsHalfOfWhatWasLeft",
     "transmittedShare(float3(0.2, 0.2, 0.2), 0.5).r", 0.4f, 2e-3f},
    {"NoWeightTransmitsNothing",
     "transmittedShare(float3(0.2, 0.2, 0.2), 0.0).r", 0.0f, 2e-3f},
    // DISPERSION: no strength bends every colour alike; a strength spreads
    // red below green and blue above it by a fortieth of the excess.
    {"NoDispersionBendsRedAsGreen", "dispersedIors(1.5, 0.0).r", 1.5f, 1e-4f},
    {"NoDispersionBendsBlueAsGreen", "dispersedIors(1.5, 0.0).b", 1.5f, 1e-4f},
    {"DispersionBendsRedLeast", "dispersedIors(1.5, 1.0).r", 1.4875f, 1e-4f},
    {"DispersionBendsBlueMost", "dispersedIors(1.5, 1.0).b", 1.5125f, 1e-4f},
    // A ROUGH SURFACE OF INDEX ONE blurs nothing behind it.
    {"IndexOneBlursNothing", "transmissionRoughness(0.6, 1.0)", 0.0f, 1e-4f},
    {"GlassBlursByItsRoughness", "transmissionRoughness(0.6, 1.5)", 0.6f,
     1e-4f},
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
    // A SHADOW MAP'S DEPTH, back in scene units: evenly across an
    // orthographic map, crowded toward the near plane in a perspective one.
    {"AnOrthographicDepthRunsEvenlyFromNearToFar",
     "shadowDistance(0.5, 1.0, 11.0, 0.0)", 6.0f, 1e-3f},
    {"APerspectiveDepthHalfWayStandsNearerThanHalfWay",
     "shadowDistance(0.5, 1.0, 3.0, 1.0)", 1.5f, 1e-3f},
    // THE BIAS: the stated texels square on, one more per texel the taps
    // reach for a face at forty-five degrees, and capped where the light
    // grazes.
    {"SquareOnTheBiasIsWhatWasStated", "shadowDepthBias(1.0, 0.1, 1.0, 1.0)",
     0.1f, 1e-3f},
    {"AtFortyFiveDegreesTheBiasGrowsByOneTexelPerTexelTheTapsReach",
     "shadowDepthBias(1.0, 0.1, 0.70710678, 1.0)", 0.3f, 1e-3f},
    {"WhereTheLightGrazesTheBiasIsCapped",
     "shadowDepthBias(1.0, 0.1, 0.0, 1.0)", 1.7f, 1e-3f},
    {"SquareOnAPointIsNotMovedAlongItsNormal",
     "shadowNormalOffset(2.0, 0.1, 1.0)", 0.0f, 1e-4f},
    {"WhereTheLightGrazesAPointMovesTheWholeNormalBias",
     "shadowNormalOffset(2.0, 0.1, 0.0)", 0.2f, 1e-4f},
    // ONE COMPARISON, and four blended: the weights sum to one.
    {"APointWithinTheBiasOfWhatIsStoredIsLit", "shadowTest(5.0, 4.95, 0.1)",
     1.0f, 0.0f},
    {"APointBehindWhatIsStoredIsShadowed", "shadowTest(5.0, 4.8, 0.1)", 0.0f,
     0.0f},
    {"FourLitComparisonsBlendToLit",
     "shadowBilinear(1.0, 1.0, 1.0, 1.0, float2(0.3, 0.7))", 1.0f, 1e-4f},
    {"OneLitCornerWeighsWhatItsNeighbourhoodLeavesIt",
     "shadowBilinear(1.0, 0.0, 0.0, 0.0, float2(0.25, 0.5))", 0.375f, 1e-4f},
    {"TheNineTapsWeighOne", "shadowTapWeight() * 9.0", 1.0f, 1e-4f},
    {"TheFirstTapStandsOneRadiusBefore", "shadowTapOffset(0.0, 2.0)", -2.0f,
     1e-4f},
    {"TheLastTapStandsOneRadiusAfter", "shadowTapOffset(2.0, 2.0)", 2.0f,
     1e-4f},
    {"AMapsMiddleIsItsTilesMiddle",
     "shadowAtlasUv(float2(0.5, 0.5), float4(0.5, 0.0, 0.5, 0.5)).x", 0.75f,
     1e-4f},
    {"InsideTheMapTheShadowStandsWhole",
     "shadowEdgeFade(float2(0.5, 0.5), 0.05)", 1.0f, 1e-4f},
    {"NearTheMapsEdgeTheShadowFades", "shadowEdgeFade(float2(0.01, 0.5), 0.05)",
     0.2f, 1e-4f},
    // CASCADES: the first whose far end lies beyond the point, the last
    // past every end, and the share crossed over near an end.
    {"ANearPointReadsTheFirstCascade",
     "shadowCascade(5.0, float4(10.0, 40.0, 160.0, 0.0))", 0.0f, 0.0f},
    {"APointBetweenTheEndsReadsTheOneItIsIn",
     "shadowCascade(50.0, float4(10.0, 40.0, 160.0, 0.0))", 2.0f, 0.0f},
    {"APointPastEveryEndReadsTheLast",
     "shadowCascade(500.0, float4(10.0, 40.0, 160.0, 0.0))", 2.0f, 0.0f},
    {"OneCascadeIsAlwaysTheFirst",
     "shadowCascade(500.0, float4(10.0, 0.0, 0.0, 0.0))", 0.0f, 0.0f},
    {"BeforeTheBandNothingCrosses", "shadowCascadeBlend(30.0, 40.0, 4.0)", 0.0f,
     1e-4f},
    {"HalfWayThroughTheBandHalfCrosses", "shadowCascadeBlend(38.0, 40.0, 4.0)",
     0.5f, 1e-4f},
    {"AWholeShadowTakesTheWholeLight", "shadowed(0.0, 1.0)", 0.0f, 1e-4f},
    {"AFadedShadowTakesNone", "shadowed(0.0, 0.0)", 1.0f, 1e-4f},
    {"AHalfFadedQuarterLitShadowLeavesFiveEighths", "shadowed(0.25, 0.5)",
     0.625f, 1e-4f},
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
