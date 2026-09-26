#pragma once
/** @file
 * @ingroup geometry-path
 *
 * THE PATH THROUGH POINTS: one constructor for a line through a run of
 * points, straight, smoothed or fitted, open or closed.
 */
#include <cstdint>
#include <glm/vec2.hpp>
#include <span>

#include "sigilgeometry/path/Outline.h"

namespace sigil::geometry::path {

/** How the points are joined. */
enum class Smooth : uint8_t {
  /** Straight segments from point to point. */
  None,
  /** A Catmull-Rom cubic that PASSES THROUGH every point, and may
   *  overshoot between two that turn sharply. */
  CatmullRom,
  /** A quadratic from the midpoint of every edge to the next, tangent to
   *  each edge there: the points steer the curve rather than lie on it,
   *  and it never leaves the hull they span. */
  Midpoint,
  /** As few cubics as follow the points within `tolerance` — for a dense
   *  run such as a traced stroke. */
  Fit,
};

/** The dials of `through()`. */
struct ThroughOptions {
  Smooth smooth = Smooth::None;
  /** How far a fitted curve may stray from the points, in px. */
  float tolerance = 1.0f;
  /** Join the last point back to the first. */
  bool closed = false;
  bool operator==(const ThroughOptions&) const = default;
};

/** The outline through @p points. Fewer than two points are the empty
 *  outline. */
Outline through(std::span<const glm::vec2> points, ThroughOptions options = {});

/** The smooth curve that passes through every one of @p points — `through`
 *  with a Catmull-Rom. */
inline Outline curveThrough(std::span<const glm::vec2> points,
                            bool closed = false) {
  return through(points, {.smooth = Smooth::CatmullRom, .closed = closed});
}

}  // namespace sigil::geometry::path
