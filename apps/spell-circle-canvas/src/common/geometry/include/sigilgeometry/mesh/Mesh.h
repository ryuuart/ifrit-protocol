#pragma once

/** @file
 * The 3D tier's currency — one Mesh whichever runtime draws it. The same
 * vertex and index buffers feed the draw in mesh/render and upload to a
 * GPU renderer downstream: positions, normals, uvs, indices, nothing
 * renderer-shaped.
 *
 * 3D data speaks glm (vec3/vec4/mat4); Skia types appear only where
 * geometry genuinely comes from or goes to Skia (the canvas
 * downstream).
 *
 * grid() is the seam every surface is built through: it evaluates any
 * parametric sheet on a vertex grid, with UVs always present so panel
 * textures land without ceremony. The stock surfaces built on it, and
 * the two lifts from another currency, are the kit's — see
 * <sigilgeometry/kit/Solids.h>.
 */

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

/** Evaluate a parametric sheet on an nu x nv vertex grid. UVs are the
 *  (u,v) parameters. Normals from the analytic cross of numeric partial
 *  derivatives. */
Mesh grid(int nu, int nv, const std::function<glm::vec3(float u, float v)>& fn);

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
