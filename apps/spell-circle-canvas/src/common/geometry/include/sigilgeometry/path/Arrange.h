#pragma once
/** @file
 * Where item i of n goes: spread around a ring, or filling the cells of a
 * grid.
 *
 * Two arrangements every catalog of placements arrives at on its own — n
 * items stepped around a ring from a start angle, and n items filling
 * columns by rows of a module — and neither may be spelled twice. Two
 * spellings of the same arrangement agree until they associate their
 * multiplications differently, and then two pictures of the one ring
 * disagree by a pixel with nothing in either file to say why.
 *
 * These are functions of numbers alone. They take the centre, the radii,
 * the module and the gaps, answer one point or one rect, allocate nothing
 * and know nothing about what is being placed — which is what lets a
 * layout scheme measuring children and a routine filling a buffer of
 * sprite positions reach the same body.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

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

/** Where item `i` of that run falls, measured from `start`. The unit is
 *  the caller's — radians around a ring, arc length along a contour — and
 *  the answer comes back in it. */
inline float along(float start, float extent, size_t i, size_t count,
                   Turn turn) {
  return start + step(extent, count, turn) * (float)i;
}

/** The point at `radians` on the ellipse at `center` with a radius per
 *  axis. Angles run clockwise on screen because y grows downward, and
 *  −π/2 is twelve o'clock. Equal radii give a circle; unequal ones the
 *  ellipse that a ring inscribed in an oblong box actually is. */
inline SkPoint onEllipse(SkPoint center, SkVector radii, float radians) {
  return {center.fX + radii.fX * std::cos(radians),
          center.fY + radii.fY * std::sin(radians)};
}

/** The centre of item `i` of `count` on that ellipse, entered at
 *  `startRadians` and swept through `sweepRadians` — the two halves above
 *  in the order every ring wants them. A caller that also needs the angle
 *  itself, to face an item along its own spoke, takes `along` and
 *  `onEllipse` separately rather than computing the angle twice. */
inline SkPoint onRing(size_t i, size_t count, SkPoint center, SkVector radii,
                      float startRadians, float sweepRadians, Turn turn) {
  return onEllipse(center, radii,
                   along(startRadians, sweepRadians, i, count, turn));
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
  const size_t cols = (size_t)std::max(columns, 1);
  return {(int)(index % cols), (int)(index / cols)};
}

/** The module that fits `columns` by `rows` of itself, plus the gaps
 *  between them, exactly into `container`. Gaps sit only BETWEEN modules,
 *  so the outer edges of the grid are the container's own. */
inline SkSize moduleSize(SkSize container, int columns, int rows, SkSize gap) {
  const float cols = (float)std::max(columns, 1);
  const float rws = (float)std::max(rows, 1);
  return {(container.width() - gap.width() * (cols - 1)) / cols,
          (container.height() - gap.height() * (rws - 1)) / rws};
}

/** The rect a block of cells covers: `cell` is its top-left module on a
 *  grid of `module`-sized modules laid from `origin` with `gap` between
 *  them, and the block spans `columnSpan` by `rowSpan` of them, swallowing
 *  the gaps it crosses.
 *
 *  A cell is NOT clamped to any column or row count — none is passed, and
 *  a cell past the end of a grid gets the rect it would have had there,
 *  outside the container. Clamping is the caller's decision because the
 *  caller is the one that knows whether landing outside is an error or a
 *  bleed. */
inline SkRect cellRect(Cell cell, SkSize module, SkSize gap = {0, 0},
                       SkPoint origin = {0, 0}, int columnSpan = 1,
                       int rowSpan = 1) {
  const float cs = (float)std::max(columnSpan, 1);
  const float rs = (float)std::max(rowSpan, 1);
  return SkRect::MakeXYWH(
      origin.fX + (module.width() + gap.width()) * (float)cell.column,
      origin.fY + (module.height() + gap.height()) * (float)cell.row,
      module.width() * cs + gap.width() * (cs - 1),
      module.height() * rs + gap.height() * (rs - 1));
}

}  // namespace sigil::geometry::arrange
