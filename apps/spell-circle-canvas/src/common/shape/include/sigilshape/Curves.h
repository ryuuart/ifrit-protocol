#pragma once

/** @file
 * SigilShape curves — splines that cross space, held as VALUES. A
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

#include "sigilshape/Mesh.h"
#include "sigilshape/Space.h"

#include <include/core/SkM44.h>
#include <include/core/SkPath.h>

#include <functional>
#include <vector>

namespace sigil::shape {

struct Spline3 {
  enum class Type : uint8_t { Linear, CatmullRom, Bezier };

  std::vector<SkV3> points;
  Type type = Type::CatmullRom;
  bool closed = false;

  /** Position at parameter t in [0,1] (uniform by segment). */
  SkV3 position(float t) const;
  /** Unit tangent at t (central difference). */
  SkV3 tangent(float t) const;
  /** Approximate length (chord sum over @p samples). */
  float length(int samples = 256) const;
  /** @p count positions, uniform in PARAMETER. */
  std::vector<SkV3> sample(int count) const;
  /** @p count positions, uniform in ARC LENGTH — even beads on the
   *  wire regardless of knot spacing. */
  std::vector<SkV3> sampleArcLength(int count) const;
};

/** An orthonormal moving frame on the curve. */
struct Frame3 {
  SkV3 position{0, 0, 0};
  SkV3 tangent{0, 0, 1};
  SkV3 normal{0, 1, 0};   // "up", parallel-transported
  SkV3 binormal{1, 0, 0}; // tangent x normal
  float t = 0;            // curve parameter
};

namespace curves {

/** @p count parallel-transport frames, arc-length spaced. The first
 *  normal starts nearest @p up and each subsequent frame rotates
 *  minimally — no Frenet flips at inflections. Closed splines get an
 *  even twist correction so the last frame meets the first. */
std::vector<Frame3> frames(const Spline3 &spline, int count,
                           SkV3 up = {0, 1, 0});

struct TubeOptions {
  float radius = 6;
  /** Radius profile over t (multiplies `radius`); null = constant. */
  std::function<float(float t)> profile;
  int segments = 96;  ///< rings along the curve
  int sides = 12;     ///< vertices around each ring
  bool caps = true;   ///< close open tube ends with fans
  SkV3 up = {0, 1, 0};
};

/** Sweep a circle along the spline. UVs: u around, v = t. */
Mesh tube(const Spline3 &spline, const TubeOptions &options = {});

struct RibbonOptions {
  float width = 24;
  std::function<float(float t)> profile;
  int segments = 96;
  SkV3 up = {0, 1, 0}; ///< the ribbon faces its frames' normal
};

/** Sweep a flat band along the spline (a 3D brush stroke). */
Mesh ribbon(const Spline3 &spline, const RibbonOptions &options = {});

/** The spline as a 2D path under @p camera — points behind the near
 *  plane split the path into separate contours. */
SkPath project(const Spline3 &spline, const space::Camera &camera,
               SkSize viewport, int samples = 128);

} // namespace curves

} // namespace sigil::shape
