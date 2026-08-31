#pragma once
/** @file
 * A pose along an outline: where the curve is at an arc length, which
 * way it heads there, and which way is sideways.
 *
 * `Contour` answers position and tangent at a distance. A pose is that
 * answer plus the two things every caller placing something ALONG a
 * curve then writes for itself — the sideways direction, and which end
 * a distance past the end lands on. Both are conventions rather than
 * measurements, and both are easy to spell two different ways in two
 * files, which is what this type and this verb exist to prevent.
 */
#include <cstdint>
#include <glm/vec2.hpp>
#include <span>

#include "sigilgeometry/path/Contour.h"

namespace sigil::geometry::path {

/** What a distance outside [0, length] means. */
enum class Wrap : uint8_t {
  /** Park at the nearer end. */
  Clamp,
  /** Come round — on CLOSED geometry only; an open curve still parks,
   *  because an open curve has two ends and no seam to come round
   *  through. */
  Around,
};

/** Where a curve is at an arc length, and how it is oriented there.
 *
 *  `normal` is the tangent turned a quarter turn toward +y, which in
 *  Skia's y-down space is to the RIGHT of the direction of travel — the
 *  side a positive offset lies on. It carries no information the tangent
 *  does not; it carries the CONVENTION, so a caller offsetting sideways
 *  never picks a sign.
 *
 *  `distance` is where the pose was actually taken, after @ref Wrap
 *  resolved it — so a caller can tell a clamped read from an interior
 *  one without repeating the policy. */
struct Pose {
  glm::vec2 position{0, 0};
  glm::vec2 tangent{1, 0};
  glm::vec2 normal{0, 1};
  float distance = 0;
};

/** The pose at @p distance along @p contour. A contour that cannot be
 *  evaluated there answers the default pose (the origin, heading +x). */
Pose poseAlong(const Contour& contour, float distance, Wrap wrap = Wrap::Clamp);

/** The pose at @p distance along @p contours walked as ONE coordinate:
 *  every contour in order, each starting where the previous one ended.
 *  This is what a path — which is a list of contours — means by "distance
 *  along", so a run that a frame cut into several pieces still carries
 *  one continuous measure.
 *
 *  `Wrap::Around` comes round the TOTAL, and only when every contour is
 *  closed; anything else clamps. @ref sigil::geometry::path::totalLength()
 *  is that total. */
Pose poseAlong(std::span<const Contour> contours, float distance,
               Wrap wrap = Wrap::Clamp);

/** The summed length of @p contours — the coordinate the span overload
 *  of @ref poseAlong walks. */
float totalLength(std::span<const Contour> contours);

/** Whether every contour in @p contours is closed, which is the
 *  condition under which a distance past the end comes round rather than
 *  parking. An empty span is not closed: there is nothing to come round.
 */
bool closedThroughout(std::span<const Contour> contours);

}  // namespace sigil::geometry::path
