#pragma once

/** @file
 * The direction-field seam: any value answering a heading at a point and
 * a time, and the two ways geometry goes through one — traced from a
 * start, or a polygon displaced along it.
 */

#include <sigildraw/brush/Stroke.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <span>

namespace sigil::draw::brush {

/** A direction field held as a value: a heading in radians for a point
 *  and a time in seconds, clockwise-positive on the y-down canvas as the
 *  pen's `rotate` is. */
using Direction = std::function<float(SkPoint, float)>;

/** Anything callable as a direction field, `Direction` included. */
template <typename Field>
concept DirectionField =
    requires(const Field& field, SkPoint point, float seconds) {
      { field(point, seconds) } -> std::convertible_to<float>;
    };

/** Integrates @p start through a direction field for @p length canvas
 *  units. The field is sampled once per step and its heading becomes the
 *  next travel direction. */
template <DirectionField Field>
[[nodiscard]] Stroke trace(SkPoint start, float length, float spacing,
                           float seconds, const Field& field,
                           float pressure = 1.0f) {
  Stroke result;
  if (!(length > 0.0f)) return result;
  spacing = std::max(0.125f, spacing);
  const int steps = std::max(1, (int)std::ceil(length / spacing));
  result.reserve((size_t)steps + 1);
  result.push_back({start, pressure});
  SkPoint point = start;
  float travelled = 0.0f;
  for (int i = 0; i < steps; ++i) {
    const float step = std::min(spacing, length - travelled);
    const float angle = (float)field(point, seconds);
    point = {point.fX + std::cos(angle) * step,
             point.fY + std::sin(angle) * step};
    result.push_back({point, pressure});
    travelled += step;
  }
  return result;
}

/** Subdivides a polygon at @p spacing and moves every sample @p amount
 *  units along the field's heading there. The result is closed by
 *  repeating its first sample. */
template <DirectionField Field>
[[nodiscard]] Stroke warp(std::span<const SkPoint> polygon, float spacing,
                          float amount, float seconds, const Field& field,
                          float pressure = 1.0f) {
  Stroke result;
  if (polygon.size() < 2) return result;
  spacing = std::max(0.125f, spacing);
  for (size_t edge = 0; edge < polygon.size(); ++edge) {
    const SkPoint from = polygon[edge];
    const SkPoint to = polygon[(edge + 1) % polygon.size()];
    const float dx = to.fX - from.fX;
    const float dy = to.fY - from.fY;
    const float length = std::hypot(dx, dy);
    const int steps = std::max(1, (int)std::ceil(length / spacing));
    for (int step = 0; step < steps; ++step) {
      const float t = (float)step / (float)steps;
      SkPoint point{from.fX + dx * t, from.fY + dy * t};
      const float direction = (float)field(point, seconds);
      point.fX += std::cos(direction) * amount;
      point.fY += std::sin(direction) * amount;
      result.push_back({point, pressure});
    }
  }
  result.push_back(result.front());
  return result;
}

}  // namespace sigil::draw::brush
