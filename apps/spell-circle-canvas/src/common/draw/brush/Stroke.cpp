/** @file
 * Formation of pressure-bearing centrelines: the geometry is
 * SigilGeometryPath's, the pressure is the lane it carries.
 */

#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Skia.h>
#include <sigildraw/brush/Stroke.h>

#include <algorithm>

namespace sigil::draw::brush {

namespace {

namespace path = geometry::path;

/** A stroke as the polyline its geometry is done on: the positions, and
 *  the pressure as the lane riding them. */
path::Polyline centreline(std::span<const Sample> samples) {
  path::Polyline line;
  line.points.reserve(samples.size());
  line.lane.reserve(samples.size());
  for (const Sample& sample : samples) {
    line.points.push_back(path::fromSk(sample.position));
    line.lane.push_back(sample.pressure);
  }
  return line;
}

Stroke restore(const path::Polyline& line) {
  Stroke result;
  result.reserve(line.points.size());
  const bool laned = line.lane.size() == line.points.size();
  for (size_t i = 0; i < line.points.size(); ++i)
    result.push_back({path::toSk(line.points[i]), laned ? line.lane[i] : 1.0f});
  return result;
}

}  // namespace

Stroke segment(SkPoint from, SkPoint to, float spacing, float startPressure,
               float endPressure) {
  path::Polyline ends;
  ends.points = {path::fromSk(from), path::fromSk(to)};
  ends.lane = {startPressure, endPressure};
  return restore(path::subdivide(ends, std::max(0.125f, spacing)));
}

Stroke spline(std::span<const Sample> controls, float spacing,
              float curvature) {
  return restore(path::catmullRom(centreline(controls),
                                  std::max(0.125f, spacing), curvature));
}

}  // namespace sigil::draw::brush
