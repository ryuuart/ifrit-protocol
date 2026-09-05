#pragma once

/** @file
 * What the interiors need a polygon as: a Skia path, a closed
 * centreline, its centre, and the rings SigilGeometryPath answers
 * questions about it through. Private to the library.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Skia.h>
#include <sigildraw/brush/Stroke.h>

#include <span>
#include <vector>

namespace sigil::draw::brush {

inline SkPath polygonPath(std::span<const SkPoint> polygon) {
  SkPathBuilder path;
  if (polygon.empty()) return path.detach();
  path.moveTo(polygon.front());
  for (size_t i = 1; i < polygon.size(); ++i) path.lineTo(polygon[i]);
  path.close();
  return path.detach();
}

/** One polygon as the ring geometry reads: an area, so it is closed
 *  whether or not the caller repeated its first vertex. */
inline geometry::path::Polyline ring(std::span<const SkPoint> polygon) {
  geometry::path::Polyline out;
  out.closed = true;
  out.points.reserve(polygon.size());
  for (const SkPoint point : polygon)
    out.points.push_back(geometry::path::fromSk(point));
  return out;
}

/** A collection as the even-odd set of rings: the first the boundary and
 *  the rest holes or islands. */
inline std::vector<geometry::path::Polyline> rings(
    std::span<const std::span<const SkPoint>> contours) {
  std::vector<geometry::path::Polyline> out;
  out.reserve(contours.size());
  for (const std::span<const SkPoint> contour : contours)
    out.push_back(ring(contour));
  return out;
}

/** The plain average of the vertices — where a wash starts from, which
 *  is the centre of the marks rather than the centre of the area. */
inline SkPoint polygonCenter(std::span<const SkPoint> polygon) {
  SkPoint center{0, 0};
  if (polygon.empty()) return center;
  for (SkPoint point : polygon) {
    center.fX += point.fX;
    center.fY += point.fY;
  }
  const float count = (float)polygon.size();
  return {center.fX / count, center.fY / count};
}

/** The polygon as a closed centreline at one pressure: its vertices, then
 *  its first vertex again. */
inline Stroke closedOutline(std::span<const SkPoint> polygon,
                            float pressure = 1.0f) {
  Stroke outline;
  if (polygon.empty()) return outline;
  outline.reserve(polygon.size() + 1);
  for (SkPoint point : polygon) outline.push_back({point, pressure});
  outline.push_back(outline.front());
  return outline;
}

}  // namespace sigil::draw::brush
