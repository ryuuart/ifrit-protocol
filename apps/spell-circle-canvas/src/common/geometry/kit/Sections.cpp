/** @file
 * The two stock sections, written out.
 */

#include "sigilgeometry/kit/Sections.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sigil::geometry::sections {

path::Polyline circle(int sides) {
  path::Polyline out;
  sides = std::max(sides, 3);
  out.points.reserve((size_t)sides + 1);
  // The seam point is emitted twice, at 0 and at a full turn, so the
  // swept ring's u reaches 1 rather than folding back to vertex zero.
  for (int s = 0; s <= sides; ++s) {
    const float a =
        (float)s / (float)sides * 2.0f * std::numbers::pi_v<float>;
    // y-down: -cos puts the first point on the frame's normal.
    out.points.emplace_back(std::sin(a), -std::cos(a));
  }
  return out;
}

path::Polyline line() {
  path::Polyline out;
  out.points = {{-0.5f, 0.0f}, {0.5f, 0.0f}};
  return out;
}

}  // namespace sigil::geometry::sections
