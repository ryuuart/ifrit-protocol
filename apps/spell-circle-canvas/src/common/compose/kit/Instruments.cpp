/** @file
 * The rest ghost: the moving copy's own leaf at rest, beside it, in one
 * flat ink.
 */

#include "sigilcompose/kit/Instruments.h"

#include <utility>

namespace sigil::compose::kit {

Element restGhost(Text moving, material::Color colour) {
  Text ghost = moving.atRest();
  // An ink stated on the leaf itself paints it over whatever whole style
  // it was set in, so the ghost reads in @p colour whatever that style
  // paints.
  ghost.ink(colour)
      .textStroke(0.0f, Fill{})
      // Pinned at the origin so the two copies share one origin, and
      // absolute so the MOVING copy is what sizes the box around them.
      .left(0.0f)
      .top(0.0f);
  return box().children({std::move(ghost), std::move(moving)});
}

}  // namespace sigil::compose::kit
