/** @file
 * The transition comparator, under the pin that keeps it honest — two
 * specs are equal only when every field of them is, and the curve is
 * read through `easing()` so that a spec written `{360ms, {}, 220ms}`
 * compares as the default curve it behaves as — and the one house curve
 * that has to be solved rather than evaluated.
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

namespace ease {

choreograph::EaseFn cubicBezier(float x1, float y1, float x2, float y2) {
  return [x1, y1, x2, y2](float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    // One coordinate of the cubic through (0,0), (a,·), (b,·), (1,1).
    const auto axis = [](float u, float a, float b) {
      const float v = 1.0f - u;
      return 3.0f * v * v * u * a + 3.0f * v * u * u * b + u * u * u;
    };
    // Twenty-four halvings put the parameter within 2^-24 of the one
    // whose x is the time asked for, which is exact to far below a pixel
    // at any size a screen has.
    float lo = 0.0f, hi = 1.0f, u = t;
    for (int i = 0; i < 24; ++i) {
      u = 0.5f * (lo + hi);
      (axis(u, x1, x2) < t ? lo : hi) = u;
    }
    return axis(u, y1, y2);
  };
}

}  // namespace ease

}  // namespace sigil::motion
