#pragma once
/** @file
 * @ingroup geometry-kit
 *
 * A SILHOUETTE FILLED WITH LINES, as one path.
 *
 * The scanline lattice already answers what lies inside a set of rings,
 * and the offset already answers a silhouette narrowed before something
 * is drawn on it. A hatch is those two with a door that takes an
 * `SkPath` and gives one back — a caller with an outline in hand should
 * not have to flatten it into rings itself, and that door is why this is
 * a stock value rather than a second construction.
 *
 * What comes back are CENTRELINES joined into one multi-contour path,
 * which is what separates a hatch from clipping a line pattern to an
 * outline: every mark can be walked, drawn along with a tool, split, or
 * banded to a width. `hatchMarks` is the same fill as MARKS, for a caller
 * that wants to do any of that.
 *
 * `Hatch` is the one hatch value of every library above this one: a
 * natural-media brush lays its marks along it, and a decoration strokes
 * it at a width. What those add — the tool, the jitter, the ink — is
 * theirs; where the lines lie is this.
 */
#include <include/core/SkPath.h>

#include <glm/vec2.hpp>
#include <optional>
#include <span>
#include <vector>

#include "sigilgeometry/path/Lattice.h"
#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::shapes {

/** How a silhouette is hatched: a fill pattern of parallel lines over a
 *  region. */
struct Hatch {
  /** The distance between one line and the next. */
  float spacing = 6.0f;
  /** Which way the lines run, in radians clockwise from +x in Skia's
   *  y-down space. */
  float angle = 0.0f;
  /** What each gap is multiplied by after the one before it: one is an
   *  even hatch, more spreads the lines as the scan advances and less
   *  crowds them. */
  float taper = 1.0f;
  /** WHERE THE LADDER IS MEASURED FROM, so a hatch over a shape that
   *  moves keeps its lines where they were rather than crawling with
   *  it. Unset lays the first line half a gap inside the outline. */
  std::optional<glm::vec2> origin;
  /** The outline narrowed before it is filled: positive keeps the marks
   *  that far inside the edge, which is what a hatch that must not touch
   *  its own keyline asks for. */
  float inset = 0.0f;
  /** A second pass at a right angle to the first, laid after it: the
   *  crosshatch. */
  bool cross = false;
  /** The most lines one pass lays down — a bound, not a preference. */
  int maxLines = 10000;
  bool operator==(const Hatch&) const = default;
};

/** `outline` filled with parallel lines, as one path of open contours. */
SkPath hatchOutline(const SkPath& outline, const Hatch& hatch = {});

/** The same fill over RINGS, read under the even-odd rule, as marks: the
 *  first pass in scan order, then the cross pass when there is one. */
std::vector<path::LatticeMark> hatchMarks(std::span<const path::Polyline> rings,
                                          const Hatch& hatch = {});

}  // namespace sigil::geometry::shapes
