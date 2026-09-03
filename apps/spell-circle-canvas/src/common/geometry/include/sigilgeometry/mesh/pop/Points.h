#pragma once

/** @file
 * SigilGeometry points — a Houdini-flavored miniature: a Cloud is
 * positions plus NAMED ATTRIBUTE LANES (scalars, vectors, colors),
 * generators put points places (a spline, a ring, a grid, a mesh
 * surface, a box), modifiers perturb them, and two consumers turn them
 * into pictures:
 *
 *  - instance()/quads(): stamp a Mesh (or a quad) onto every point —
 *    scale/tint/orientation read from lanes — producing ONE merged
 *    Mesh for render::drawMesh or world::World. "Instance planes
 *    across points" is quads() + a normal lane (or leave normals off
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

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/pop/Stamp.h"

namespace sigil::geometry::mesh {

/** Points plus named attribute lanes, all parallel to `positions`.
 *  A lane is one value per point under a name the operators agree on
 *  — "size", "tint", "normal" — which is what lets a generator, a
 *  modifier and a consumer meet without a fixed vertex format. Lanes
 *  come in three widths (scalar, vector, color) and are created on
 *  first touch, sized to the cloud and filled with a default. */
struct Cloud {
  std::vector<glm::vec3> positions;
  std::map<std::string, std::vector<float>, std::less<>> scalars;
  std::map<std::string, std::vector<glm::vec3>, std::less<>> vectors;
  std::map<std::string, std::vector<glm::vec4>, std::less<>> colors;

  size_t size() const { return positions.size(); }

  /** Lane accessors, create-on-touch, sized to the cloud. */
  std::vector<float>& scalar(const std::string& name, float fill = 0);
  std::vector<glm::vec3>& vector(const std::string& name,
                                 glm::vec3 fill = {0, 0, 1});
  std::vector<glm::vec4>& color(const std::string& name,
                                glm::vec4 fill = {1, 1, 1, 1});

  /** Read-only lane lookups; null when absent. */
  const std::vector<float>* scalarIf(std::string_view name) const;
  const std::vector<glm::vec3>* vectorIf(std::string_view name) const;
  const std::vector<glm::vec4>* colorIf(std::string_view name) const;

  /** Append another cloud. Shared lanes concatenate; a lane missing
   *  on one side pads by NAME convention: scalar "size" pads 1
   *  (others 0), color "Tex" pads the identity window {0,0,1,1} and
   *  "uv" pads {0,0,0,0} (other colors white), vectors pad {0,0,1}. */
  void append(const Cloud& other);

  /** Content equality, lane for lane. */
  bool operator==(const Cloud&) const = default;
};

namespace points {

/** @p count points along the spline, arc-length spaced; writes "t"
 *  plus the full parallel-transport frame as "tangent", "normal", and
 *  "binormal" lanes — cook them into any orient lane you need. */
Cloud onSpline(const curve::Spline3& spline, int count,
               glm::vec3 up = {0, 1, 0});

/** nu x nv lattice spanned by two edge vectors; writes "t" (row-major
 *  0..1) and "normal" (du x dv). */
Cloud grid(glm::vec3 origin, glm::vec3 du, glm::vec3 dv, int nu, int nv);

/** A ring of @p count points; writes "t" and "normal" (outward). */
Cloud ring(glm::vec3 center, float radius, int count,
           glm::vec3 axis = {0, 1, 0});

/** Uniform random points in a box; writes "t" (by index). */
Cloud scatterBox(glm::vec3 lo, glm::vec3 hi, int count, uint32_t seed = 1);

/** Area-weighted random points on a mesh surface; writes "normal"
 *  (interpolated) and "t". */
Cloud onMesh(const Mesh& mesh, int count, uint32_t seed = 1);

/** Seeded uniform jitter of every position, +-amplitude per axis — the
 *  `pop::Jitter` operator reached for without a chain, and the same
 *  offsets: it runs that operator's own kernel over the positions, so
 *  the two spellings of the verb cannot answer differently. */
void jitter(Cloud& cloud, float amplitude, uint32_t seed = 7);

/** Smooth sin-field displacement (the organic drift) — the `pop::Noise`
 *  operator reached for without a chain, reading the same field through
 *  `pop::noiseField`. */
void displaceNoise(Cloud& cloud, float amplitude, float frequency,
                   uint32_t seed = 7);

// ---------------------------------------------------------------------------
// Consumers

/** How `instance()` stamps its mesh at every point. Each lane name is
 *  optional: when empty, that property is uniform across the cloud
 *  rather than read per point. */
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
  /** Who forms the stamped vertices. The default is the built-in host
   *  executor; assigning another one is the whole of switching runtimes,
   *  and the indices and the lanes the result carries are the same
   *  either way. */
  StampRuntime runtime = StampRuntime::cpu();
};

