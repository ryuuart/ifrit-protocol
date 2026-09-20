#pragma once

/** @file
 * @ingroup material-kit
 *
 * ONE INSTANCE OF EVERY RECIPE THE KIT SHIPS, as a list. What is
 * enumerated is materials rather than recipes, because a slot left empty
 * and a slot holding a texture generate different programs, so each is
 * dressed the way its own builder dresses it and carries its recipe's
 * name. It is for a caller that has to reach every program the kit can
 * ask a backend for without knowing what the kit holds.
 */

#include <sigilmaterial/core/Material.h>

#include <vector>

/** The PRESETS: functions that fix a primitive's parameters into
 *  something already worth looking at — named ramps and skies, the
 *  metallic-roughness surface, grained stone and timber, reflective gold
 *  and chrome and glass, the pattern panels, the layer-style colour
 *  tables and the animated text paints. Nothing here decides anything a
 *  caller could have decided; each is one call away from the primitive
 *  it dresses, so a caller who wants other numbers reaches past the kit
 *  into the feature that owns them. */
namespace sigil::material::kit {

/** An instance of every recipe the kit ships, one apiece: the two
 *  metallic-roughness surfaces and the unlit one, the reflective gold,
 *  chrome and glass over a stand-in normal map and environment, the
 *  grained stone, timber, latten and board, the globe, the constant and
 *  sampled masks, and the six text paints. The layer styles and the girih panel
 *  are not here because neither is a recipe — one is colour tables, the
 *  other a baked tile. */
std::vector<Material> everyRecipe();

}  // namespace sigil::material::kit
