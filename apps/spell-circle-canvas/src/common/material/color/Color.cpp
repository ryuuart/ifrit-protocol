/** @file
 * The colour value's packed spelling.
 */

#include "sigilmaterial/color/Color.h"

namespace sigil::material {

Color rgb(uint32_t hex, float a) {
  return {(float)((hex >> 16u) & 0xffu) / 255.0f,
          (float)((hex >> 8u) & 0xffu) / 255.0f, (float)(hex & 0xffu) / 255.0f,
          a};
}

}  // namespace sigil::material
