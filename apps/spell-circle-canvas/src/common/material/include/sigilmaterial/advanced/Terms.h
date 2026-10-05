#pragma once

/** @file
 * @ingroup material-advanced
 *
 * The shading terms a surface is composed of, as source a renderer's
 * compiler is handed. A term is one small piece of arithmetic with a
 * closed form, and a surface is a composition of them. The terms are
 * ONE text: a module for a device renderer, the same without the module
 * line for SkSL. Every term is PURE — sampling is spelled differently
 * in every shading language while arithmetic is not.
 */

#include <sigilmaterial/core/Target.h>

#include <glm/vec2.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::material {

/** The terms as source in @p target, ready to be prepended to a body.
 *  Empty for a target that has no spelling of them. */
const std::string& termsSource(Target target);

/** ONE TEXT, TWO LANGUAGES: @p slang read as SkSL. The module line and
 *  the export qualifiers come off, and the three intrinsics the
 *  languages spell differently — `frac`, `lerp`, `atan2` — are written
 *  SkSL's way. Whole identifiers only, so a `fraction` is left alone.
 *  @trap Everything else must be spelled the same in both: no texture
 *  sampling, no library transcendental whose two implementations could
 *  part company, no construct one language has and the other does not. */
std::string skSLFromSlang(std::string_view slang);

/** THE SPLIT SUM'S SCALE AND BIAS for a surface of @p roughness seen at
 *  the cosine @p nDotV: the GGX lobe with height-correlated visibility —
 *  the terms' own `distributionGgx` and `visibilityGgx` — integrated over
 *  the hemisphere by importance sampling, x the share a surface's own
 *  reflectance scales and y the share its rim adds. Their sum is what a
 *  white surface keeps of a uniform surround in one bounce. */
glm::vec2 environmentBrdf(float roughness, float nDotV);

/** The texels a side of the table `environmentBrdfTable` answers. */
inline constexpr int kEnvironmentBrdfSize = 32;

/** `environmentBrdf` over a square table, row by row: row y is the
 *  roughness y / (size - 1), column x the cosine x / (size - 1). The table
 *  is what a renderer binds and reads at `environmentBrdfUv`, and the
 *  reason it is a table: no analytic fit found for this lobe stays within
 *  a few hundredths of its integral everywhere a surface is seen from —
 *  the one the terms used to carry, fitted for an uncorrelated
 *  visibility, missed it by a fifth at a rough surface's rim. */
const std::vector<glm::vec2>& environmentBrdfTable();

/** The names the terms answer to, so a composition can be read without
 *  opening the source: `lambert`, `blinn`, `specularColor`,
 *  `diffuseColor`, `fresnel`, `fresnelSchlick`, `fresnelRough`,
 *  `reflectance90`, `microfacetWidth`, `distributionGgx`,
 *  `visibilityGgx`, `directDiffuse`, `directSpecular`,
 *  `clearcoatTransmittance`, `distanceFalloff`, `coneFalloff`,
 *  `hemisphereIrradiance`, `environmentBrdfUv`, `environmentSpecular`,
 *  `environmentDiffuse`, `environmentReflection`, `refraction`,
 *  `attenuate`, `emission`, `occlusion`; the display transform every lit
 *  sum ends at — `luminance`, `toneMap`, `toneMapNeutral`,
 *  `toneMapLinear`, `toneCurve`, the same four backward —
 *  `toneMapInverse`, `toneMapNeutralInverse`, `toneMapLinearInverse`,
 *  `toneCurveInverse` — `linearFromSrgb`, `srgbFromLinear` and
 *  their one-channel forms; and the panorama's own geometry:
 *  `equirectangularUv`, `roughnessLevel`, `atan2P`, `acosP`. */

}  // namespace sigil::material
