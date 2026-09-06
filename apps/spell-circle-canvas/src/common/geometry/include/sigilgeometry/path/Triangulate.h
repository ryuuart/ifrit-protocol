#pragma once
/** @file
 * THE DELAUNAY TRIANGULATION OF A PLANAR POINT SET, AND ITS DUAL.
 *
 * One construction answers two questions that look unrelated and are the
 * same one. The triangulation is the mesh over a scatter in which no
 * point falls inside any triangle's circumcircle — the triangles as
 * near-equilateral as the points allow — which is what a surface fitted
 * to samples, a spring lattice over a scatter, and "who is adjacent to
 * whom" all want. Its dual, the Voronoi diagram, is the region around
 * each point that is nearer to it than to any other: a cell map, a
 * shattering, a stipple's territory, and the centroids a Lloyd
 * relaxation walks toward.
 *
 * The triangulation is `CDT`'s. What is here is the shape of the answer
 * in this library's own types — points, indices and `Polyline` cells —
 * and the dual, which CDT does not build.
 *
 * NO DRAWING HAPPENS HERE. Cells come back as rings; what is painted
 * inside them is the caller's.
 */
#include <include/core/SkRect.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <vector>

#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path {

/** A TRIANGULATED POINT SET: the points, the triangles over them, and
 *  which triangle lies across each edge.
 *
 *  `points` is what was actually triangulated, and it is not necessarily
 *  what was handed in: two points in the same place are one point to a
 *  triangulation, so duplicates are dropped and the survivors keep their
 *  first-seen order. A caller that must map an answer back to its own
 *  list compares positions, or de-duplicates before asking. */
struct Triangulation {
  /** The value an edge on the outer boundary names instead of a
   *  triangle: there is nothing across it. */
  static constexpr uint32_t noNeighbour = 0xFFFFFFFFu;

  std::vector<glm::vec2> points;
  /** Three point indices per triangle. */
  std::vector<glm::uvec3> triangles;
  /** What lies across each of a triangle's three edges, where edge `i`
   *  runs from its vertex `i` to its vertex `i + 1`. */
  std::vector<glm::uvec3> neighbours;

  [[nodiscard]] size_t size() const { return triangles.size(); }
  /** The centre of the triangle's circumcircle — the point equidistant
   *  from its three corners, and the corner every Voronoi cell around it
   *  is built from. A degenerate triangle has none, and answers its own
   *  centroid instead. */
  [[nodiscard]] glm::vec2 circumcentre(size_t triangle) const;
  /** The radius of that circle. What an alpha shape thresholds on: a
   *  triangle spanning a gap in the points has a large one. */
  [[nodiscard]] float circumradius(size_t triangle) const;
  /** Every point index that shares an edge with `point`. */
  [[nodiscard]] std::vector<uint32_t> adjacent(uint32_t point) const;
};

/** THE DELAUNAY TRIANGULATION of `points`. Fewer than three distinct
 *  points, or points all on one line, bound no area and answer a
 *  triangulation with the points in it and no triangles. */
[[nodiscard]] Triangulation delaunay(std::span<const glm::vec2> points);

/** THE VORONOI CELL OF EVERY POINT, one ring per point of
 *  `triangulation.points` and in that order, each clipped to `bounds`.
 *
 *  A cell is the set of places nearer to its own point than to any
 *  other, which is the intersection of the half-planes bisecting it with
 *  each of its neighbours. The cells of the points on the outside of the
 *  set are unbounded, and it is `bounds` that closes them — without a
 *  rect to cut against there is no ring to answer, which is why the
 *  bounds is an argument and not an option. A point whose cell falls
 *  entirely outside the bounds gets an empty ring, so the answer stays
 *  parallel to the points.
 *
 *  A set too degenerate to triangulate — two points, or a row of them on
 *  one line — still has a diagram, and gets it: with no triangles to read
 *  adjacency from, every point is cut against every other. */
[[nodiscard]] std::vector<Polyline> voronoi(const Triangulation& triangulation,
                                            SkRect bounds);
/** The same, triangulating first. */
[[nodiscard]] std::vector<Polyline> voronoi(std::span<const glm::vec2> points,
                                            SkRect bounds);

}  // namespace sigil::geometry::path
