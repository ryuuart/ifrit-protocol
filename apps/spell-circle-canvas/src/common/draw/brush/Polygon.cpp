/** @file
 * The stored polygon and the gestures through a collection of them.
 */

#include "PolygonMath.h"

#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Skia.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Engine.h>
#include <sigildraw/brush/Hatch.h>
#include <sigildraw/brush/Mass.h>
#include <sigildraw/brush/Polygon.h>
#include <sigildraw/brush/Wash.h>

#include <algorithm>

namespace sigil::draw::brush {

std::vector<SkPoint> Polygon::intersect(const Line& line) const {
  const std::vector<glm::vec2> hits = geometry::path::edgeCrossings(
      ring(vertices), geometry::path::fromSk(line.from),
      geometry::path::fromSk(line.to));
  std::vector<SkPoint> result;
  result.reserve(hits.size());
  for (const glm::vec2 hit : hits)
    result.push_back(geometry::path::toSk(hit));
  return result;
}

void Polygon::draw(Pen& pen, const Tool& tool) const {
  if (empty()) return;
  paint(pen, tool, closedOutline(vertices));
}

void Polygon::fill(Pen& pen, const Wash& style) const { wash(pen, style); }

void Polygon::wash(Pen& pen, const Wash& style) const {
  brush::wash(pen, style, vertices);
}

void Polygon::hatch(Pen& pen, const Tool& tool, const Hatch& style) const {
  brush::hatch(pen, tool, vertices, style);
}

void Polygon::mass(Pen& pen, const Tool& tool, const Mass& style) const {
  brush::mass(pen, tool, vertices, style);
}

void Polygon::draw(Pen& pen, const Engine& engine) const {
  engine.draw(pen, *this);
}

void Polygon::fill(Pen& pen, const Engine& engine) const {
  engine.fill(pen, *this);
}

void Polygon::wash(Pen& pen, const Engine& engine) const {
  engine.wash(pen, *this);
}

void Polygon::hatch(Pen& pen, const Engine& engine) const {
  engine.hatch(pen, *this);
}

void Polygon::mass(Pen& pen, const Engine& engine) const {
  engine.mass(pen, *this);
}

void Polygon::show(Pen& pen, const Engine& engine) const {
  engine.wash(pen, *this);
  engine.fill(pen, *this);
  engine.mass(pen, *this);
  engine.hatch(pen, *this);
  engine.draw(pen, *this);
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
