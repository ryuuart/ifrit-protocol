/** @file
 * The path through points, over the polyline tier's straight, smoothed
 * and fitted constructions.
 */

#include "sigilgeometry/path/Through.h"

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <vector>

#include "sigilgeometry/path/Fit.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

Outline through(std::span<const glm::vec2> points, ThroughOptions options) {
  if (points.size() < 2) return {};
  const std::vector<glm::vec2> run(points.begin(), points.end());
  switch (options.smooth) {
    case Smooth::None:
      return fromSk(toPath(Polyline{.points = run, .closed = options.closed}));
    case Smooth::CatmullRom:
      return fromSk(toPath(Sampled{.points = run, .closed = options.closed},
                           true));
    case Smooth::Midpoint:
      return fromSk(smoothThrough(points, options.closed));
    case Smooth::Fit: {
      const SkPath fitted = fitCurve(points, options.tolerance);
      if (!options.closed) return fromSk(fitted);
      SkPathBuilder closing(fitted);
      closing.close();
      return fromSk(closing.detach());
    }
  }
  return {};
}

}  // namespace sigil::geometry::path
