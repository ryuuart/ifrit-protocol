/** @file
 * The number an instance's named scalar is asked for by.
 */

#include "sigilmaterial/core/Instance.h"

#include <cstdint>

namespace sigil::material {

float instanceAttributeKey(std::string_view name) {
  if (name.empty()) return 0.0f;
  uint32_t hash = 2166136261u;
  for (const char c : name) {
    hash ^= (uint8_t)c;
    hash *= 16777619u;
  }
  // Twenty-three bits and one more: every value is a whole number a float
  // holds exactly, and none of them is zero.
  return (float)((hash & 0x7FFFFFu) + 1u);
}

}  // namespace sigil::material
