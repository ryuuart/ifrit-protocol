#pragma once
/** @file
 * NODES TAKEN AWAY. The other half of the node arithmetic: an outline
 * that came out of a tracer, a sampler, a boolean or a hand carries
 * nodes that say nothing — a node in the middle of a straight run, a
 * node sitting on top of its neighbour, a curve whose handles lie on its
 * own chord — and every one of them is a node that has to be moved when
 * the shape is edited, interpolated or hinted.
 *
 * The word `simplify` is taken, by the boolean family's
 * self-intersection cleanup, which is a different operation on a
 * different thing.
 *
 * What is deliberately NOT here is the second half of a font editor's
 * tidy-up: setting each node's smooth-or-corner mode. Nothing in this
 * library carries a node type, and inventing one to serve one operator
 * would put a font editor's model into a drawing library.
 */
#include <include/core/SkPath.h>

namespace sigil::geometry::path {

/** Which redundancies a tidy-up takes out. Each is on by default,
 *  because each removes a node that says nothing; switching one off is
 *  for a caller that means to keep those. */
struct TidyOptions {
  /** Drop a node standing on a straight run between its neighbours. */
  bool collinear = true;
  /** Merge nodes that sit on top of one another. */
  bool duplicates = true;
  /** A curve whose handles lie on its own chord becomes a line. */
  bool reduceOrder = true;
  bool operator==(const TidyOptions&) const = default;
};

/** `path` with its redundant nodes gone. @p tolerance is the whole
 *  measure: it is how far, in px, the outline is allowed to move where a
 *  node is taken out, so a node exactly on a straight run goes at any
 *  tolerance and one a little off it goes only once the tolerance covers
 *  the distance. A closed contour stays closed and the drawn shape stays
 *  within the tolerance of the one it came from. */
SkPath tidy(const SkPath& path, float tolerance = 0.1f,
            const TidyOptions& options = {});

}  // namespace sigil::geometry::path
