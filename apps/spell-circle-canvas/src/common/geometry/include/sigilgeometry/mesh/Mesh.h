#pragma once

/** @file
 * @ingroup geometry-mesh
 *
 * The 3D tier's currency — one Mesh whichever renderer draws it. The
 * vertex and index buffers upload to a GPU renderer downstream:
 * positions, normals, uvs, indices, nothing renderer-shaped.
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
#include <concepts>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <optional>
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
 *  3D data speaks glm; the renderer's types appear only where geometry
 *  genuinely comes from or goes to the renderer. Where the 2D tier's
 *  currency is an outline addressed by arc length, this one's is
 *  vertices and indices. */
namespace sigil::geometry::mesh {

/** Renderer-neutral triangle mesh. Indices are 32-bit; the renderer's
 *  16-bit vertex-batch limit is handled by the draw (chunking), not by
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
   *  per-primitive tint — a renderer that reads a primitive colour lane
   *  reads it, and mesh::bakePrimitiveColor bakes it for one that reads
   *  vertex colours alone) and "Id"
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
   *  the whole mesh, so
   *  an undersized merge would turn lighting, texturing or tinting off
   *  for BOTH halves. Pads: colors white, normals +Z, uvs (0, 0), and
   *  tangents none, so a side that carried none reads its own off the
   *  uvs. */
  void append(const Mesh& other);
  /** Transform positions by @p matrix, normals by its inverse transpose and
   *  tangents by its own turn, their handedness flipped by a mirror. */
  void transform(const glm::mat4& matrix);
  /** Recompute vertex normals as area-weighted triangle-normal sums. */
  void computeNormals();
  /** Fill the tangent lane from the uvs: at each vertex, the direction u
   *  grows along the surface, summed over the triangles meeting there and
   *  squared to the vertex's normal, with w saying which way v grows
   *  beside it. A mesh whose uvs or normals are not sized to its positions
   *  comes back carrying none, and so does a vertex no triangle gives a
   *  direction of u. */
  void computeTangents();
  /** Axis-aligned bounds. */
  void bounds(glm::vec3* lo, glm::vec3* hi) const;

  /** THE TANGENT LANE, optional: per vertex, in xyz the unit direction u
   *  runs along the surface, and in w which way v runs beside it — the
   *  direction v grows along is cross(normal, xyz) times w, one or minus
   *  one. Every stock generator fills it — `grid()` from how its sheet is
   *  parameterised, the rest through `computeTangents()` — and every
   *  operation that forms vertices anew carries it. Empty, or any count
   *  other than positions, is a mesh that carries none, and a renderer
   *  reads the direction off the uvs where it draws. A vertex whose xyz
   *  has no length carries none either. */
  std::vector<glm::vec4> tangents;

  /** Content equality, lane for lane. */
  bool operator==(const Mesh&) const = default;
};

/** A SOLID SPELLED AS A VALUE: compared by the numbers that decide it,
 *  and asked for its mesh with `mesh()`. A holder that keeps descriptions
 *  compares two such values instead of two meshes, and forms the mesh
 *  once while the value stands unchanged. The stock solids are the kit's
 *  `solids::` values; a consumer's own value meeting this is their peer. */
template <typename Value>
concept Solid = std::equality_comparable<Value> && requires(const Value& v) {
  { v.mesh() } -> std::same_as<Mesh>;
};

/** A rectangle laid flat in a sheet's own space that a texture covers:
 *  its top-left corner sits at `origin`, its width runs `size.x` along
 *  `across` and its height `size.y` along `down`. A vertex's texture
 *  coordinates are where it projects into that rectangle, (0, 0) at the
 *  top-left and (1, 1) at the bottom-right, so a sheet that is not the
 *  whole rectangle — a blanket with a corner cut away — keeps the page
 *  laid over it where the page would lie. Both directions are unit
 *  length; a vertex off the rectangle's plane projects onto it. */
struct TextureRectangle {
  glm::vec3 origin{0, 0, 0};  ///< where the texture's top-left lands
  glm::vec3 across{1, 0, 0};  ///< unit direction of the texture's width
  glm::vec3 down{0, -1, 0};   ///< unit direction of the texture's height
  glm::vec2 size{1, 1};       ///< width and height, in the sheet's units
};

/** How `grid()` places a texture over its sheet. With nothing stated the
 *  texture coordinates are the (u, v) parameters, image convention: u
 *  across, v from the sheet's v = 1 edge at the top. */
struct GridOptions {
  /** The texture coordinates as a formula of each vertex's position;
   *  stated, it wins over `textureRectangle`. */
  std::function<glm::vec2(glm::vec3 position)> textureFromPosition;
  /** The rectangle of the sheet's space the texture covers. */
  std::optional<TextureRectangle> textureRectangle;
};

/** Evaluate a parametric sheet on an columns x rows vertex grid. UVs are the
 *  (u,v) parameters unless @p options places the texture another way.
 *
 *  WHAT THE FORMULA ANSWERS DECIDES WHERE THE NORMALS COME FROM. A formula
 *  answering the POSITION is differenced: the sheet's two partial
 *  derivatives are taken numerically and crossed, which is what a sheet
 *  with no normal of its own needs and costs four extra evaluations per
 *  vertex. A formula answering the position AND ITS NORMAL, in that order,
 *  is taken at its word — one evaluation per vertex, and an exact normal
 *  at a pole, where a difference has no direction to normalize. Either way
 *  the normals come back unit length. A pole — a vertex with no normal of
 *  its own or, for a differenced formula, one standing on its row's or
 *  column's neighbour — takes the sheet's limit there: the area-weighted
 *  sum of the triangles meeting the point it collapses onto, which at a
 *  revolved apex is the axis. */
Mesh grid(int columns, int rows,
          const core::Callable<glm::vec3(float u, float v)>& fn,
          const GridOptions& options = {});
/** The same sheet from a formula answering the position and its normal
 *  together, which is taken at its word rather than differenced. */
Mesh grid(
    int columns, int rows,
    const core::Callable<std::pair<glm::vec3, glm::vec3>(float u, float v)>& fn,
    const GridOptions& options = {});

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
