#pragma once

/** @file
 * SigilGeometry edge arithmetic — the two ways to NARROW an outline
 * before something is drawn on it: down to the sub-contours that face
 * chosen box edges, and in (or out) to a concentric copy of the whole
 * silhouette.
 *
 * Both take an outline and return an outline, so a consumer that dresses
 * a path composes them with anything else that does.
 */

#include <include/core/SkPath.h>

#include <cstdint>

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

}  // namespace sigil::geometry::path
