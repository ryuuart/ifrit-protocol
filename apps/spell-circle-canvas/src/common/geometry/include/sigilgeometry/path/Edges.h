#pragma once

/** @file
 * SigilGeometry edge arithmetic — the ways to NARROW an outline before
 * something is drawn on it: down to the sub-contours that face chosen
 * box edges, in (or out) to a concentric copy of the whole silhouette,
 * and, for a polygon, in to a copy whose vertices still answer to the
 * source's one for one.
 *
 * The first two take an outline and return an outline, so a consumer
 * that dresses a path composes them with anything else that does; the
 * third takes vertices and gives vertices, because the correspondence is
 * what it is for.
 */

#include <include/core/SkPath.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <span>
#include <vector>

namespace sigil::geometry::path {

/** Which box edges a treatment applies to. */
enum class Edge : uint8_t {
  Top = 1,
  Right = 2,
  Bottom = 4,
  Left = 8,
  All = 15,
};
constexpr Edge operator|(Edge a, Edge b) {
  // the type is a bit set; any union of enumerators is a valid value
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return Edge(uint8_t(a) | uint8_t(b));
}
constexpr bool has(Edge mask, Edge e) {
  return (uint8_t(mask) & uint8_t(e)) != 0;
}

/** Extracts the sub-contours of @p outline that face the selected box
 *  edges. Facing is classified against the outline's bounds center
 *  (diagonal split, so rounded-rect corner arcs divide naturally
 *  between their two edges). Exact geometry via SkContourMeasure
 *  segment extraction; @p step is the classification sampling length
 *  in px. */
SkPath edges(const SkPath& outline, Edge mask, float step = 3.0f);

/** A concentric copy of @p outline: positive @p px shrinks, negative
 *  grows. Implemented as a stroke-and-fill offset, so it follows any
 *  silhouette — a chamfered panel, a star, a blob — not just rectangles,
 *  and MITRED, so a straight edge stays parallel to the one it came from.
 *
 *  `ops::offset` is the other spelling of the same idea, rounding its
 *  joins and simplifying the result: that one is the drawing operator,
 *  this one is the frame six pixels in. */
SkPath insetOutline(const SkPath& outline, float px);

/** THE VERTICES OF A POLYGON MOVED INWARD by @p distance, one for one:
 *  every edge of the result is parallel to the edge it came from and
 *  @p distance inside it, and vertex i of the result is where the two
 *  moved edges round vertex i meet. Positive shrinks, negative grows.
 *  Inward is read off the polygon's own winding, so either winding
 *  insets, and a reflex corner moves the way its two edges say rather
 *  than the way a convex one would. Fewer than three vertices come back
 *  as they were.
 *
 *  A corner KEEPS ITS VERTEX, which is what tells this apart from
 *  `insetOutline`: a caller pairing each source corner with its moved
 *  one — a bevel between the two, a chamfer band, a lid on a plinth —
 *  needs the correspondence an outline offset cannot give. The price is
 *  the mitre. A corner of interior angle θ moves distance / sin(θ/2)
 *  along its bisector, which at a needle-sharp corner runs far past the
 *  distance, so @p miterLimit caps how many distances a vertex may move
 *  (at least one; one blunts every corner to the distance itself). A
 *  capped corner is pulled back less than its true mitre and its two
 *  edges stand a little nearer the source than @p distance there — the
 *  corner is blunted, and the polygon loses no vertex. */
std::vector<glm::vec2> insetPolygon(std::span<const glm::vec2> polygon,
                                    float distance, float miterLimit = 4.0f);

}  // namespace sigil::geometry::path
