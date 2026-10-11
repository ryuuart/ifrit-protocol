#pragma once

/** @file
 * @ingroup compose-texture
 *
 * What the maps of a page painted as a surface stand for at their white:
 * stated once by the author, or learned from what the page paints.
 */

namespace sigil::compose {

/** WHAT A PAGE'S MAPS STAND FOR AT THEIR WHITE. The maps are eight bits, so
 *  the emissive map's white is a strength of light, the thickness map's a
 *  depth and the distortion map's full swing a displacement, all in scene
 *  units. A number stated here is that reading from the first paint on:
 *  the page never repaints because its content grew brighter, and content
 *  that moves its emission on the clock — a bound strength, a live map —
 *  keeps every value up to the one stated. Content past it clips there.
 *
 *  A number left at zero is learned instead: the reading rises with the
 *  brightest emission, the deepest volume and the strongest distortion the
 *  page's materials state as they are painted, and every map read against
 *  one that rose is painted again in that same render. */
struct SurfaceRange {
  /** The light the emissive map's white stands for; at least one. */
  float emission = 0;
  /** The depth the thickness map's white stands for. */
  float thickness = 0;
  /** The displacement the distortion map's full swing stands for. */
  float distortion = 0;

  bool operator==(const SurfaceRange&) const = default;
};

}  // namespace sigil::compose
