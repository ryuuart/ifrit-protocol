#pragma once

/** @file
 * Pressure-bearing paths for the brush kit.
 *
 * A Stroke is reusable geometry rather than drawing state. It may be formed
 * from a segment, smoothed through control points or integrated through any
 * callable direction field, then painted by any Brush.
 */

#include <include/core/SkPoint.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <span>
#include <vector>

namespace sigil::draw::brush {

/** One sampled centreline position and the pressure applied there. */
struct Sample {
  SkPoint position{0, 0};
  float pressure = 1.0f;

  bool operator==(const Sample&) const = default;
};

/** A centreline that can be painted repeatedly with different brushes. */
using Stroke = std::vector<Sample>;

/** Samples a straight segment closely enough for a pressure envelope to vary
 *  along it. Pressure changes linearly from @p startPressure to
 *  @p endPressure. */
Stroke segment(SkPoint from, SkPoint to, float spacing = 1.0f,
               float startPressure = 1.0f, float endPressure = 1.0f);

/** Smooths pressure-bearing control points into a sampled centreline.
 *  @p curvature is zero for straight joins and one for the full cubic curve;
 *  @p spacing is the target distance between samples. */
Stroke spline(std::span<const Sample> controls, float spacing = 1.0f,
              float curvature = 0.5f);

/** A value that answers a direction in radians at a point and a time. */
template <typename Field>
concept DirectionField =
    requires(const Field& field, SkPoint point, float seconds) {
      { field(point, seconds) } -> std::convertible_to<float>;
    };

/** Integrates @p start through a direction field for @p length canvas units.
 *  The field is sampled once per step and its radians become the next travel
 *  direction. */
template <DirectionField Field>
Stroke trace(SkPoint start, float length, float spacing, float seconds,
             const Field& field, float pressure = 1.0f) {
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

}  // namespace sigil::draw::brush
