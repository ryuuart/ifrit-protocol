#pragma once

/** @file
 * @ingroup material-texture
 *
 * THE FLOW MAP: a material read through a texture of directions, moving
 * along them with the clock — water round a rock, lava along a channel,
 * smoke curling out of a vent — the way an effects tool hands over a
 * direction field to be played.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Texture.h>

namespace sigil::material {

/** How a flow map moves its source. */
struct FlowmapOptions {
  /** THE DIRECTIONS: red along u and green along v, each encoded from
   *  zero to one around one half. It is data, and is read as data
   *  whatever it says. */
  Texture flow;
  /** How far one cycle carries the source, in units of the region. */
  float strength = 0.1f;
  /** Cycles a second. Two reads half a cycle apart stream the source
   *  without a seam, each fading out as it jumps back. */
  float speed = 0.5f;
};

/** @p source moving along @p options' flow. */
Material flowmap(Material source, FlowmapOptions options);

}  // namespace sigil::material
