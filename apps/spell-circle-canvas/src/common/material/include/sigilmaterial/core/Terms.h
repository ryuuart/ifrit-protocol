#pragma once

/** @file
 * @ingroup material-core
 *
 * The shading terms a surface is composed of, as source a renderer's
 * compiler is handed. A term is one small piece of arithmetic with a
 * closed form, and a surface is a composition of them. The terms are
 * ONE text: a module for a device renderer, the same without the module
 * line for SkSL. Every term is PURE — sampling is spelled differently
 * in every shading language while arithmetic is not.
 */

#include <sigilmaterial/core/Target.h>

#include <string>
#include <string_view>

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

/** The names the terms answer to, so a composition can be read without
 *  opening the source: `lambert`, `blinn`, `specularColor`, `fresnel`,
 *  `fresnelRough`, `environmentBrdf`, `environmentSpecular`,
 *  `environmentReflection`, `refraction`, `absorption`, `emission`,
 *  `occlusion`, the display transform every lit sum ends at —
 *  `luminance`, `toneMap` — and the panorama's own geometry:
 *  `equirectangularUv`, `equirectangularDirection`, `roughnessLevel`, `atan2P`,
 *  `acosP`. */

}  // namespace sigil::material
