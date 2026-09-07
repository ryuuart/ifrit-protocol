#pragma once

/** @file
 * EVERY RECIPE THIS LIBRARY SHIPS, and the one call that warms them.
 *
 * The primitives are enumerated by the features that own them — the
 * per-pixel fields, the signed-distance shapes — and the presets by the
 * kit. A host that wants the stock materials resident before its first
 * frame wants all three and has no reason to know how many catalogues
 * there are, which one reads its shaders from where, or that reading
 * them waits on a disk. So the composition is here: one list, and one
 * warm-up over it.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/core/Target.h>

#include <vector>

namespace sigil::material::stock {

/** One instance of every recipe the library ships: the field primitives,
 *  the signed-distance primitives and the kit's presets, each dressed the
 *  way its own catalogue dresses it. The catalogues read their shader
 *  files, which is a wait rather than a computation, so they are asked
 *  side by side. */
[[nodiscard]] std::vector<Material> everyRecipe();

/** Compiles every stock recipe for @p target into the shared program
 *  cache, so the first frame that reaches for one finds it compiled.
 *
 *  A compiler for @p target must be registered before this is called —
 *  which backend a host draws through is the host's choice, and this
 *  library registers none. */
WarmupResult warmup(Target target);

}  // namespace sigil::material::stock
