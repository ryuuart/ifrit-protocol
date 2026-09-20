#pragma once

/** @file
 * WHAT AN OFFSET WALK DOES AT A CONTOUR'S REAL VERTICES: the single miter
 * point a turn toward the offset side collapses to, the arc a turn away
 * from it opens, and which sampled points a miter has already answered
 * for.
 *
 * One body, because there are two offset walks — the rail a constant
 * distance out and the rail a width law cuts — and a corner they repaired
 * separately would drift apart. The walks themselves stay with their
 * callers: each samples the contour at its own spacing, and the samples
 * are what a caller's output is otherwise made of. Private to the path
 * leaf; no consumer include path carries it.
 */

#include <functional>
#include <glm/vec2.hpp>
#include <span>
#include <vector>

#include "sigilgeometry/path/Contour.h"

class SkPathBuilder;

namespace sigil::geometry::path {

/** ONE REAL VERTEX, OFFSET: either the single MITER point that replaces
 *  the two offset edge ends, or those two ends with an ARC between them.
 *
 *  `radius` is the offset's magnitude AT THIS VERTEX — what the arc is
 *  struck with, and how far either side the sampled points the join
 *  stands for reach. A width law may read a different number at every
 *  corner, so it rides the join rather than the walk. */
struct OffsetJoin {
  float distance = 0;  ///< arc length along the contour
  float radius = 0;
  /** HOW FAR EITHER SIDE OF `distance` THIS JOIN ANSWERS FOR: every
   *  sample inside it is the corner's own place and belongs to the join
   *  rather than to the walk. A miter sits back from the vertex by the
   *  offset over the tangent of half the interior angle — at a right
   *  angle exactly the offset, below one further, above one less — and
   *  every sample inside that has already passed it, so the rail doubles
   *  back if the walk writes them. An arc answers for its vertex alone:
   *  a turn away from the offset side spreads the two offset edges
   *  apart, so no sample beyond the vertex has overshot anything. */
  float answers = 0;
  bool miter = false;
  glm::vec2 point{0, 0};  ///< miter: the single replacement point
  glm::vec2 entering{0, 0}, leaving{0, 0};
  glm::vec2 vertex{0, 0};
  float startRadians = 0, sweepRadians = 0;
  bool arc = false;
};

/** The joins along one contour for a width law read at a DISTANCE along
 *  it, in the public frame: positive `across` is LEFT of travel.
 *
 *  The law is read AT THE VERTEX. A corner is one place, and the two
 *  edges either side of it may be different widths; the width of that
 *  place is the one the join is struck with. `stride` is the spacing the
 *  caller walks the contour at, which is also how finely a corner is
 *  searched for. */
std::vector<OffsetJoin> offsetJoins(
    const Contour& contour,
    const std::function<float(float distance)>& acrossAt, float stride);

/** Write one rail point, opening the rail when it has not started. */
void appendOffsetPoint(SkPathBuilder& out, glm::vec2 point, bool& started);

/** Write one join into the rail being built. */
void appendOffsetJoin(SkPathBuilder& out, const OffsetJoin& join,
                      bool& started);

/** Is a sample at `distance` inside the reach of some join — a place a
 *  join has already answered for? Those samples are dropped: the join
 *  stands for them, and keeping them is what doubles a rail back into a
 *  loop at the inside of a turn. */
bool swallowedByJoin(std::span<const OffsetJoin> joins, const Contour& contour,
                     float distance);

/** Has the walk already written a join for this place? `written` is the
 *  joins it has emitted so far, in contour order, and it emits each on
 *  reaching that join's distance — so a sample standing no further along
 *  than the last one written is that join's own vertex, written once
 *  already. A contour's real vertex falls exactly on a sample whenever
 *  the walk's step divides its edges, which is every box. */
bool joinAlreadyWrote(std::span<const OffsetJoin> written, float distance);

}  // namespace sigil::geometry::path
