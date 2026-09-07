/** @file
 * The transforms in a build that found no OpenColorIO: nothing is
 * available, and every factory answers the empty LUT material a bad
 * config would, so a consumer's path through them is the same on every
 * machine.
 */

#include "sigilmaterial/ocio/Ocio.h"

namespace sigil::material::ocio {

bool available() { return false; }

Material viewTransform(std::string_view, std::string_view, std::string_view,
                       int) {
  return Material(lutRecipe());
}

Material convert(std::string_view, std::string_view, std::string_view, int) {
  return Material(lutRecipe());
}

Material exponent(float, int) { return Material(lutRecipe()); }

}  // namespace sigil::material::ocio
