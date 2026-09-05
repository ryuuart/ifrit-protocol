/** @file
 * Formation of pressure-bearing centrelines.
 */

#include <sigildraw/Math.h>
#include <sigildraw/brush/Stroke.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

Stroke segment(SkPoint from, SkPoint to, float spacing, float startPressure,
               float endPressure) {
  const float length = dist(from.fX, from.fY, to.fX, to.fY);
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

  // Uniform Catmull-Rom through the controls, blended toward the chord by
  // the curvature, sampled per chord length.
  const auto catmull = [](float p0, float p1, float p2, float p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
  };
  Stroke result;
  for (size_t index = 0; index + 1 < controls.size(); ++index) {
    const Sample& p0 = controls[index == 0 ? 0 : index - 1];
    const Sample& p1 = controls[index];
    const Sample& p2 = controls[index + 1];
    const Sample& p3 = controls[std::min(index + 2, controls.size() - 1)];
    const float chord = dist(p1.position.fX, p1.position.fY, p2.position.fX,
                             p2.position.fY);
    const int steps = std::max(1, (int)std::ceil(chord / spacing));
    const int first = index == 0 ? 0 : 1;
    for (int i = first; i <= steps; ++i) {
      const float t = (float)i / (float)steps;
      const float curveX = catmull(p0.position.fX, p1.position.fX,
                                   p2.position.fX, p3.position.fX, t);
      const float curveY = catmull(p0.position.fY, p1.position.fY,
                                   p2.position.fY, p3.position.fY, t);
      const float lineX = lerp(p1.position.fX, p2.position.fX, t);
      const float lineY = lerp(p1.position.fY, p2.position.fY, t);
      result.push_back(
          {{lerp(lineX, curveX, curvature), lerp(lineY, curveY, curvature)},
           lerp(p1.pressure, p2.pressure, t)});
    }
  }
  return result;
}

}  // namespace sigil::draw::brush
