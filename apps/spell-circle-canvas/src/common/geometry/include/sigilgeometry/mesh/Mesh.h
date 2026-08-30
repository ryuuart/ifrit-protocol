#pragma once

/** @file
 * The 3D tier's currency — one Mesh whichever runtime draws it. The same
 * vertex and index buffers feed the draw in mesh/render and upload to a
 * GPU renderer downstream: positions, normals, uvs, indices, nothing
 * renderer-shaped.
 *
 * 3D data speaks glm (vec3/vec4/mat4); Skia types appear only where
 * geometry genuinely comes from or goes to Skia (extrude's SkPath
 * outline here, the canvas downstream).
 *
 * Generators cover the diegetic-surface needs: extrude() lifts any
 * SkPath into a solid (caps earcut-triangulated with holes intact,
 * walls from the flattened contours), revolve() lathes a profile,
 * grid() evaluates any parametric sheet, and the named surfaces
 * (torus, superellipsoid, cylinderPanel) are grid() presets. UVs are
 * always present so panel textures land without ceremony.
 */

#include <include/core/SkPath.h>

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

namespace sigil::geometry::mesh {

/** Renderer-neutral triangle mesh. Indices are 32-bit; Skia's 16-bit
 *  SkVertices limit is handled by the draw (chunking), not by
 *  the data. */
struct Mesh {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;  // unit, same count as positions
  std::vector<glm::vec2> uvs;      // [0,1]^2, same count as positions
  /** Optional per-vertex tint (instancing writes it); empty = none.
   *  Both renderers multiply it into the shaded color when present. */
  std::vector<glm::vec4> colors;
  std::vector<uint32_t> indices;

  /** PRIMITIVE attribute lanes — the Houdini/TouchDesigner prim class,
   *  the point lanes' sibling. A primitive here IS a TRIANGLE (one
   *  index triple), so every lane holds exactly triangleCount() float4
   *  values, addressed BY NAME exactly like Cloud's point lanes and
   *  pop's AttrRef: no second identity system, just a second class.
   *
   *  Conventional names (nothing enforces them): "Color" (flat
   *  per-primitive tint — render::MeshStyle::primColorLane reads it and
   *  mesh::bakePrimColor bakes it for vertex-only renderers) and "Id"
   *  (.x = the piece the triangle belongs to; instancing writes the
   *  owning point's index, so "a stamp instance" is expressible as a
   *  lane VALUE rather than a new container). Any other name is a
   *  custom lane, create-on-first-touch. */
  std::map<std::string, std::vector<glm::vec4>, std::less<>> prims;

  size_t vertexCount() const { return positions.size(); }
  size_t triangleCount() const { return indices.size() / 3; }

  /** Primitive-lane accessor, create-on-touch, sized to
   *  triangleCount(). */
  std::vector<glm::vec4>& prim(const std::string& name,
                               glm::vec4 fill = {1, 1, 1, 1});
  /** Read-only primitive-lane lookup; null when absent. */
  const std::vector<glm::vec4>* primIf(std::string_view name) const;

  /** Append another mesh (indices re-based). Primitive lanes
   *  concatenate; a lane missing on one side pads by NAME convention
   *  ("Color" pads white, everything else zeros) — the same posture
   *  Cloud::append takes for point lanes.
   *
   *  Every optional lane comes out sized to the merge: colors, normals
   *  and uvs to positions.size(), prims to triangleCount(). That holds
   *  whether a side lacks the lane entirely or carries a SHORT one —
   *  consumers read "lane sized to positions" as the presence bit for
   *  the whole mesh (render::drawMesh's hasNormals is exactly that), so
   *  an undersized merge would turn lighting, texturing or tinting off
   *  for BOTH halves. Pads: colors white, normals +Z, uvs (0, 0). */
  void append(const Mesh& other);
  /** Transform positions by @p m and normals by its inverse transpose. */
  void transform(const glm::mat4& m);
  /** Recompute vertex normals as area-weighted triangle-normal sums. */
  void computeNormals();
  /** Axis-aligned bounds. */
  void bounds(glm::vec3* lo, glm::vec3* hi) const;

  /** Content equality, lane for lane. */
  bool operator==(const Mesh&) const = default;
};

/** How `extrude()` thickens a path: the total depth and which of the
 *  three surfaces — front cap, back cap, swept walls — to emit.
 *  Dropping caps leaves an open shell, which is what a wall-only
 *  extrusion is for. */
struct ExtrudeOptions {
  float depth = 24;         ///< total thickness, centered on z = 0
  float tolerance = 0.25f;  ///< curve flattening tolerance for walls/caps
  bool frontCap = true;
  bool backCap = true;
  bool walls = true;
};

/** Lift a filled path into a solid: front/back caps triangulated with
 *  hole support (even-odd containment decides outer vs hole rings),
 *  walls swept between them. Cap UVs are the path's unit bounds; wall
 *  UVs run u = contour arc length, v = depth. */
Mesh extrude(const SkPath& path, const ExtrudeOptions& options = {});

/** How `revolve()` lathes a profile: the number of steps around the
 *  axis and how far around to go. A partial sweep leaves the surface
 *  open at the seam; a full one duplicates the seam ring so the u
 *  coordinate can reach 1 instead of wrapping to 0. */
struct RevolveOptions {
  int segments = 48;     ///< steps around the axis
  float sweepDeg = 360;  ///< partial sweeps leave an open seam
  bool close = true;     ///< duplicate seam ring for clean UV wrap
};

/** Lathe a profile polyline around the +y axis: each profile point is
 *  (radius, height). UVs: u around the sweep, v along the profile. */
Mesh revolve(const std::vector<glm::vec2>& profile,
             const RevolveOptions& options = {});

/** Evaluate a parametric sheet on an nu x nv vertex grid. UVs are the
 *  (u,v) parameters. Normals from the analytic cross of numeric partial
 *  derivatives. */
Mesh grid(int nu, int nv, const std::function<glm::vec3(float u, float v)>& fn);

/** Torus around +y: major radius R in xz, tube radius r. */
Mesh torus(float R, float r, int nu = 64, int nv = 32);

/** Superellipsoid (exponent 2 = sphere, higher = rounded box). */
Mesh superellipsoid(glm::vec3 radii, float exponent, int nu = 48, int nv = 32);

/** A width x height panel curved around a vertical cylinder of
 *  @p radius (0 or infinite radius = flat), facing +z, centered at the
 *  origin. The natural diegetic-UI surface. */
Mesh cylinderPanel(float width, float height, float radius, int nu = 32,
                   int nv = 8);

/** Flat quad panel in the xy plane facing +z, centered at origin. */
Mesh quad(float width, float height);

/** The primitive layer's PORTABLE consumer: bake a primitive lane into
 *  per-vertex colors by unsharing vertices (three per triangle), so a
 *  renderer that speaks only vertex attributes — SigilWorld's Diligent
 *  pipelines, any GPU vertex buffer — shows flat per-primitive colour
 *  with no shader change. Existing vertex colors multiply through; a
 *  missing or mis-sized lane returns the mesh unchanged. Primitive
 *  lanes survive on the result (triangle order is preserved). */
Mesh bakePrimColor(const Mesh& mesh, std::string_view lane = "Color");

}  // namespace sigil::geometry::mesh
