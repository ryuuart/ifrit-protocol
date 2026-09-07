#pragma once
/** @file
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
 * banded to a width. `path::lattice` is the same fill as MARKS, for a
 * caller that wants to do any of that.
 */
#include <include/core/SkPath.h>

#include <glm/vec2.hpp>
#include <optional>

namespace sigil::geometry::shapes {

/** How a silhouette is hatched. */
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
  /** The most lines one hatch lays down — a bound, not a preference. */
  int maxLines = 10000;
  bool operator==(const Hatch&) const = default;
};

/** `outline` filled with parallel lines, as one path of open contours. */
SkPath hatchOutline(const SkPath& outline, const Hatch& hatch = {});

}  // namespace sigil::geometry::shapes
