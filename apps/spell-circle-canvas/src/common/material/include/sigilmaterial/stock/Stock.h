#pragma once

/** @file
 * @ingroup material-stock
 *
 * EVERY RECIPE THIS LIBRARY SHIPS. The primitives are enumerated by the
 * features that own them and the presets by the kit; a host that wants
 * the stock materials resident before its first frame wants all three and
 * has no reason to know how many catalogues there are. So the composition
 * is here, as one list a warm-up takes whole.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/core/Target.h>

#include <vector>

/** EVERY RECIPE THE LIBRARY SHIPS, gathered from the features that own
 *  them; `material::warmup(stock::everyRecipe(), target)` compiles them all
 *  for a target once that target's compiler is registered. It is for
 *  a caller that must reach every program without knowing what the
 *  library holds: a renderer warming its pipeline cache before the first
 *  frame, and the proof that every body compiles on a device and not
 *  only as SkSL on the CPU. */
namespace sigil::material::stock {

/** One instance of every recipe the library ships: the field primitives,
 *  the signed-distance primitives and the kit's presets, each dressed the
 *  way its own catalogue dresses it. The catalogues read their shader
 *  files, which is a wait rather than a computation, so they are asked
 *  side by side. */
[[nodiscard]] std::vector<Material> everyRecipe();

}  // namespace sigil::material::stock
