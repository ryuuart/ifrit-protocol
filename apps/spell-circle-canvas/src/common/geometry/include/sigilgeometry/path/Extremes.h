#pragma once
/** @file
 * NODES PUT WHERE A CURVE TURNS. An outline drawn by hand, or one that
 * came out of a boolean, has its nodes wherever the drawing put them;
 * an outline that is going to be scaled, hinted, interpolated or read as
 * a set of masters wants one at every place the curve reaches its
 * extreme in x or in y, because those are the places a rasteriser and a
 * reader both look for the shape's edge.
 *
 * `Where` names the three places worth a node — the axis extremes, the
 * inflections where a cubic changes which way it bends, and the points
 * of maximum curvature — and `minDepthPx` is the one dial that decides
 * how shallow a turn may be and still earn one. The drawn curve does not
 * move: a node is inserted by splitting a piece into two of its own
 * kind, which is exactly the same curve read in two halves.
 */
#include <include/core/SkPath.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <vector>

namespace sigil::geometry::path {

/** Which turns of a curve are worth a node. A bit set: any union of
 *  these is a valid value. */
enum class Where : uint8_t {
  /** Where the curve reaches its furthest in x or in y. */
  Axis = 1,
  /** Where a cubic changes which way it bends. */
  Inflection = 2,
  /** Where a curve is bending hardest. */
  MaxCurvature = 4,
};
constexpr Where operator|(Where a, Where b) {
  // the type is a bit set; any union of enumerators is a valid value
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return Where(uint8_t(a) | uint8_t(b));
}
constexpr bool has(Where mask, Where one) {
  return (uint8_t(mask) & uint8_t(one)) != 0;
}

struct ExtremeOptions {
  Where where = Where::Axis;
  /** HOW FAR PAST ITS OWN ENDS a curve must reach before the turn earns
   *  a node, in px. A few units of bulge is a rounding artefact rather
   *  than a feature of the drawing, and a node there is a node that will
   *  wander; zero inserts one wherever the arithmetic finds a turn at
   *  all. Only the axis extremes are measured this way — an inflection
   *  and a curvature peak have no depth to measure. */
  float minDepthPx = 1.0f;
  bool operator==(const ExtremeOptions&) const = default;
};

/** `path` with a node at every turn the options name. Each piece is
 *  split into pieces of its own kind at those parameters, so the curve
 *  drawn is the curve that was drawn. A path holding no curve comes back
 *  as it was. */
SkPath extremes(const SkPath& path, const ExtremeOptions& options = {});

/** WHERE THOSE NODES WOULD GO, without putting them there — what a
 *  caller marking up an outline, or explaining why it gained a node,
 *  reads. In path order, contour by contour. */
std::vector<glm::vec2> extremeNodes(const SkPath& path,
                                    const ExtremeOptions& options = {});

}  // namespace sigil::geometry::path
