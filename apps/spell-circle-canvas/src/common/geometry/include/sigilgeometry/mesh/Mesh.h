#pragma once

/** @file
 * @ingroup geometry-mesh
 *
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

#include <sigilcore/callable/Callable.h>

#include <boost/container/map.hpp>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <utility>
#include <vector>

/** THE 3D TIER. Its currency is one renderer-neutral triangle mesh, and
 *  everything here either makes one, reads one or draws one: the
 *  generators and the faces they are built from, the camera that looks
 *  at a mesh, the splines that sweep one, the point clouds and the
 *  operator chain over them, model import and export, and the painter
 *  that puts the result on an ordinary canvas.
 *
 *  3D data speaks glm; Skia's types appear only where geometry genuinely
 *  comes from or goes to Skia. Where the 2D tier's currency is an
 *  `SkPath` addressed by arc length, this one's is vertices and
 *  indices. */
namespace sigil::geometry::mesh {

/** Renderer-neutral triangle mesh. Indices are 32-bit; Skia's 16-bit
 *  SkVertices limit is handled by the draw (chunking), not by
 *  the data. */
struct Mesh {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;  ///< unit, same count as positions
  std::vector<glm::vec2> uvs;      ///< [0,1]^2, same count as positions
  /** Optional per-vertex tint (instancing writes it); empty = none.
   *  Both renderers multiply it into the shaded color when present. */
  std::vector<glm::vec4> colors;
  std::vector<uint32_t> indices;

  /** PRIMITIVE attribute lanes — the Houdini/TouchDesigner primitive class,
   *  the point lanes' sibling. A primitive here IS a TRIANGLE (one
   *  index triple), so every lane holds exactly triangleCount() float4
   *  values, addressed BY NAME exactly like Cloud's point lanes and
   *  pop's AttributeReference: no second identity system, just a second class.
   *
   *  Conventional names (nothing enforces them): "Color" (flat
   *  per-primitive tint — render::MeshStyle::primitiveColorLane reads it and
   *  mesh::bakePrimitiveColor bakes it for vertex-only renderers) and "Id"
   *  (.x = the piece the triangle belongs to; instancing writes the
   *  owning point's index, so "a stamp instance" is expressible as a
   *  lane VALUE rather than a new container). Any other name is a
   *  custom lane, create-on-first-touch. */
  boost::container::map<std::string, std::vector<glm::vec4>, std::less<>>
      primitives;

  size_t vertexCount() const { return positions.size(); }
  size_t triangleCount() const { return indices.size() / 3; }

  /** Primitive-lane accessor, create-on-touch, sized to
   *  triangleCount(). */
  std::vector<glm::vec4>& primitive(const std::string& name,
                                    glm::vec4 fill = {1, 1, 1, 1});
  /** Read-only primitive-lane lookup; null when absent. */
  const std::vector<glm::vec4>* primitiveIf(std::string_view name) const;
  /** WHICH LANES THE MESH CARRIES, in the map's own order — the reading
   *  that asks what is there rather than for one lane by name, so a
   *  caller can walk the lanes without holding the map's own type. */
  [[nodiscard]] std::vector<std::string> primitiveNames() const;

  /** Append another mesh (indices re-based). Primitive lanes
   *  concatenate; a lane missing on one side pads by NAME convention
   *  ("Color" pads white, everything else zeros) — the same posture
   *  Cloud::append takes for point lanes.
   *
   *  Every optional lane comes out sized to the merge: colors, normals
   *  and uvs to positions.size(), primitives to triangleCount(). That holds
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
 *  (u,v) parameters.
 *
 *  WHAT THE FORMULA ANSWERS DECIDES WHERE THE NORMALS COME FROM. A formula
 *  answering the POSITION is differenced: the sheet's two partial
 *  derivatives are taken numerically and crossed, which is what a sheet
 *  with no normal of its own needs and costs four extra evaluations per
 *  vertex. A formula answering the position AND ITS NORMAL, in that order,
 *  is taken at its word — one evaluation per vertex, and an exact normal
 *  at a pole, where a difference has no direction to normalize. Either way
 *  the normals come back unit length, and a normal with no length at all
 *  borrows the nearest one that has. */
Mesh grid(int nu, int nv,
          const core::Callable<glm::vec3(float u, float v)>& fn);
/** The same sheet from a formula answering the position and its normal
 *  together, which is taken at its word rather than differenced. */
Mesh grid(int nu, int nv,
          const core::Callable<std::pair<glm::vec3, glm::vec3>(float u,
                                                               float v)>& fn);

/** Flat quad panel in the xy plane facing +z, centered at origin. */
Mesh quad(float width, float height);

/** The primitive layer's PORTABLE consumer: bake a primitive lane into
 *  per-vertex colors by unsharing vertices (three per triangle), so a
 *  renderer that speaks only vertex attributes — any GPU vertex buffer,
 *  and every pipeline built over one — shows flat per-primitive colour
 *  with no shader change. Existing vertex colors multiply through; a
 *  missing or mis-sized lane returns the mesh unchanged. Primitive
 *  lanes survive on the result (triangle order is preserved). */
Mesh bakePrimitiveColor(const Mesh& mesh, std::string_view lane = "Color");

}  // namespace sigil::geometry::mesh
