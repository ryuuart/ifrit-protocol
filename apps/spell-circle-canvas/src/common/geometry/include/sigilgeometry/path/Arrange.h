#pragma once
/** @file
 * @ingroup geometry-path
 *
 * Where item i of n goes: stepped along a run, spread around a ring or an
 * ellipse, or filling the cells of a grid — and which way it faces there.
 *
 * This is the one place the tree turns an angle into a point. Two
 * spellings of the same arrangement agree until they associate their
 * multiplications differently, and then two pictures of the one ring
 * disagree by a fraction of a pixel with nothing in either file to say
 * why; so a layout measuring children, a routine filling a buffer of
 * sprite positions and a polar frame resolving a plate's angles all
 * reach these bodies, and none spells a cosine of its own.
 *
 * These are functions of numbers alone, over glm: they take the centre,
 * the radii, the module and the gaps, answer one point, one heading or
 * one rect, allocate nothing and know nothing about what is being placed.
 *
 * Angles given in radians are SCREEN angles: measured from +x and
 * increasing in the direction that looks clockwise, because y grows
 * downward, so −π/2 is twelve o'clock. A `Ring` is stated in degrees in
 * the same convention.
 */

#include <glm/vec2.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Outline.h"

/** WHERE ITEM i OF n GOES, AND WHICH WAY IT FACES: the run, the ring and
 *  the grid of modules, as plain arithmetic that every placement in the
 *  tree steps through rather than spelling again. */
