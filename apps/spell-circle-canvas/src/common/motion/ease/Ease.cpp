/** @file
 * `easeEqual`: the one rule every curve slot in the library compares by.
 */

#include "sigilmotion/ease/Ease.h"

namespace sigil::motion {

bool easeEqual(const Easing& a, const Easing& b) {
  const bool aSet = (bool)a, bSet = (bool)b;
  if (aSet != bSet) return false;
  if (!aSet) return true;
  using Pointer = float (*)(float);
  if (const Pointer* pointerA = a.target<Pointer>(); pointerA) {
    const Pointer* pointerB = b.target<Pointer>();
    return pointerB && *pointerA == *pointerB;
  }
  // A shaped curve keeps its shape and its numbers where they can be read
  // back; anything else is a lambda and stays unequal.
  if (const ease::Curve* curveA = a.target<ease::Curve>(); curveA) {
    const ease::Curve* curveB = b.target<ease::Curve>();
    return curveB && *curveA == *curveB;
  }
  return false;
}

}  // namespace sigil::motion
