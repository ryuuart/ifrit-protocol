/** @file
 * Formation of pressure-bearing brush paths.
 */

#include <sigildraw/kit/Path.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

namespace {

float distance(SkPoint a, SkPoint b) {
  return std::hypot(b.fX - a.fX, b.fY - a.fY);
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

}  // namespace

Stroke segment(SkPoint from, SkPoint to, float spacing, float startPressure,
               float endPressure) {
  const float length = distance(from, to);
  if (!(length > 0.0f)) return {{from, startPressure}};
  spacing = std::max(0.125f, spacing);
  const int steps = std::max(1, (int)std::ceil(length / spacing));
  Stroke result;
  result.reserve((size_t)steps + 1);
  for (int i = 0; i <= steps; ++i) {
    const float t = (float)i / (float)steps;
    result.push_back({{lerp(from.fX, to.fX, t), lerp(from.fY, to.fY, t)},
                      lerp(startPressure, endPressure, t)});
  }
  return result;
}

Stroke spline(std::span<const Sample> controls, float spacing,
              float curvature) {
  if (controls.empty()) return {};
  if (controls.size() == 1) return {controls.front()};
  spacing = std::max(0.125f, spacing);
  curvature = std::clamp(curvature, 0.0f, 1.0f);

  Stroke result;
  for (size_t segmentIndex = 0; segmentIndex + 1 < controls.size();
       ++segmentIndex) {
    const Sample& p0 = controls[segmentIndex == 0 ? 0 : segmentIndex - 1];
    const Sample& p1 = controls[segmentIndex];
    const Sample& p2 = controls[segmentIndex + 1];
    const Sample& p3 =
        controls[std::min(segmentIndex + 2, controls.size() - 1)];
    const int steps = std::max(
        1, (int)std::ceil(distance(p1.position, p2.position) / spacing));
    const int first = segmentIndex == 0 ? 0 : 1;
    for (int i = first; i <= steps; ++i) {
      const float t = (float)i / (float)steps;
      const float t2 = t * t;
      const float t3 = t2 * t;
      const float catmullX =
          0.5f *
          ((2.0f * p1.position.fX) + (-p0.position.fX + p2.position.fX) * t +
           (2.0f * p0.position.fX - 5.0f * p1.position.fX +
            4.0f * p2.position.fX - p3.position.fX) *
               t2 +
           (-p0.position.fX + 3.0f * p1.position.fX - 3.0f * p2.position.fX +
            p3.position.fX) *
               t3);
      const float catmullY =
          0.5f *
          ((2.0f * p1.position.fY) + (-p0.position.fY + p2.position.fY) * t +
           (2.0f * p0.position.fY - 5.0f * p1.position.fY +
            4.0f * p2.position.fY - p3.position.fY) *
               t2 +
           (-p0.position.fY + 3.0f * p1.position.fY - 3.0f * p2.position.fY +
            p3.position.fY) *
               t3);
      const float lineX = lerp(p1.position.fX, p2.position.fX, t);
      const float lineY = lerp(p1.position.fY, p2.position.fY, t);
      result.push_back(
          {{lerp(lineX, catmullX, curvature), lerp(lineY, catmullY, curvature)},
           lerp(p1.pressure, p2.pressure, t)});
    }
  }
  return result;
}

}  // namespace sigil::draw::brush
