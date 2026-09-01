#pragma once

/** @file
 * SigilGeometry curves — splines that cross space, held as VALUES. A
 * Spline3 is control points + a type + closure: edit any of them and
 * re-evaluate; nothing downstream is baked until asked. Three
 * consumers, all regenerable from the same spline:
 *
 *  - sample()/frames()/hangFrames(): positions and moving frames —
 *    the RAIL, and the spine for placement, cameras and point clouds;
 *  - sweep(): one 2D profile carried along that rail into a Mesh —
 *    a circle makes a tube, a two-point line makes a ribbon, any
 *    flattened outline makes an extrusion. `curve/Sweep.h` is the
 *    subject on its own: the profiles, the options, the ring executor
 *    and the runtime that carries one;
 *  - project(): the curve as a 2D SkPath under a Camera — draw the
 *    SAME spline as a glowing overlay over the scene that swept it.
 *
 * CatmullRom interpolates THROUGH its points (the default; closed
 * loops wrap), Bezier reads points as cubic segments (3n+1 layout,
 * anchors interpolated), Linear is the polyline.
 */

#include <include/core/SkPath.h>

#include <glm/glm.hpp>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/curve/Frame.h"
#include "sigilgeometry/mesh/curve/Sweep.h"
#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::mesh::curve {

/** A 3D curve as control points plus the rule that reads them. Linear
 *  joins them, CatmullRom passes through them, and Bezier reads a
 *  3n+1 layout of anchor, out-handle, in-handle, anchor. Every
 *  evaluator here is uniform in the PARAMETER, so equal steps of t
 *  cover unequal arc length wherever the control points bunch up;
 *  `sampleArcLength()` is what trades that for even spacing. */
struct Spline3 {
  enum class Type : uint8_t { Linear, CatmullRom, Bezier };

  std::vector<glm::vec3> points;
  Type type = Type::CatmullRom;
  bool closed = false;

  /** Position at parameter t in [0,1] (uniform by segment). */
  glm::vec3 position(float t) const;
  /** Unit tangent at t (central difference). */
  glm::vec3 tangent(float t) const;
  /** Approximate length (chord sum over @p samples). */
  float length(int samples = 256) const;
  /** @p count positions, uniform in PARAMETER. */
  std::vector<glm::vec3> sample(int count) const;
  /** @p count positions, uniform in ARC LENGTH — even beads on the
   *  wire regardless of knot spacing. */
  std::vector<glm::vec3> sampleArcLength(int count) const;
};

/** @p count parallel-transport frames, arc-length spaced. The first
 *  normal starts nearest @p up and each subsequent frame rotates
 *  minimally — no Frenet flips at inflections. Closed splines get an
 *  even twist correction so the last frame meets the first. */
std::vector<Frame3> frames(const Spline3& spline, int count,
                           glm::vec3 up = {0, 1, 0});

/** @p count frames over a window of a CLOSED spline, walked evenly in
 *  the loop PARAMETER: the towed-banner rail. `head` is the leading
 *  edge and `span` the length trailing it, so advancing `head` alone
 *  tows the window around the curve. Each frame's binormal is the
 *  world-vertical hang direction — the component of straight down
 *  perpendicular to the tangent, carried unchanged through stretches
 *  where the curve is too near vertical to define one — so a profile
 *  swept on this rail never rolls upside-down the way one on
 *  parallel-transport frames does. `t` is the position in the window,
 *  0 at the tail and 1 at the head, not the curve parameter. */
std::vector<Frame3> hangFrames(const Spline3& spline, int sections,
                               float head = 1, float span = 1);

/** The same sweep over a spline, whose parallel-transport frames are
 *  the rail (`segments` of them, seeded by `up`). */
Mesh sweep(const Spline3& spline, const path::Polyline& profile,
           const SweepOptions& options = {});

/** The spline as a 2D path under @p camera — points behind the near
 *  plane split the path into separate contours. */
SkPath project(const Spline3& spline, const camera::Camera& camera,
               SkSize viewport, int samples = 128);

}  // namespace sigil::geometry::mesh::curve
