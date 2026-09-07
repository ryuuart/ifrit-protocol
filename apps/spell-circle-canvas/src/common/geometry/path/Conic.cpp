/** @file
 * The conic evaluated and sampled.
 */

#include "sigilgeometry/path/Conic.h"

#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace sigil::geometry::path {

namespace {

constexpr float kDegrees = 3.14159265358979323846f / 180.0f;

/** 1 + e cos v, kept off zero so an open conic's radius stays a number.
 *  The floor is what turns the asymptote from an infinity into a point
 *  far enough out that `ConicSpan::reach` drops it. */
float denominator(const Conic& conic, float anomalyDeg) {
  return std::max(1e-3f,
                  1.0f + conic.eccentricity * std::cos(anomalyDeg * kDegrees));
}

}  // namespace

float Conic::radiusAt(float anomalyDeg) const {
  return semiLatus / denominator(*this, anomalyDeg);
}

glm::vec2 Conic::at(float anomalyDeg) const {
  const float bearing = (periapsisDeg + anomalyDeg) * kDegrees;
  const float r = radiusAt(anomalyDeg);
  return {focus.x + r * std::cos(bearing), focus.y + r * std::sin(bearing)};
}

glm::vec2 Conic::alongAt(float anomalyDeg) const {
  const float v = anomalyDeg * kDegrees;
  const float bearing = (periapsisDeg + anomalyDeg) * kDegrees;
  const float r = radiusAt(anomalyDeg);
  // The radius changes as the bearing sweeps, so the tangent is the sum
  // of the two motions: out along the radius, and round with it.
  const float dr =
      r * eccentricity * std::sin(v) / denominator(*this, anomalyDeg);
  const glm::vec2 along{dr * std::cos(bearing) - r * std::sin(bearing),
                        dr * std::sin(bearing) + r * std::cos(bearing)};
  const float len = glm::length(along);
  return len < 1e-9f ? glm::vec2{1, 0} : along / len;
}

glm::vec2 Conic::outwardAt(float anomalyDeg) const {
  const float bearing = (periapsisDeg + anomalyDeg) * kDegrees;
  return {std::cos(bearing), std::sin(bearing)};
}

float Conic::asymptoteDeg() const {
  if (closes()) return 180.0f;
  return std::acos(-1.0f / eccentricity) / kDegrees;
}

SkPath conicPath(const Conic& conic, const ConicSpan& span) {
  SkPathBuilder built;
  const int steps = std::max(span.steps, 1);
  bool running = false;
  for (int i = 0; i <= steps; ++i) {
    const float anomaly =
        span.fromDeg + (span.toDeg - span.fromDeg) * (float)i / (float)steps;
    if (span.reach > 0 && conic.radiusAt(anomaly) > span.reach) {
      running = false;  // the contour breaks rather than reaching out
      continue;
    }
    const glm::vec2 p = conic.at(anomaly);
    const SkPoint at{p.x, p.y};
    if (running) {
      built.lineTo(at);
    } else {
      built.moveTo(at);
      running = true;
    }
  }
  // The whole way round a closed conic meets itself, and a closed contour
  // is what a fill, a seamless dash and an unbroken arc-length walk all
  // want; anything short of it stays open.
  if (conic.closes() && std::abs(span.toDeg - span.fromDeg) >= 359.5f)
    built.close();
  return built.detach();
}

}  // namespace sigil::geometry::path
