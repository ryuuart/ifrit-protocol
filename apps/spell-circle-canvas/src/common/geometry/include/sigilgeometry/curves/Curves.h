#pragma once

/** @file
 * SigilGeometry curves — splines that cross space, held as VALUES. A
 * Spline3 is control points + a type + closure: edit any of them and
 * re-evaluate; nothing downstream is baked until asked. Three
 * consumers, all regenerable from the same spline:
 *
 *  - sample()/frames(): positions and parallel-transport frames
 *    (tangent/normal/binormal that never flip on the way around a
 *    knot) — the spine for placement, cameras, and point clouds;
 *  - tube()/ribbon(): swept Mesh geometry, radius optionally a
 *    profile function of t — 3D strokes for Space and SigilWorld;
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
#include "sigilgeometry/space/Space.h"

namespace sigil::geometry {

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

namespace curves {

/** @p count parallel-transport frames, arc-length spaced. The first
 *  normal starts nearest @p up and each subsequent frame rotates
 *  minimally — no Frenet flips at inflections. Closed splines get an
 *  even twist correction so the last frame meets the first. */
std::vector<Frame3> frames(const Spline3& spline, int count,
                           glm::vec3 up = {0, 1, 0});

/** How `tube()` sweeps its circle: the cross-section radius and its
 *  profile over the curve, the tessellation along and around, and
 *  whether open ends are closed off. */
struct TubeOptions {
  float radius = 6;
  /** Radius profile over t (multiplies `radius`); null = constant. */
  std::function<float(float t)> profile;
  int segments = 96;  ///< rings along the curve
  int sides = 12;     ///< vertices around each ring
  bool caps = true;   ///< close open tube ends with fans
  glm::vec3 up = {0, 1, 0};
};

/** Sweep a circle along the spline. UVs: u around, v = t. */
Mesh tube(const Spline3& spline, const TubeOptions& options = {});

/** How `ribbon()` sweeps its band. Unlike a tube the band has a
 *  facing, so `up` seeds the frames that decide which way it turns. */
struct RibbonOptions {
  float width = 24;
  std::function<float(float t)> profile;
  int segments = 96;
  glm::vec3 up = {0, 1, 0};  ///< the ribbon faces its frames' normal
};

/** Sweep a flat band along the spline (a 3D brush stroke). */
Mesh ribbon(const Spline3& spline, const RibbonOptions& options = {});

/** How `banner()` cuts its cloth. The band covers only a window of the
 *  closed spline: `head` is the leading edge in loop parameter and
 *  `span` the length trailing it, so advancing `head` alone tows the
 *  banner around the curve. */
struct BannerOptions {
  float width = 24;
  float head = 1;  ///< window end, in loop parameter
  float span = 1;  ///< window length back from head
  int sections = 160;
};
/** A gravity-rigged band over a window of a CLOSED spline — the
 *  towed-banner rig: the cloth's width hangs world-vertical off the
 *  tangent (hysteresis through vertical stretches), so it never rolls
 *  upside-down on a ball winding the way parallel-transport frames
 *  do. u = 0 is the TOP edge, v runs tail -> head over the window.
 *  The CPU twin of SigilWorld's GPU sweep kernel. */
Mesh banner(const Spline3& spline, const BannerOptions& options = {});

/** The spline as a 2D path under @p camera — points behind the near
 *  plane split the path into separate contours. */
SkPath project(const Spline3& spline, const space::Camera& camera,
               SkSize viewport, int samples = 128);

}  // namespace curves

}  // namespace sigil::geometry
