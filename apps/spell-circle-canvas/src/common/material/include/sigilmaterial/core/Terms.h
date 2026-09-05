#pragma once

/** @file
 * The shading terms a surface is composed of, as source a renderer's
 * compiler is handed.
 *
 * A term is one small piece of arithmetic with a closed form — a diffuse
 * factor, a highlight, a Fresnel weight, a split-sum environment
 * reflection, a refraction, an absorption — and a surface is a
 * composition of them, the way a shader graph in an authoring tool is a
 * composition of nodes. No term is a whole shading model and none has to
 * be physically complete to be useful; the author picks the ones the
 * surface needs.
 *
 * The terms are ONE text. `termsSource(Target::Slang)` is a MODULE: a device
 * renderer loads it into its compiler session under the name `Shading`
 * and imports it from its own shaders, so the renderer's shading and
 * every material body compiled beside it call one definition of a term
 * rather than a copy apiece — which is what stops two surfaces meaning
 * two things by one word. `termsSource(Target::SkSL)` is the same text with
 * the module line and the export qualifiers taken off, since SkSL has
 * neither; nothing else in it uses a construct the two languages spell
 * differently, the transcendentals included, which are written out as
 * polynomials for the reason a portable subset exists at all.
 *
 * Every term is PURE: nothing here samples a texture, because sampling
 * is spelled differently in every shading language while arithmetic is
 * not. A caller fetches the radiance and hands it in.
 */

#include <sigilmaterial/core/Target.h>

#include <string>

namespace sigil::material {

/** The terms as source in @p target, ready to be prepended to a body.
 *  Empty for a target that has no spelling of them. */
const std::string& termsSource(Target target);

/** The names the terms answer to, so a composition can be read without
 *  opening the source: `lambert`, `blinn`, `specularColor`, `fresnel`,
 *  `fresnelRough`, `environmentBrdf`, `environmentSpecular`,
 *  `environmentReflection`, `refraction`, `absorption`, `emission`,
 *  `occlusion`, the display transform every lit sum ends at —
 *  `luminance`, `toneMap` — and the panorama's own geometry:
 *  `equirectUv`, `equirectDirection`, `roughnessLevel`, `atan2P`,
 *  `acosP`. */

}  // namespace sigil::material
