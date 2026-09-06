#pragma once
/** @file
 * THE OUTLINE OF A POINT SET, at a tightness.
 *
 * The convex hull and the alpha shape are one construction with one dial
 * between them, not two functions: an alpha shape is the hull with a
 * bound on how far it may span, and at no bound at all it IS the convex
 * hull. Written as two, a caller who wants an outline that follows a
 * crescent has to know which of them to reach for and how to move
 * between them; written as one, it is a number.
 */
#include <glm/vec2.hpp>
#include <limits>
#include <span>
#include <vector>

#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path {

/** THE RINGS THAT ENCLOSE `points`, closed.
 *
 *  `alpha` is the largest circumcircle a triangle of the set may have and
 *  still count as inside the shape. At infinity — the default — nothing
 *  is ever too large, every triangle is inside, and the answer is the ONE
 *  ring of the convex hull. Smaller values let the outline reach into
 *  concavities: a crescent comes back as a crescent rather than as the
 *  disc that contains it, and below the spacing of the points themselves
 *  the shape falls apart into islands, which is why the answer is a set
 *  of rings and not one.
 *
 *  A ring that encloses a hole comes back wound against the ring that
 *  contains it, so the set reads by the even-odd rule the rest of this
 *  library reads rings by. Fewer than three distinct points, or points
 *  all on one line, enclose nothing and answer nothing. */
[[nodiscard]] std::vector<Polyline> hull(
    std::span<const glm::vec2> points,
    float alpha = std::numeric_limits<float>::infinity());

}  // namespace sigil::geometry::path
