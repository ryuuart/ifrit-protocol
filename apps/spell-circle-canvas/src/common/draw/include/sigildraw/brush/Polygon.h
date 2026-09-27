#pragma once

/** @file
 * @ingroup draw-brush
 *
 * A stored polygon, and the one gesture that runs through a collection
 * of them.
 */

#include <include/core/SkPoint.h>

#include <span>
#include <vector>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

struct Tool;
struct Hatch;
struct Mass;

/** Stored polygon geometry. The vertices are the whole of its state and
 *  every edge is derived from them when asked; what paints the shape is
 *  an engine or a tool verb taking those vertices, never the polygon
 *  itself. */
struct Polygon {
  Polygon() = default;
  explicit Polygon(std::vector<SkPoint> points) : vertices(std::move(points)) {}

  std::vector<SkPoint> vertices;

  /** Where the segment from @p from to @p to crosses the edges, nearest
   *  @p from first. */
  [[nodiscard]] std::vector<SkPoint> intersect(SkPoint from, SkPoint to) const;
  [[nodiscard]] Polygon translated(float x, float y) const;

  [[nodiscard]] bool empty() const { return vertices.size() < 3; }
};

/** One hatch or mass gesture through an even-odd collection: the first
 *  polygon is the outer boundary and the rest cut holes or stand as
 *  islands. */
void hatchArray(Pen& pen, const Tool& tool, std::span<const Polygon> polygons,
                const Hatch& style);
/** The same collection filled with mass rather than hatched. */
void massArray(Pen& pen, const Tool& tool, std::span<const Polygon> polygons,
               const Mass& style);

}  // namespace sigil::draw::brush
