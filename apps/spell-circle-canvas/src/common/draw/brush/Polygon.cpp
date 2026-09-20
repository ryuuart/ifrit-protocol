/** @file
 * The stored polygon and the gestures through a collection of them.
 */

#include <sigildraw/brush/Hatch.h>
#include <sigildraw/brush/Mass.h>
#include <sigildraw/brush/Polygon.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Skia.h>

#include "PolygonMath.h"

namespace sigil::draw::brush {

std::vector<SkPoint> Polygon::intersect(const Line& line) const {
  const std::vector<glm::vec2> hits = geometry::path::edgeCrossings(
      ring(vertices), geometry::path::fromSk(line.from),
      geometry::path::fromSk(line.to));
  std::vector<SkPoint> result;
  result.reserve(hits.size());
  for (const glm::vec2 hit : hits) result.push_back(geometry::path::toSk(hit));
  return result;
}

Polygon Polygon::translated(float x, float y) const {
  std::vector<SkPoint> moved = vertices;
  for (SkPoint& point : moved) {
    point.fX += x;
    point.fY += y;
  }
  return Polygon(std::move(moved));
}

void hatchArray(Pen& pen, const Tool& tool, std::span<const Polygon> polygons,
                const Hatch& style) {
  if (polygons.empty() || polygons.front().empty()) return;
  std::vector<std::span<const SkPoint>> contours;
  contours.reserve(polygons.size());
  for (const Polygon& polygon : polygons) contours.push_back(polygon.vertices);
  brush::hatch(pen, tool, contours, style);
}

void massArray(Pen& pen, const Tool& tool, std::span<const Polygon> polygons,
               const Mass& style) {
  if (polygons.empty() || polygons.front().empty()) return;
  std::vector<std::span<const SkPoint>> contours;
  contours.reserve(polygons.size());
  for (const Polygon& polygon : polygons) contours.push_back(polygon.vertices);
  brush::mass(pen, tool, contours, style);
}

}  // namespace sigil::draw::brush
