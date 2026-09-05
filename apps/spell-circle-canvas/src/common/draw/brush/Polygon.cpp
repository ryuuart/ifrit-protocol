/** @file
 * The stored polygon and the gestures through a collection of them.
 */

#include "PolygonMath.h"

#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Engine.h>
#include <sigildraw/brush/Hatch.h>
#include <sigildraw/brush/Mass.h>
#include <sigildraw/brush/Polygon.h>
#include <sigildraw/brush/Wash.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

namespace {

float cross(SkPoint a, SkPoint b) { return a.fX * b.fY - a.fY * b.fX; }

SkPoint subtract(SkPoint a, SkPoint b) { return {a.fX - b.fX, a.fY - b.fY}; }

}  // namespace

std::vector<SkPoint> Polygon::intersect(const Line& line) const {
  std::vector<SkPoint> result;
  const SkPoint ray = subtract(line.to, line.from);
  for (size_t i = 0; i < vertices.size(); ++i) {
    const SkPoint from = vertices[i];
    const SkPoint edge = subtract(vertices[(i + 1) % vertices.size()], from);
    const float denominator = cross(ray, edge);
    if (std::abs(denominator) < 0.000001f) continue;
    const SkPoint between = subtract(from, line.from);
    const float alongRay = cross(between, edge) / denominator;
    const float alongEdge = cross(between, ray) / denominator;
    if (alongRay >= 0.0f && alongRay <= 1.0f && alongEdge >= 0.0f &&
        alongEdge <= 1.0f)
      result.push_back(
          {line.from.fX + ray.fX * alongRay, line.from.fY + ray.fY * alongRay});
  }
  std::ranges::sort(result, [&](SkPoint a, SkPoint b) {
    return std::hypot(a.fX - line.from.fX, a.fY - line.from.fY) <
           std::hypot(b.fX - line.from.fX, b.fY - line.from.fY);
  });
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
