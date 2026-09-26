/** @file
 * Points where a shape says: the patterns over the scatter, the radial
 * arrangement and the arc-length walk.
 */

#include "sigilgeometry/path/Points.h"

#include <include/core/SkPath.h>

#include <algorithm>
#include <cmath>
#include <span>

#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Pose.h"
#include "sigilgeometry/path/Scatter.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

Pattern random(int count, uint64_t seed) {
  return {.kind = Pattern::Kind::Random, .count = count, .seed = seed};
}

Pattern poisson(float radius, uint64_t seed) {
  return {.kind = Pattern::Kind::Poisson, .spacing = radius, .seed = seed};
}

Pattern grid(float spacing, GridOptions options) {
  return {.kind = Pattern::Kind::Grid,
          .spacing = spacing,
          .jitter = options.jitter,
          .seed = options.seed};
}

Pattern radial(int count, RadialOptions options) {
  Pattern pattern{.kind = Pattern::Kind::Radial, .count = count};
  pattern.radial = std::move(options);
  return pattern;
}

Pattern along(float spacing, AlongOptions options) {
  return {.kind = Pattern::Kind::Along,
          .spacing = spacing,
          .from = options.from,
          .to = options.to};
}

namespace {

std::vector<glm::vec2> scattered(const Region& region, const Pattern& pattern) {
  switch (pattern.kind) {
    case Pattern::Kind::Random:
      return sample(region, distribution::uniform(pattern.count, pattern.seed));
    case Pattern::Kind::Poisson:
      return sample(region, distribution::poisson(pattern.spacing, pattern.seed));
    case Pattern::Kind::Grid:
      return sample(region, pattern.jitter > 0
                                ? distribution::jittered(pattern.spacing,
                                                         pattern.seed,
                                                         pattern.jitter)
                                : distribution::grid(pattern.spacing));
    default:
      return {};
  }
}

std::vector<glm::vec2> dealtIn(const Rect& bounds, const Pattern& pattern) {
  PolarFrame frame = pattern.radial.frame;
  frame.centre = bounds.centre();
  frame.radius = std::min(bounds.width(), bounds.height()) * 0.5f;
  return radialPoints(pattern.count, pattern.radial, frame);
}

std::vector<glm::vec2> walked(const Outline& where, const Pattern& pattern) {
  std::vector<glm::vec2> out;
  if (!(pattern.spacing > 0)) return out;
  const std::vector<Contour> contours = Contour::of(toSk(where));
  const float total = totalLength(contours);
  const float end = pattern.to < 0 ? total : std::min(pattern.to, total);
  for (float distance = std::max(0.0f, pattern.from); distance <= end + 1e-4f;
       distance += pattern.spacing)
    out.push_back(poseAlong(contours, distance).position);
  return out;
}

}  // namespace

std::vector<glm::vec2> points(const Outline& where, const Pattern& pattern) {
  if (pattern.kind == Pattern::Kind::Along) return walked(where, pattern);
  if (pattern.kind == Pattern::Kind::Radial)
    return dealtIn(where.bounds(), pattern);
  return scattered(Region::of(toSk(where)), pattern);
}

std::vector<glm::vec2> points(const Rect& where, const Pattern& pattern) {
  if (pattern.kind == Pattern::Kind::Radial) return dealtIn(where, pattern);
  if (pattern.kind == Pattern::Kind::Along)
    return walked(Outline::rectangle(where), pattern);
  return scattered(Region::of(toSk(where)), pattern);
}

float heading(glm::vec2 vector) {
  if (vector.x == 0 && vector.y == 0) return 0;
  return degrees(std::atan2(vector.y, vector.x));
}

}  // namespace sigil::geometry::path
