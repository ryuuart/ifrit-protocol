#pragma once

/** @file
 * ONE INSTANCE OF EVERY RECIPE THE KIT SHIPS, as a list.
 *
 * A recipe is only half a thing a backend can compile: the other half is
 * an instance, because a slot left empty and a slot holding a texture
 * generate different programs. So what is enumerated here is materials,
 * one per recipe, each dressed the way its own builder dresses it — with
 * a small stand-in image where the recipe needs one — and each carrying
 * its recipe's name.
 *
 * The list is for a caller that has to reach every program the kit can
 * ask a backend for without knowing what the kit holds: a device
 * renderer warming its pipeline cache before the first frame, and the
 * proof that every body compiles on a device and not only as its own
 * SkSL program on the CPU.
 */

#include <sigilmaterial/core/Material.h>

#include <vector>

namespace sigil::material::kit {

/** An instance of every recipe the kit ships, one apiece: the two
 *  metallic-roughness surfaces and the unlit one, the reflective gold,
 *  chrome and glass over a stand-in normal map and environment, the
 *  grained stone, timber, latten and board, the constant and sampled
 *  masks, and the six text paints. The layer styles and the girih panel
 *  are not here because neither is a recipe — one is colour tables, the
 *  other a baked tile. */
std::vector<Material> everyRecipe();

}  // namespace sigil::material::kit