namespace sigil::geometry::arrange {

/** Whether a run comes back round to where it began, which is the whole
 *  difference between the two ways to divide an extent among n items. */
enum class Turn : uint8_t {
  /** Both ends are occupied: the first item at the start of the extent and
   *  the last at its end, so n items take n−1 steps. */
  Open,
  /** The far end IS the start: n items take n steps and the last one stops
   *  short of landing on top of the first. */
  Closed,
};

/** The distance between neighbours in a run of `count` items over
 *  `extent`. Zero for a run of one, which sits at the start. */
inline float step(float extent, size_t count, Turn turn) {
  if (count <= 1) return 0.0f;
  return extent / (float)(turn == Turn::Closed ? count : count - 1);
}

/** Where item `index` of that run falls, measured from `start`. The unit
 *  is the caller's — radians around a ring, degrees on a dial, arc length
 *  along a contour — and the answer comes back in it. */
inline float along(float start, float extent, size_t index, size_t count,
                   Turn turn) {
  return start + step(extent, count, turn) * (float)index;
}

/** The unit vector pointing out along the screen angle `radians`. */
inline glm::vec2 direction(float radians) {
  return {std::cos(radians), std::sin(radians)};
}

/** The screen angle, in degrees, that `vector` points along: what an item
 *  laid on a run is turned by so its x axis follows the run. A zero
 *  vector points nowhere and answers 0. */
inline float heading(glm::vec2 vector) {
  if (vector.x == 0 && vector.y == 0) return 0;
  return std::atan2(vector.y, vector.x) * path::kRadToDeg;
}

/** The point at the screen angle `radians` on the ellipse at `center`
 *  with a radius per axis. Equal radii give a circle; unequal ones the
 *  ellipse that a ring inscribed in an oblong box actually is. */
inline glm::vec2 onEllipse(glm::vec2 center, glm::vec2 radii, float radians) {
  return {center.x + radii.x * std::cos(radians),
          center.y + radii.y * std::sin(radians)};
}

/** A RING OF ITEMS: the ellipse they stand on and the stretch of it they
 *  are dealt over, in degrees. The defaults are a whole turn entered at
 *  twelve o'clock, each item a step apart and the last short of the
 *  first; a fan is a partial `sweepDegrees` with `Turn::Open`, which puts
 *  the first item at `fromDegrees` and the last at the far end. */
struct Ring {
  glm::vec2 center{0, 0};
  /** The radius along each axis; equal radii are a circle. */
  glm::vec2 radii{1, 1};
  /** Where item 0 stands, in screen degrees: −90 is twelve o'clock. */
  float fromDegrees = -90.0f;
  /** The stretch the items are dealt over; negative runs anticlockwise. */
  float sweepDegrees = 360.0f;
  Turn turn = Turn::Closed;
  bool operator==(const Ring&) const = default;
};

/** The screen angle, in radians, at which item `index` of `count` stands
 *  on `ring`. */
inline float radiansOnRing(size_t index, size_t count, const Ring& ring) {
  return along(ring.fromDegrees * path::kDegToRad,
               ring.sweepDegrees * path::kDegToRad, index, count, ring.turn);
}

/** The screen angle, in radians, `fraction` of the way through `ring`'s
 *  sweep from its start — for an item placed by a quantity of its own
 *  rather than by its index. */
inline float radiansAt(float fraction, const Ring& ring) {
  return ring.fromDegrees * path::kDegToRad +
         ring.sweepDegrees * path::kDegToRad * fraction;
}

/** The centre of item `index` of `count` on `ring`. */
inline glm::vec2 onRing(size_t index, size_t count, const Ring& ring) {
  return onEllipse(ring.center, ring.radii,
                   radiansOnRing(index, count, ring));
}

/** WHERE AN ITEM STANDS AND WHICH WAY IT FACES: its centre, and the
 *  clockwise turn in degrees that stands it along the arrangement — the
 *  direction of travel at that point, so an item turned by it reads along
 *  a curve and stands upright at the top of a ring. */
struct Placement {
  glm::vec2 position{0, 0};
  float headingDegrees = 0.0f;
  bool operator==(const Placement&) const = default;
};

/** The item at the screen angle `radians` on the ellipse at `center`: its
 *  point, and its spoke turned a quarter clockwise as its heading, so an
 *  item at twelve o'clock is upright and one at three o'clock is turned a
 *  quarter. On an unequal ellipse the heading is the spoke's quarter turn,
 *  not the curve's own tangent. */
inline Placement placeOnEllipse(glm::vec2 center, glm::vec2 radii,
                                float radians) {
  return {onEllipse(center, radii, radians),
          radians * path::kRadToDeg + 90.0f};
}

/** Item `index` of `count` on `ring`, placed and faced. */
inline Placement placeOnRing(size_t index, size_t count, const Ring& ring) {
  return placeOnEllipse(ring.center, ring.radii,
                        radiansOnRing(index, count, ring));
}

/** An item at `position` on a run whose direction of travel there is
 *  `tangent`: faced along it. */
inline Placement placeAlong(glm::vec2 position, glm::vec2 tangent) {
  return {position, heading(tangent)};
}

/** A cell's place in a grid, counted from the top-left one. */
struct Cell {
  int column = 0;
  int row = 0;
  bool operator==(const Cell&) const = default;
};

/** Which cell `index` is when cells fill each row left to right before
 *  starting the next. */
inline Cell cellAt(size_t index, int columns) {
  const size_t columnCount = (size_t)std::max(columns, 1);
  return {(int)(index % columnCount), (int)(index / columnCount)};
}

/** The module that fits `columns` by `rows` of itself, plus the gaps
 *  between them, exactly into `container`. Gaps sit only BETWEEN modules,
 *  so the outer edges of the grid are the container's own. */
inline glm::vec2 moduleSize(glm::vec2 container, int columns, int rows,
                            glm::vec2 gap = {0, 0}) {
  const float columnCount = (float)std::max(columns, 1);
  const float rowCount = (float)std::max(rows, 1);
  return {(container.x - gap.x * (columnCount - 1)) / columnCount,
          (container.y - gap.y * (rowCount - 1)) / rowCount};
}

/** How a block of cells sits on its grid: the gap between modules, where
 *  the grid's top-left module is laid, and how many columns and rows the
 *  block spans. */
struct CellBlock {
  glm::vec2 gap{0, 0};
  glm::vec2 origin{0, 0};
  int columnSpan = 1;
  int rowSpan = 1;
  bool operator==(const CellBlock&) const = default;
};

/** The rect a block of cells covers: `cell` is its top-left module on a
 *  grid of `module`-sized modules, and the block swallows the gaps it
 *  crosses.
 *
 *  A cell is NOT clamped to any column or row count — none is passed, and
 *  a cell past the end of a grid gets the rect it would have had there,
 *  outside the container. Clamping is the caller's decision because the
 *  caller is the one that knows whether landing outside is an error or a
 *  bleed. */
inline path::Rect cellRect(Cell cell, glm::vec2 module,
                           const CellBlock& block = {}) {
  const float columnSpan = (float)std::max(block.columnSpan, 1);
  const float rowSpan = (float)std::max(block.rowSpan, 1);
  const glm::vec2 corner{
      block.origin.x + (module.x + block.gap.x) * (float)cell.column,
      block.origin.y + (module.y + block.gap.y) * (float)cell.row};
  const glm::vec2 extent{module.x * columnSpan + block.gap.x * (columnSpan - 1),
                         module.y * rowSpan + block.gap.y * (rowSpan - 1)};
  return path::Rect::of(corner, extent);
}

}  // namespace sigil::geometry::arrange
