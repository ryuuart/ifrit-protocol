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
 *    flattened outline makes an extrusion;
 *  - project(): the curve as a 2D SkPath under a Camera — draw the
 *    SAME spline as a glowing overlay over the scene that swept it.
 *
 * CatmullRom interpolates THROUGH its points (the default; closed
 * loops wrap), Bezier reads points as cubic segments (3n+1 layout,
 * anchors interpolated), Linear is the polyline.
 */

#include <include/core/SkPath.h>

#include <functional>
#include <glm/glm.hpp>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
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

/** An orthonormal moving frame on the curve. */
struct Frame3 {
  glm::vec3 position{0, 0, 0};
  glm::vec3 tangent{0, 0, 1};
  glm::vec3 normal{0, 1, 0};    // "up", parallel-transported
  glm::vec3 binormal{1, 0, 0};  // tangent x normal
  float t = 0;                  // curve parameter
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

/** The cross-sections `sweep()` carries. A profile is a plain
 *  `path::Polyline` in Skia's y-down 2D space — x runs along the
 *  frame's binormal, y against its normal, the convention
 *  `mesh::extrude()` uses — so any outline reaches a sweep by being
 *  flattened. The two below are UNIT shapes: `SweepOptions::scale`
 *  sizes them, which is why a radius or a width is not a parameter
 *  here. */
namespace profile {

/** The unit circle, one point per @p sides step around it plus the
 *  seam point DUPLICATED — the contour is open and its first and last
 *  points coincide — because the swept surface's u must run 0 to 1
 *  across the seam instead of wrapping back onto vertex zero. */
path::Polyline circle(int sides = 12);

/** The unit-width segment across the frame's binormal: two points,
 *  open. Swept, it is a flat band — pair it with
 *  `SweepOptions::Normals::Frame` so the band faces its rail. */
path::Polyline line();

/** The first contour of @p outline, flattened. The door from the 2D
 *  shape vocabulary: a star, a squircle, an `ops::PathOp` recipe's
 *  result. A closed outline sweeps as a wrapped ring; an open one
 *  sweeps as a strip. */
path::Polyline fromPath(const SkPath& outline, float tolerance = 0.4f);

}  // namespace profile

/** How a profile rides a rail. */
struct SweepOptions {
  /** Where each swept vertex's normal comes from. */
  enum class Normals : uint8_t {
    Radial,     ///< the profile offset itself — a round profile's outward
    Frame,      ///< the rail's normal — a flat profile's facing
    Geometric,  ///< averaged from the formed triangles — any profile
  };

  int segments = 96;  ///< rings along the spline, when a spline is given
  float scale = 1;    ///< the profile's size
  /** Size over t (multiplies `scale`); null = constant. */
  std::function<float(float t)> taper;
  glm::vec3 up = {0, 1, 0};  ///< seeds the rail's first normal
  Normals normals = Normals::Radial;
  /** Close the rail's two ends with a fan to the ring's centre. The
   *  fan assumes a convex profile, and a closed spline has no ends,
   *  so the spline overload drops it for one. */
  bool caps = false;
};

/** Carry @p profile along @p rail into one Mesh. THE swept primitive:
 *  a circle profile forms a tube, a two-point line forms a ribbon or
 *  a banner, a flattened outline forms an extrusion along the curve.
 *  Each ring is the profile placed on one frame — x along the
 *  binormal, y against the normal — scaled by `scale` times `taper`
 *  at that frame's t. u runs across the profile and v is the frame's
 *  t. A GPU executor forming the same geometry writes exactly these
 *  vertices from the same rail. */
Mesh sweep(const std::vector<Frame3>& rail, const path::Polyline& profile,
           const SweepOptions& options = {});

/** The same sweep over a spline, whose parallel-transport frames are
 *  the rail (`segments` of them, seeded by `up`). */
Mesh sweep(const Spline3& spline, const path::Polyline& profile,
           const SweepOptions& options = {});

/** The spline as a 2D path under @p camera — points behind the near
 *  plane split the path into separate contours. */
SkPath project(const Spline3& spline, const camera::Camera& camera,
               SkSize viewport, int samples = 128);

}  // namespace sigil::geometry::mesh::curve
