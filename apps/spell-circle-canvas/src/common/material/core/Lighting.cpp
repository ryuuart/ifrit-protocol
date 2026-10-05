/** @file
 * One emitter: what of it moves.
 */

#include "sigilmaterial/core/Lighting.h"

namespace sigil::material {

bool Light::isRunning() const {
  return color.isRunning() || intensity.isRunning() ||
         (kind != LightKind::Point &&
          (direction.isRunning() || elevation.isRunning()));
}

}  // namespace sigil::material
