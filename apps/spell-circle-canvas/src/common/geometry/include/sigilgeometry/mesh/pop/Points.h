#pragma once

/** @file
 * @ingroup geometry-mesh
 *
 * SigilGeometry points — a Houdini-flavored miniature: a Cloud is
 * positions plus NAMED ATTRIBUTE LANES (scalars, vectors, colors),
 * generators put points places, modifiers perturb them, and two
 * consumers turn them into pictures — instance()/quads(), which stamp a
 * Mesh onto every point and merge the result, and drawBillboards(), the
 * UI-particle path of camera-facing sprites.
 *
 * Everything is a value: clouds copy, lanes are plain vectors, and a
 * generator + modifier stack re-runs whenever a parameter moves.
 * Conventional lane names, with nothing enforcing them: "t", "normal",
 * "size", "tint".
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <boost/container/map.hpp>
#include <glm/glm.hpp>
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
  boost::container::map<std::string, std::vector<float>, std::less<>> scalars;
  boost::container::map<std::string, std::vector<glm::vec3>, std::less<>>
      vectors;
  boost::container::map<std::string, std::vector<glm::vec4>, std::less<>>
      colors;

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

  /** WHICH LANES OF EACH WIDTH THE CLOUD CARRIES, in the maps' own
   *  order — the reading that asks what is there rather than for one
   *  lane by name, so a caller can walk the lanes without holding the
   *  maps' own type. */
  [[nodiscard]] std::vector<std::string> scalarNames() const;
  [[nodiscard]] std::vector<std::string> vectorNames() const;
  [[nodiscard]] std::vector<std::string> colorNames() const;

  /** Append another cloud. Shared lanes concatenate; a lane missing
   *  on one side pads by NAME convention: scalar "size" pads 1
   *  (others 0), color "Tex" pads the identity window {0,0,1,1} and
   *  "uv" pads {0,0,0,0} (other colors white), vectors pad {0,0,1}. */
  void append(const Cloud& other);

  /** Content equality, lane for lane. */
  bool operator==(const Cloud&) const = default;
};

/** THE GENERATORS THAT PUT POINTS PLACES: along a spline, around a
 *  ring, over a grid, across a mesh surface, through a box. Each answers
 *  a cloud already carrying the lanes its own shape implies — an arc
 *  length, a surface normal, a row-major position — so a stamp or a
 *  cook downstream has something to read without a second pass. */
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
 *  `pop::noiseField`. The seed is a FLOAT, as the operator's is: the
 *  field reads it as one, and a modifier that could not reach a
 *  fractional seed would be a narrower verb than the operator it is the
 *  same verb as. */
void displaceNoise(Cloud& cloud, float amplitude, float frequency,
                   float seed = 7);

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

  /** Value equality: the lane names, the uniform scale, the up axis and
   *  the runtime. Two default option sets are equal, which is what lets
   *  a consumer prove two frames asked for the same stamping. */
  bool operator==(const InstanceOptions&) const = default;
};

/** HOW A STAMP RIDES A CLOUD'S CONVENTIONAL LANES, as one table: the
 *  orient lane is "dir" where a chain produced one and "normal" where a
 *  generator or an importer did, "size" scales and "tint" colours. A
 *  lane @p cloud does not carry is left EMPTY rather than named, so
 *  nothing is looked for that is not there. Every stamping path takes
 *  its options from here, so one cloud stamps the same way whichever
 *  caller stamps it. */
InstanceOptions stampOptions(const Cloud& cloud);

/** @p cloud stamped with @p stamp under @p options, as a dispatch.
 *  False — leaving @p out untouched — when there is nothing to stamp:
 *  no points, or a stamp with no vertices. A lane the cloud or the stamp
 *  does not carry is filled here with what it would have been read as,
 *  once, rather than asked about per vertex. */
bool describe(const Cloud& cloud, const Mesh& stamp,
              const InstanceOptions& options, kernel::StampDispatch* out);

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

/** THE POINT CLASS TO PRIMITIVE CLASS BRIDGE, the instancing companion:
 *  an instanced @p mesh lays each point's stamp down as a consecutive
 *  run of triangles, so triangle index / (triangles per stamp) IS the
 *  owning point. Scalars broadcast to all four components, vectors take
 *  w = 0, colors copy, and the RESERVED @p cloudLane "Id" writes the
 *  owning point's index in .x instead of reading a lane.
 *  @silent the mesh's triangle count does not divide evenly by the
 *  cloud's point count, so it is not @p cloud instanced. */
void promoteToPrimitives(Mesh& mesh, const Cloud& cloud,
                         std::string_view cloudLane,
                         const std::string& primitiveLane);

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
   *  uScale, vScale} per point, in the unit square, which is what a
   *  `pop::AtlasCell` operation writes into "Tex" — so one sheet splats
   *  as a field of different sprites. Named rather than assumed, since
   *  a cloud may carry "Tex" for the stamping path. A point whose
   *  window is degenerate, or which the lane does not reach, takes the
   *  whole image.
   *  @trap THE SHEET WANTS A GUTTER: a cell is taken half a texel
   *  inside its window, and a cell narrower than that inset is not
   *  drawn at all. */
  std::string textureLane;
  glm::vec4 tint = {1, 1, 1, 1};
  bool additive = true;  ///< kPlus glow vs kSrcOver
  bool depthSort = true;
  /** Shrink with distance (perspective); off = constant pixel size. */
  bool perspective = true;

  /** Value equality, dial for dial. The sprite compares by identity, as
   *  `sk_sp` does. */
  bool operator==(const BillboardStyle&) const = default;
};

/** The UI-particle draw: project, sort, splat camera-facing sprites. */
void drawBillboards(SkCanvas& canvas, const Cloud& cloud,
                    const camera::Camera& camera, SkSize viewport,
                    const BillboardStyle& style = {});

}  // namespace points

}  // namespace sigil::geometry::mesh