/** HOW A STAMP RIDES A CLOUD'S CONVENTIONAL LANES, as one table.
 *
 *  The orient lane is "dir" where a chain produced one and "normal"
 *  where a generator or an importer did, so a cloud from either source
 *  stands its stamps up without the author naming a lane; "size" scales
 *  and "tint" colours. A lane the cloud does not carry is left empty
 *  rather than named, so nothing is looked for that is not there.
 *
 *  Every stamping path takes its options from here. Two tables would
 *  mean one cloud standing its stamps up through one caller and lying
 *  them flat through another, which is what a single convention is for.
 */
InstanceOptions stampOptions(const Cloud& cloud);

/** @p cloud stamped with @p stamp under @p options, as a dispatch.
 *  False — leaving @p out untouched — when there is nothing to stamp:
 *  no points, or a stamp with no vertices. A lane the cloud or the stamp
 *  does not carry is filled here with what it would have been read as,
 *  once, rather than asked about per vertex. */
bool describe(const Cloud& cloud, const Mesh& stamp,
              const InstanceOptions& options, kernel::Dispatch* out);

/** Stamp @p stamp at every point into one merged Mesh — dir orients,
 *  size scales, tint colours, and the cloud's "Tex" window remaps each
 *  stamped vertex's uv. The vertices are formed on `options.runtime`;
 *  every executor writes exactly these vertices from the same cloud. */
Mesh instance(const Cloud& cloud, const Mesh& stamp,
              const InstanceOptions& options = {});

/** Stamp w x h quads — "instance planes across points". With an
 *  orient lane the planes stand in the world; without one they lie in
 *  xy facing +z (billboard-ready). */
Mesh quads(const Cloud& cloud, float width, float height,
           const InstanceOptions& options = {});

/** The point class -> PRIMITIVE class bridge (Houdini's Attribute
 *  Promote), the instancing companion: an instanced @p mesh lays each
 *  point's stamp down as a consecutive run of triangles, so triangle
 *  index / (triangles per stamp) IS the owning point. Fills
 *  Mesh::prims[@p primLane] from the cloud lane @p cloudLane —
 *  scalars broadcast to all four components, vectors take w = 0,
 *  colors copy — and the RESERVED source name "Id" writes the owning
 *  point's index in .x instead of reading a lane.
 *
 *  No-op unless the mesh's triangle count divides evenly by the
 *  cloud's point count (i.e. it really is @p cloud instanced). */
void promoteToPrims(Mesh& mesh, const Cloud& cloud, std::string_view cloudLane,
                    const std::string& primLane);

/** How `drawBillboards()` splats its sprites — the image and its size,
 *  the lanes that vary size and tint per point, and whether the
 *  splats glow additively, sort back-to-front, and shrink with
 *  distance. */
struct BillboardStyle {
  /** Sprite image; null draws a soft radial dot. */
  sk_sp<SkImage> sprite;
  float size = 10;       ///< world units at scale 1
  std::string sizeLane;  ///< scalar multiplier per point
  std::string tintLane;  ///< color per point
  /** THE ATLAS WINDOW LANE: a colour lane holding {uOffset, vOffset,
   *  uScale, vScale} per point, in the unit square — which is exactly
   *  what a `pop::Atlas` op writes into "Tex". Each splat then draws
   *  THAT CELL of the sprite instead of the whole image, so one sheet of
   *  sprites splats as a field of different ones and a cloud carries
   *  which is which.
   *
   *  Named rather than assumed, because a cloud may carry "Tex" for the
   *  stamping path while these splats are meant to be one sprite; say
   *  `"Tex"` to read what the atlas op wrote. A point whose window is
   *  degenerate, or which the lane does not reach, takes the whole
   *  image. */
  std::string texLane;
  glm::vec4 tint = {1, 1, 1, 1};
  bool additive = true;  ///< kPlus glow vs kSrcOver
  bool depthSort = true;
  /** Shrink with distance (perspective); off = constant pixel size. */
  bool perspective = true;
};

/** The UI-particle draw: project, sort, splat camera-facing sprites. */
void drawBillboards(SkCanvas& canvas, const Cloud& cloud,
                    const camera::Camera& camera, SkSize viewport,
                    const BillboardStyle& style = {});

}  // namespace points

}  // namespace sigil::geometry::mesh
