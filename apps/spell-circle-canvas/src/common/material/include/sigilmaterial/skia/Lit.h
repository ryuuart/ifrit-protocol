#pragma once

/** @file
 * @ingroup material-skia
 *
 * A LIT SURFACE IN 2D through Skia: a material whose `surface()` states a
 * response, shaded under a `Lighting` — its colour stack lowered once,
 * as it would be painted flat, and a lighting pass over it that reads the
 * normal map for relief. The pass is the only part that moves: a bound
 * light re-resolves the pass and the colour stack beneath stays the
 * shader it was lowered to.
 */

#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Paint.h>

namespace sigil::material::skia {

/** Whether @p material states a surface that takes light: one stated and
 *  not `unlit`. */
bool isLit(const Material& material);

/** The lighting @p material is shaded under where @p inForce is the
 *  scene's: its own surface's `lighting` over the scene's. Empty where
 *  neither states one, which paints the material flat. */
Lighting lightingFor(const Material& material, const Lighting& inForce);

/** @p material's colour stack SHADED UNDER @p lighting, as one paint: live
 *  while an angle, a strength or the environment moves, static
 *  otherwise. With no lighting, or a material that is not lit, the paint
 *  is the colour stack alone, as `paint(material)` lowers it. */
Paint lit(const Material& material, const Lighting& lighting);

}  // namespace sigil::material::skia
