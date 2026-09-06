/** @file
 * CDT under this library's types, and the dual it does not build.
 */
#include "sigilgeometry/path/Triangulate.h"

#include <CDT.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>

namespace sigil::geometry::path {
namespace {

/** The polygon `subject` cut to the side of the line through `on` facing
 *  `keep` — one step of Sutherland and Hodgman's clip, which is how a
 *  Voronoi cell is built: start from the bounds and cut once per
 *  neighbour. The line is given as a point on it and a normal, since
 *  that is what a perpendicular bisector already is. */
std::vector<glm::vec2> clipToHalfPlane(const std::vector<glm::vec2>& subject,
                                       glm::vec2 on, glm::vec2 keep) {
  std::vector<glm::vec2> out;
  if (subject.empty()) return out;
  const auto side = [&](glm::vec2 p) { return glm::dot(p - on, keep); };
  out.reserve(subject.size() + 2);
  for (size_t i = 0; i < subject.size(); ++i) {
    const glm::vec2 from = subject[i];
    const glm::vec2 to = subject[(i + 1) % subject.size()];
    const float a = side(from), b = side(to);
    if (a >= 0) out.push_back(from);
    // An edge that crosses the line contributes the crossing as well as
    // whichever of its ends was kept.
    if ((a >= 0) != (b >= 0)) {
      const float t = a / (a - b);
      out.push_back(from + (to - from) * t);
    }
  }
  return out;
}

std::vector<glm::vec2> rectPolygon(SkRect rect) {
  return {{rect.fLeft, rect.fTop},
          {rect.fRight, rect.fTop},
          {rect.fRight, rect.fBottom},
          {rect.fLeft, rect.fBottom}};
}

}  // namespace

glm::vec2 Triangulation::circumcentre(size_t triangle) const {
  const glm::uvec3 t = triangles[triangle];
  const glm::vec2 a = points[t.x], b = points[t.y], c = points[t.z];
  const glm::vec2 ab = b - a, ac = c - a;
  const float twiceArea = 2.0f * (ab.x * ac.y - ab.y * ac.x);
  // Three collinear points have no circumcentre at all — the circle
  // through them is a line — so the centroid is answered instead, which
  // is the one point inside the degenerate triangle that is a function of
  // all three.
  if (std::abs(twiceArea) < 1e-12f) return (a + b + c) / 3.0f;
  const float abSquared = glm::dot(ab, ab), acSquared = glm::dot(ac, ac);
  return a + glm::vec2{(ac.y * abSquared - ab.y * acSquared) / twiceArea,
                       (ab.x * acSquared - ac.x * abSquared) / twiceArea};
}

float Triangulation::circumradius(size_t triangle) const {
  return glm::length(points[triangles[triangle].x] - circumcentre(triangle));
}

std::vector<uint32_t> Triangulation::adjacent(uint32_t point) const {
  std::vector<uint32_t> found;
  for (const glm::uvec3 triangle : triangles) {
    if (triangle.x != point && triangle.y != point && triangle.z != point)
      continue;
    for (int corner = 0; corner < 3; ++corner) {
      const uint32_t other = triangle[corner];
      if (other == point) continue;
      if (std::find(found.begin(), found.end(), other) == found.end())
        found.push_back(other);
    }
  }
  return found;
}

Triangulation delaunay(std::span<const glm::vec2> points) {
  Triangulation out;
  if (points.empty()) return out;

  std::vector<CDT::V2d<float>> vertices;
  vertices.reserve(points.size());
  for (const glm::vec2 point : points) vertices.push_back({point.x, point.y});
  // Two points in the same place are one point to a triangulation, and a
  // duplicate left in is a degenerate triangle rather than an extra
  // vertex. The survivors keep their first-seen order.
  CDT::RemoveDuplicates(vertices);

  out.points.reserve(vertices.size());
  for (const CDT::V2d<float>& vertex : vertices)
    out.points.push_back({vertex.x, vertex.y});
  if (vertices.size() < 3) return out;

  CDT::Triangulation<float> mesh;
  mesh.insertVertices(vertices);
  mesh.eraseSuperTriangle();

  out.triangles.reserve(mesh.triangles.size());
  out.neighbours.reserve(mesh.triangles.size());
  for (const CDT::Triangle& triangle : mesh.triangles) {
    out.triangles.push_back({(uint32_t)triangle.vertices[0],
                             (uint32_t)triangle.vertices[1],
                             (uint32_t)triangle.vertices[2]});
    const auto across = [&](size_t edge) {
      const CDT::TriInd neighbour = triangle.neighbors[edge];
      return neighbour == CDT::noNeighbor ? Triangulation::noNeighbour
                                          : (uint32_t)neighbour;
    };
    out.neighbours.push_back({across(0), across(1), across(2)});
  }
  return out;
}

std::vector<Polyline> voronoi(const Triangulation& triangulation,
                              SkRect bounds) {
  std::vector<Polyline> cells(triangulation.points.size());
  if (triangulation.points.empty() || bounds.isEmpty()) return cells;

  // Who neighbours whom, gathered in one walk of the triangles rather
  // than by asking each point in turn, which would walk them all again
  // per point.
  std::vector<std::vector<uint32_t>> adjacency(triangulation.points.size());
  const auto join = [&](uint32_t a, uint32_t b) {
    std::vector<uint32_t>& list = adjacency[a];
    if (std::find(list.begin(), list.end(), b) == list.end())
      list.push_back(b);
  };
  if (triangulation.triangles.empty()) {
    // A set with no triangulation still has a diagram: two points split
    // the bounds down their bisector, and a row of collinear points cuts
    // it into slabs. With no triangles to read adjacency from, every
    // point is cut against every other — which is what the diagram is
    // defined as, and is affordable exactly because a set this degenerate
    // is the only one that reaches here.
    for (uint32_t i = 0; i < (uint32_t)triangulation.points.size(); ++i)
      for (uint32_t j = 0; j < (uint32_t)triangulation.points.size(); ++j)
        if (i != j) adjacency[i].push_back(j);
  }
  for (const glm::uvec3 triangle : triangulation.triangles)
    for (int corner = 0; corner < 3; ++corner) {
      const uint32_t a = triangle[corner], b = triangle[(corner + 1) % 3];
      join(a, b);
      join(b, a);
    }

  // One point alone has no neighbour to be cut against and owns the whole
  // of the bounds, which is the right answer and not a special case:
  // nothing is nearer to anywhere else.
  const std::vector<glm::vec2> box = rectPolygon(bounds);
  for (size_t i = 0; i < triangulation.points.size(); ++i) {
    const glm::vec2 here = triangulation.points[i];
    std::vector<glm::vec2> cell = box;
    for (const uint32_t other : adjacency[i]) {
      const glm::vec2 there = triangulation.points[other];
      // The perpendicular bisector: halfway between the two, facing the
      // one whose cell is being cut.
      cell = clipToHalfPlane(cell, (here + there) * 0.5f, here - there);
      if (cell.empty()) break;
    }
    if (cell.size() < 3) continue;
    cells[i].points = std::move(cell);
    cells[i].closed = true;
  }
  return cells;
}

std::vector<Polyline> voronoi(std::span<const glm::vec2> points,
                              SkRect bounds) {
  return voronoi(delaunay(points), bounds);
}

}  // namespace sigil::geometry::path
