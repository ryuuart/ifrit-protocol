/** @file
 * `easeEqual`: the one rule every curve slot in the library compares by.
 */

#include "sigilmotion/ease/Ease.h"

namespace sigil::motion {

bool easeEqual(const Easing& left, const Easing& right) {
  const bool leftSet = (bool)left, rightSet = (bool)right;
  if (leftSet != rightSet) return false;
  if (!leftSet) return true;
  using Pointer = float (*)(float);
  if (const Pointer* leftPointer = left.target<Pointer>(); leftPointer) {
    const Pointer* rightPointer = right.target<Pointer>();
    return rightPointer && *leftPointer == *rightPointer;
  }
  // A shaped curve keeps its shape and its numbers where they can be read
  // back; anything else is a lambda and stays unequal.
  if (const ease::Curve* leftCurve = left.target<ease::Curve>(); leftCurve) {
    const ease::Curve* rightCurve = right.target<ease::Curve>();
    return rightCurve && *leftCurve == *rightCurve;
  }
  return false;
}

}  // namespace sigil::motion
