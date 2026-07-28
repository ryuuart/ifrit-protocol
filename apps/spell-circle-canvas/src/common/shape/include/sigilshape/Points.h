#pragma once

/** @file
 * SigilShape points — a Houdini-flavored miniature: a Cloud is
 * positions plus NAMED ATTRIBUTE LANES (scalars, vectors, colors),
 * generators put points places (a spline, a ring, a grid, a mesh
 * surface, a box), modifiers perturb them, and two consumers turn them
 * into pictures:
 *
 *  - instance()/panels(): stamp a Mesh (or a quad) onto every point —
 *    scale/tint/orientation read from lanes — producing ONE merged
 *    Mesh for space::drawMesh or world::World. "Instance planes
 *    across points" is panels() + a normal lane (or leave normals off
 *    and let billboarding face the camera at draw time).
 *  - drawBillboards(): the UI-particle path — camera-facing sprites
 *    (an SkImage, or a soft procedural dot) with perspective size,
 *    depth sort, per-point size/tint lanes, additive or normal blend.
 *
 * Everything is a value: clouds copy, lanes are plain vectors, and a
 * generator + modifier stack re-runs whenever a parameter moves — the
 * non-destructive posture of the rest of the library.
 *
 * Conventional lane names (nothing enforces them): "t" (0..1 along a
 * generator), "normal" (orientation), "size", "tint".
 */

#include "sigilshape/Curves.h"
#include "sigilshape/Mesh.h"
#include "sigilshape/Space.h"

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <glm/glm.hpp>

#include <map>
#include <string>
#include <vector>

namespace sigil::shape {

struct Cloud {
  std::vector<glm::vec3> positions;
  std::map<std::string, std::vector<float>, std::less<>> scalars;
  std::map<std::string, std::vector<glm::vec3>, std::less<>> vectors;
  std::map<std::string, std::vector<glm::vec4>, std::less<>> colors;

  size_t size() const { return positions.size(); }

  /** Lane accessors, create-on-touch, sized to the cloud. */
  std::vector<float> &scalar(const std::string &name, float fill = 0);
  std::vector<glm::vec3> &vector(const std::string &name,
                                 glm::vec3 fill = {0, 0, 1});
  std::vector<glm::vec4> &color(const std::string &name,
                                glm::vec4 fill = {1, 1, 1, 1});

  /** Read-only lane lookups; null when absent. */
  const std::vector<float> *scalarIf(std::string_view name) const;
  const std::vector<glm::vec3> *vectorIf(std::string_view name) const;
  const std::vector<glm::vec4> *colorIf(std::string_view name) const;

  /** Append another cloud. Shared lanes concatenate; a lane missing
   *  on one side pads by NAME convention: scalar "size" pads 1
   *  (others 0), color "Tex" pads the identity window {0,0,1,1} and
   *  "uv" pads {0,0,0,0} (other colors white), vectors pad {0,0,1}. */
  void append(const Cloud &other);
};

namespace points {

/** @p count points along the spline, arc-length spaced; writes "t"
 *  plus the full parallel-transport frame as "tangent", "normal", and
 *  "binormal" lanes — cook them into any orient lane you need. */
Cloud onSpline(const Spline3 &spline, int count,
               glm::vec3 up = {0, 1, 0});

/** nu x nv lattice spanned by two edge vectors; writes "t" (row-major
 *  0..1) and "normal" (du x dv). */
Cloud grid(glm::vec3 origin, glm::vec3 du, glm::vec3 dv, int nu,
           int nv);

/** A ring of @p count points; writes "t" and "normal" (outward). */
Cloud ring(glm::vec3 center, float radius, int count,
           glm::vec3 axis = {0, 1, 0});

/** Uniform random points in a box; writes "t" (by index). */
Cloud scatterBox(glm::vec3 lo, glm::vec3 hi, int count,
                 uint32_t seed = 1);

/** Area-weighted random points on a mesh surface; writes "normal"
 *  (interpolated) and "t". */
Cloud onMesh(const Mesh &mesh, int count, uint32_t seed = 1);

/** Seeded uniform jitter of every position, +-amplitude per axis. */
void jitter(Cloud &cloud, float amplitude, uint32_t seed = 7);

/** Smooth value-noise displacement (the organic drift). */
void displaceNoise(Cloud &cloud, float amplitude, float frequency,
                   uint32_t seed = 7);

// ---------------------------------------------------------------------------
// Consumers

struct InstanceOptions {
  float scale = 1;
  /** Scalar lane multiplied into scale per point (e.g. "size"). */
  std::string scaleLane;
  /** Color lane copied to the stamped vertices' tint (e.g. "tint"). */
  std::string tintLane;
  /** Vector lane orienting the stamp's +z (e.g. "normal"); empty =
   *  keep the stamp's own orientation. */
  std::string orientLane;
  glm::vec3 up = {0, 1, 0};
};

/** Stamp @p stamp at every point into one merged Mesh. */
Mesh instance(const Cloud &cloud, const Mesh &stamp,
              const InstanceOptions &options = {});

/** Stamp w x h quads — "instance planes across points". With an
 *  orient lane the planes stand in the world; without one they lie in
 *  xy facing +z (billboard-ready). */
Mesh panels(const Cloud &cloud, float width, float height,
            const InstanceOptions &options = {});

struct BillboardStyle {
  /** Sprite image; null draws a soft radial dot. */
  sk_sp<SkImage> sprite;
  float size = 10;          ///< world units at scale 1
  std::string sizeLane;     ///< scalar multiplier per point
  std::string tintLane;     ///< color per point
  glm::vec4 tint = {1, 1, 1, 1};
  bool additive = true;     ///< kPlus glow vs kSrcOver
  bool depthSort = true;
  /** Shrink with distance (perspective); off = constant pixel size. */
  bool perspective = true;
};

/** The UI-particle draw: project, sort, splat camera-facing sprites. */
void drawBillboards(SkCanvas &canvas, const Cloud &cloud,
                    const space::Camera &camera, SkSize viewport,
                    const BillboardStyle &style = {});

} // namespace points

} // namespace sigil::shape
