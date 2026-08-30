#pragma once
/** @file
 * A pose along a spline: where the curve is at an arc length, and the
 * moving frame it carries there.
 *
 * The value a pose answers with is `Frame3` — the rail's own ring. A
 * pose and a rail frame are the same thing measured two ways, so there
 * is one type for both and nothing downstream learns a second spelling;
 * what is new here is the ADDRESSING, by distance rather than by index
 * or by parameter.
 *
 * Two overloads, the same pair `sweep()` offers: one over a rail you
 * built and hold — a camera resampling the same curve every frame builds
 * it once — and one over a spline, which builds the rail first.
 */

#include <vector>

#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/path/Pose.h"

namespace sigil::geometry::mesh::curve {

/** The pose at @p distance along @p rail, measured as the summed chord
 *  length between its frames. Between two frames the pose is
 *  interpolated and re-orthogonalized, so a denser rail is a truer pose.
 *
 *  `Wrap::Around` comes round the rail's total length; give it only to a
 *  rail that closes on itself (a closed spline's does — its last frame
 *  meets its first). A rail of fewer than two frames answers the default
 *  pose. */
Frame3 poseAlong(const std::vector<Frame3>& rail, float distance,
                 path::Wrap wrap = path::Wrap::Clamp);

/** The pose at @p distance along @p spline, over a rail of @p samples
 *  parallel-transport frames seeded by @p up — so the normal does not
 *  flip at an inflection, and on a closed spline the frame at the seam
 *  meets the frame the walk started with.
 *
 *  `Wrap::Around` comes round on a CLOSED spline and parks on an open
 *  one, which is the rule a camera or a mark riding the curve past its
 *  end wants. The distance is in the spline's own units;
 *  `Spline3::length()` is the total. */
Frame3 poseAlong(const Spline3& spline, float distance,
                 path::Wrap wrap = path::Wrap::Around, int samples = 256,
                 glm::vec3 up = {0, 1, 0});

}  // namespace sigil::geometry::mesh::curve
