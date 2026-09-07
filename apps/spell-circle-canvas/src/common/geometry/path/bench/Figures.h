#pragma once
/** @file
 * The one figure every bench arm here is measured over, so an arm can
 * stand in the file of its own subject without a second spelling of the
 * shape it loads.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <numbers>

namespace sigil::geometry::path::figures {

/** A closed ring of `segments` cubic arcs with a gentle radial ripple, so
 *  every segment is a real curve the flattener has to subdivide. */
inline SkPath rippledRing(int segments, float radius = 200.0f) {
  SkPathBuilder builder;
  const float step = 2.0f * std::numbers::pi_v<float> / (float)segments;
  auto point = [&](float a) {
    const float r = radius * (1.0f + 0.15f * std::sin(6.0f * a));
    return SkPoint{r * std::cos(a), r * std::sin(a)};
  };
  builder.moveTo(point(0));
  for (int i = 0; i < segments; ++i) {
    const float a0 = step * (float)i, a1 = step * (float)(i + 1);
    const SkPoint p0 = point(a0), p3 = point(a1);
    const float tangent = radius * step / 3.0f;
    const SkPoint p1 = {p0.fX - tangent * std::sin(a0),
                        p0.fY + tangent * std::cos(a0)};
    const SkPoint p2 = {p3.fX + tangent * std::sin(a1),
                        p3.fY - tangent * std::cos(a1)};
    builder.cubicTo(p1, p2, p3);
  }
  builder.close();
  return builder.detach();
}

}  // namespace sigil::geometry::path::figures
