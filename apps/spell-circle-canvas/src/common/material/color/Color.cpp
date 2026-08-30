/** @file
 * The colour value's packed spelling.
 */

#include "sigilmaterial/color/Color.h"

namespace sigil::material {

Color rgb(uint32_t hex, float a) {
  return {(float)((hex >> 16) & 0xff) / 255.0f,
          (float)((hex >> 8) & 0xff) / 255.0f, (float)(hex & 0xff) / 255.0f, a};
}

}  // namespace sigil::material
