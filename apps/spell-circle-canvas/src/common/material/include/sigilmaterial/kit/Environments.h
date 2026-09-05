#pragma once

/** @file
 * Two named skies, baked with no assets.
 *
 * `EnvironmentMap::baked()` is the seam: a function from an equirect
 * coordinate to linear radiance. These two are the stock values over it,
 * the ones the reflective surfaces are lit and graded against when no
 * photographed panorama is at hand.
 */

#include <sigilmaterial/texture/EnvironmentMap.h>

namespace sigil::material::kit {

/** Neutral photo-studio bake: graded sky, floor bounce, three softboxes
 *  — the sky steel and gold read best under. */
EnvironmentMap studioEnvironment(int width = 512);

/** A chrome horizon: banded sky over dark ground with a hot sun stripe.
 *  The sun sits at u = 0.5, which is the direction a camera with no
 *  rotation faces, so a flat face pointed at the viewer mirrors it. */
EnvironmentMap sunsetEnvironment(int width = 512);

}  // namespace sigil::material::kit
