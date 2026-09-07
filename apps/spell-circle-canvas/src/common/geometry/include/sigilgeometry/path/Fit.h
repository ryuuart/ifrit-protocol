#pragma once
/** @file
 * A RUN OF POINTS AS FEW CUBICS. What a tracer, a stylus, a sampled
 * field line or a decoded stroke hands over is a dense run of points; a
 * drawing wants a curve with nodes where the shape turns and nowhere
 * else.
 *
 * The rule is Schneider's: fit ONE cubic to the whole run by least
 * squares, with the ends' own directions as the two tangents and the
 * chord lengths as the first guess at each point's parameter; improve
 * those parameters by Newton-Raphson against the fitted curve; and if
 * the worst point is still further off than the tolerance, split the run
 * there and fit both halves the same way. It is the algorithm every
 * autotrace is a variant of, and what it answers is the fewest cubics
 * that hold every point within the tolerance.
 *
 * This is not `toPath(sampled, smooth)`, `smoothThrough` or
 * `catmullRom`: those three build a curve THROUGH or AROUND a set of
 * controls, one piece per control, and the point count is the node
 * count. Here the point count is the input and the node count is the
 * answer.
 */
#include <include/core/SkPath.h>

#include <glm/vec2.hpp>
#include <span>

#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path {

/** The fewest cubics that hold every one of `points` within @p tolerance
 *  px of the curve. Fewer than two points is an empty path and exactly
 *  two is the line between them. Repeated points are one point: a run
 *  that stands still has no direction to fit. */
SkPath fitCurve(std::span<const glm::vec2> points, float tolerance = 1.0f);

/** The same over a polyline, closed when it is. A closed run is fitted
 *  from its seam back round to its seam and then closed, so the seam is
 *  the one node the fit is pinned at: every other node falls where the
 *  tolerance puts it, and the seam is a corner. */
SkPath fitCurve(const Polyline& line, float tolerance = 1.0f);

}  // namespace sigil::geometry::path
