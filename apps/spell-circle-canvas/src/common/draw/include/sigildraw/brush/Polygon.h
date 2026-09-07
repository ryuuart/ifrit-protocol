#pragma once

/** @file
 * A stored polygon and the interiors it can receive.
 */

#include <include/core/SkPoint.h>

#include <span>
#include <vector>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

class Engine;
struct Tool;
struct Wash;
struct Hatch;
struct Mass;

struct Line {
  SkPoint from{0, 0};
  SkPoint to{0, 0};

  bool operator==(const Line&) const = default;
};

/** Stored polygon geometry with direct natural-media operations. The
 *  vertices are the whole of its state; every edge is derived from them
 *  when asked. */
struct Polygon {
  Polygon() = default;
  explicit Polygon(std::vector<SkPoint> points) : vertices(std::move(points)) {}

  std::vector<SkPoint> vertices;

  /** Where @p line crosses the edges, nearest its start first. */
  [[nodiscard]] std::vector<SkPoint> intersect(const Line& line) const;
  void draw(Pen& pen, const Tool& tool) const;
  void fill(Pen& pen, const Wash& style) const;
  void wash(Pen& pen, const Wash& style) const;
  void hatch(Pen& pen, const Tool& tool, const Hatch& style) const;
  void mass(Pen& pen, const Tool& tool, const Mass& style) const;
  /** The same through an engine's current state. */
  void draw(Pen& pen, const Engine& engine) const;
  void fill(Pen& pen, const Engine& engine) const;
  void wash(Pen& pen, const Engine& engine) const;
  void hatch(Pen& pen, const Engine& engine) const;
  void mass(Pen& pen, const Engine& engine) const;
  /** Every active interior in the engine's order: wash, fill, mass,
   *  hatch, outline. */
  void show(Pen& pen, const Engine& engine) const;
  [[nodiscard]] Polygon translated(float x, float y) const;

  [[nodiscard]] bool empty() const { return vertices.size() < 3; }
};

/** One hatch or mass gesture through an even-odd collection: the first
 *  polygon is the outer boundary and the rest cut holes or stand as
 *  islands. */
void hatchArray(Pen& pen, const Tool& tool, std::span<const Polygon> polygons,
                const Hatch& style);
void massArray(Pen& pen, const Tool& tool, std::span<const Polygon> polygons,
               const Mass& style);

}  // namespace sigil::draw::brush
