/** @file
 * The transition comparator, under the pin that keeps it honest: two
 * specs are equal only when every field of them is, and the curve is
 * read through `easing()` so that a spec written `{360ms, {}, 220ms}`
 * compares as the default curve it behaves as.
 */

#include "sigilmotion/values/Transition.h"

#include <sigilcore/comparable/Fields.h>

namespace sigil::motion {

static_assert(core::kFieldCount<Transition> == 3,
              "Transition gained or lost a field — rule on it in "
              "transitionEqual() below, then bump this count.");
bool transitionEqual(const Transition& a, const Transition& b) {
  return a.duration == b.duration && a.delay == b.delay &&
         easeEqual(a.easing(), b.easing());  // `ease` is read through easing()
}

}  // namespace sigil::motion
