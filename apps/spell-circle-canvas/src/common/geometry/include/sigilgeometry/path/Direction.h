#pragma once
/** @file
 * WHICH WAY ROUND AN OUTLINE IS DRAWN, and the two things that travel
 * with it: what order its contours come in, and which node each of them
 * starts at.
 *
 * They are one subject because they exist for one reason. An outline
 * whose holes wind the way its outers do is filled solid under the
 * non-zero rule; an outline whose contours arrive in another order, or
 * whose contour starts at another node, pairs its nodes against a second
 * outline in the wrong places and interpolates into a tangle. Fixing the
 * winding without fixing the other two leaves the outline still unable
 * to do the thing the winding was fixed for.
 *
 * The sign convention is Skia's y-down space, which `Polyline::signedArea`
 * states: a positive area is a CLOCKWISE ring. A winding test copied from
 * a y-up source reads inverted here.
 */
#include <include/core/SkPath.h>

#include <cstdint>
#include <span>
#include <vector>

#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path {

/** Which way the OUTER rings of an outline are drawn; a hole is always
 *  drawn the other way, which is what makes it a hole under the non-zero
 *  fill rule. */
enum class Winding : uint8_t {
  /** Outers clockwise, holes counter-clockwise — TrueType's convention
   *  in Skia's y-down space. */
  OutersClockwise,
  /** Outers counter-clockwise, holes clockwise — PostScript's. */
  OutersCounterClockwise,
};

/** WHERE ONE RING SITS AMONG THE OTHERS: how many rings enclose it, and
 *  which of those encloses it most tightly. Depth zero is an outer, one
 *  is a hole in that outer, two is an island inside the hole, and so on
 *  — the even-odd reading the rest of this library fills rings by. A
 *  ring nothing encloses has no parent and answers -1. */
struct Nesting {
  int depth = 0;
  int parent = -1;
  bool operator==(const Nesting&) const = default;
};

/** The nesting of every ring against every other, in the order they
 *  were given. A ring of fewer than three points encloses nothing and is
 *  enclosed by nothing. */
std::vector<Nesting> nesting(std::span<const Polyline> rings);

/** The three things `direction` puts right, each switchable on its own
 *  so a caller that needs only one of them pays for only that one. */
struct DirectionOptions {
  Winding winding = Winding::OutersCounterClockwise;
  /** Draw the contours outers-first, each outer followed by nothing —
   *  the nesting depth in stable order, so two outlines with the same
   *  nesting come out in the same order however they were built. */
  bool orderContours = true;
  /** Start every closed contour at its bottom-left node: the one lowest
   *  on the page, the leftmost of those where two are level. A rule
   *  rather than a preference — any two outlines that follow it start
   *  their contours at nodes that correspond. */
  bool resetStart = true;
};

/** `path` with its windings, its contour order and its start points put
 *  right. The drawn shape does not move; every one of the three changes
 *  is about which way the pen went. */
SkPath direction(const SkPath& path, const DirectionOptions& options = {});

}  // namespace sigil::geometry::path
