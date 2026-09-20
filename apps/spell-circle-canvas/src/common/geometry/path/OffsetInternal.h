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

/** ONE REAL VERTEX, OFFSET: either the MITER the two offset edges are
 *  cut back to where they fold across each other, or those two ends
 *  with an ARC between them where the turn spreads them apart.
 *
 *  `radius` is the offset's magnitude AT THIS VERTEX — what the arc is
 *  struck with, and how far either side the sampled points the join
 *  stands for reach. A width law may read a different number at every
 *  corner, so it rides the join rather than the walk. */
struct OffsetJoin {
  float distance = 0;  ///< arc length along the contour
  float radius = 0;
  /** HOW FAR BACK ALONG THE CONTOUR AND HOW FAR ON THIS JOIN ANSWERS
   *  FOR: every sample inside is the corner's own place and belongs to
   *  the join rather than to the walk.
   *
   *  A turn toward the offset side folds the two offset edges across
   *  each other, and they meet the offset over the tangent of half the
   *  interior angle back from where they end — at a right angle exactly
   *  the offset, above one less, below one further, and without bound
   *  as the turn approaches a reversal. Every sample inside that has
   *  already passed the meeting, so the rail doubles back if the walk
   *  writes them.
   *
   *  NEITHER SIDE REACHES PAST THE NEIGHBOURING CORNER. The fold is
   *  between the two edges this vertex sits between; past the next
   *  vertex the contour has turned away, and a sample there stands off
   *  an edge this join never met. An open contour's own ends bound it
   *  the same way. So the two are separate numbers: a corner between a
   *  long edge and a short one answers far in one direction and little
   *  in the other.
   *
   *  An arc answers for its vertex alone, and both are zero: a turn away
   *  from the offset side spreads the two offset edges apart, so no
   *  sample beyond the vertex has overshot anything. */
  float answersBefore = 0, answersAfter = 0;
  bool miter = false;
  /** MITER: where the edge arriving is cut and where the edge leaving
   *  is, in place of both their ends. They are ONE PLACE — the point
   *  the two fold across each other at — wherever the corner has the
   *  room either side for the fold to close; where a neighbouring
   *  corner is nearer than that, each edge is cut at the neighbour
   *  instead and the pair is the chord across the corner. */
  glm::vec2 cutEntering{0, 0}, cutLeaving{0, 0};
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
 *  loop at the inside of a turn. On a closed contour two places are as
 *  far apart as the nearer way round the seam. */
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
