#pragma once

/** @file
 * The pressure-bearing centreline a tool is painted along: reusable
 * geometry, formed from a segment or smoothed through control points, and
 * painted by any tool as many times as wanted.
 */

#include <include/core/SkPoint.h>

#include <span>
#include <vector>

namespace sigil::draw::brush {

/** One sampled centreline position and the pressure applied there. */
struct Sample {
  SkPoint position{0, 0};
  float pressure = 1.0f;

  bool operator==(const Sample&) const = default;
};

/** A centreline that can be painted repeatedly with different tools. */
using Stroke = std::vector<Sample>;

/** Samples a straight segment closely enough for a pressure envelope to
 *  vary along it. Pressure changes linearly from @p startPressure to
 *  @p endPressure. */
[[nodiscard]] Stroke segment(SkPoint from, SkPoint to, float spacing = 1.0f,
                             float startPressure = 1.0f,
                             float endPressure = 1.0f);

/** Smooths pressure-bearing control points into a sampled centreline.
 *  @p curvature is zero for straight joins and one for the full
 *  Catmull-Rom curve through every control; @p spacing is the target
 *  distance between samples. */
[[nodiscard]] Stroke spline(std::span<const Sample> controls,
                            float spacing = 1.0f, float curvature = 0.5f);

}  // namespace sigil::draw::brush
