/** @file
 * The rest ghost: the moving copy's own leaf at rest, beside it, in one
 * flat ink.
 */

#include "sigilcompose/kit/Instruments.h"

#include <sigilmaterial/paint/Paint.h>
#include <sigilmaterial/skia/Paint.h>

#include <utility>

namespace sigil::compose::kit {

Element restGhost(Text moving, material::Color colour) {
  Text ghost = moving.atRest();
  // The ink is written as a PAINT and not as a colour: an ink colour is
  // the inherited lane, which a leaf's own style overrides, and the ghost
  // has to read in @p colour whatever that style paints.
  ghost.ink(material::skia::base(material::Paint::solid(colour)))
      .textStroke(0.0f, Fill{})
      // Pinned at the origin so the two copies share one origin, and
      // absolute so the MOVING copy is what sizes the box around them.
      .left(0.0f)
      .top(0.0f);
  return box().children({std::move(ghost), std::move(moving)});
}

}  // namespace sigil::compose::kit
