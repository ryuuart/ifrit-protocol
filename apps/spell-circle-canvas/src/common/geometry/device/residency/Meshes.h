#pragma once

/** @file
 * MESHES RESIDENT ON THE DEVICE: the buffers a mesh becomes, held under
 * the number the caller gave the artefact it came from, and the one pair
 * of buffers a mesh nobody can name is streamed through.
 *
 * A mesh is host memory and a draw needs device buffers, so something
 * has to make the crossing and remember that it did. It stands here, in
 * the device feature, because the crossing is the device's — one vertex
 * layout, one lifetime rule, one cache — and every executor that draws a
 * mesh on this device makes the same one. Nothing about a pass, a
 * material or a frame's targets is here.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/InputLayout.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <boost/container/map.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Diligent {
class DynamicBuffer;
}  // namespace Diligent

namespace sigil::geometry::device {

/** ONE VERTEX, as every pipeline over these buffers reads it. A mesh
 *  that carries no normals, uvs or tint is filled in on upload rather
 *  than drawn against a second layout — which is what makes the vertex
 *  layout a constant of this device instead of an axis a program varies
 *  on. */
struct MeshVertex {
  float position[3];
  float normal[3];
  float uv[2];
  /** The vertex's own colour, times the colour of the triangle it
   *  belongs to where a primitive colour lane was named: a triangle has
   *  nowhere of its own to hold a value, so its three vertices each carry
   *  it multiplied in. */
  float color[4];
  /** The way u runs along the surface and the side v runs on, as the
   *  mesh's tangent lane carries it; zeros where it carries none, which a
   *  program reads as a direction to find off the uvs itself. */
  float tangent[4];
};

/** ONE INSTANCE, as an instanced draw reads it from a second buffer that
 *  steps once per instance: where the copy stands, how it is turned (a
 *  quaternion, x y z w), how it is scaled, the colour it multiplies the
 *  stamp's by, the texture window its uv is remapped through, and its
 *  four named scalars in the order the instanced geometry names them. */
struct InstanceVertex {
  float position[4];
  float orientation[4];
  float scale[4];
  float color[4];
  float textureWindow[4];
  float attributes[4];
};

/** The input number the first instance element is read at, right after
 *  the four vertex elements; the five after it follow in
 *  `InstanceVertex`'s order. */
inline constexpr unsigned kFirstInstanceInput = 4;

/** THE LAYOUT A PIPELINE DECLARES over these buffers, in the order the
 *  elements are numbered: position, normal, uv and colour always; with
 *  @p instanced the six `InstanceVertex` elements from
 *  `kFirstInstanceInput`, read from the second buffer once per instance;
 *  with @p withTangent the tangent at `kTangentInput`. One form per
 *  choice of the two. Every vertex carries a tangent either way, so the
 *  stride is a whole `MeshVertex` in every form and a program that does
 *  not read it is simply not given an attribute it never declared. */
std::span<const Diligent::LayoutElement> meshLayout(bool instanced = false,
                                                    bool withTangent = false);

/** The input number the tangent is read at, when the layout declares it:
 *  after every instance element. */
inline constexpr unsigned kTangentInput = 10;

/** INSTANCED GEOMETRY'S INSTANCES UPLOADED, under the number of the
 *  artefact they came from, as `MeshBuffers` are. */
struct InstanceBuffers {
  Diligent::RefCntAutoPtr<Diligent::IBuffer> instances;
  uint32_t count = 0;
  /** The names of the four scalars each instance carries, in the order
   *  they were packed; an empty name is a scalar nobody named. */
  std::string attributeNames[4];
  uint64_t used = 0;
};

/** A MESH UPLOADED, held under the number the caller gave the artefact
 *  it came from. NOT under its address: an artefact that is dropped
 *  frees its memory and the next one cooked can land on it, so an
 *  address cannot say whether two frames are looking at the same
 *  triangles. */
struct MeshBuffers {
  Diligent::RefCntAutoPtr<Diligent::IBuffer> vertices;
  Diligent::RefCntAutoPtr<Diligent::IBuffer> indices;
  size_t vertexCount = 0;
  uint32_t indexCount = 0;
  /** The PRIMITIVE LANE these buffers were packed for. It is part of
   *  what they are and not only of how they were made: a lane unshares
   *  the vertices and writes a value per triangle, so the same artefact
   *  asked for once with a lane and once without is two different pairs
   *  of buffers, each held under the artefact and the lane together. */
  std::string primitiveColorLane;
  /** THE TRIANGLES EACH MATERIAL SLOT WEARS, as index buffers over the
   *  same vertices: slot k's holds every triangle whose "Material"
   *  primitive lane names k, in the mesh's order. Made on the first ask,
   *  every slot at once, with an empty buffer and a count of zero for a
   *  slot no triangle names. */
  std::vector<Diligent::RefCntAutoPtr<Diligent::IBuffer>> slotIndices;
  std::vector<uint32_t> slotIndexCounts;
  /** The frame this was last drawn in, so a mesh nobody names any more
   *  is let go. */
  uint64_t used = 0;
};

/**
 * The meshes standing on one device, and what it takes to put one there.
 *
 * A caller that can NAME its mesh — a cooked artefact carries a number —
 * uploads once and draws from the same buffers every frame after.
 * A caller that cannot streams instead: one pair of buffers, grown to
 * fit and overwritten by the next draw, because a draw whose seam
 * carries no number is told nothing that says two of them are the same
 * triangles and there is nothing to key a cache on.
 *
 * `endFrame()` closes the frame and lets go of what no draw has named
 * lately, so a window sliding along a curve — resolving a fresh artefact
 * every frame — does not hold what it cooked for the life of the scene.
 */
class MeshResidency {
 public:
  explicit MeshResidency(Device& device);
  ~MeshResidency();
  MeshResidency(const MeshResidency&) = delete;
  MeshResidency& operator=(const MeshResidency&) = delete;

  /** @p mesh's buffers, uploaded the first time @p artefact is asked
   *  for with @p primitiveColorLane and held under the two together: one
   *  mesh drawn tinted by a lane and untinted in the same frame is two
   *  packings, each uploaded once. A caller cooking a mesh of its own —
   *  the stamps of a point set — has no artefact to name, and passes an
   *  id of its own that no frame after it repeats. Null when the mesh has
   *  no triangles or the device refused the buffers. */
  const MeshBuffers* upload(uint64_t artefact, const mesh::Mesh& mesh,
                            std::string_view primitiveColorLane = {});

  /** @p mesh in the streaming buffers, overwriting whatever draw wrote
   *  them last. For a caller whose seam carries no artefact number. */
  const MeshBuffers* stream(const mesh::Mesh& mesh,
                            std::string_view primitiveColorLane = {});

  /** @p instances' instances, uploaded the first time @p artefact is
   *  asked for and kept as a mesh is. The stamp is uploaded apart, under
   *  its own number. Null when there are no instances or the device
   *  refused the buffer. */
  /** THE TRIANGLES OF @p artefact's buffers packed for
   *  @p primitiveColorLane that wear slot @p slot, read off @p mesh's
   *  "Material" primitive lane — which the buffers must have been
   *  uploaded from first. Null when the mesh carries no such lane, names
   *  no triangle with that slot, or was never uploaded with that lane;
   *  the count is written to @p count. */
  Diligent::IBuffer* slotIndices(uint64_t artefact, const mesh::Mesh& mesh,
                                 std::string_view primitiveColorLane,
                                 size_t slot, uint32_t* count);
  const InstanceBuffers* instances(uint64_t artefact,
                                   const mesh::Instances& instances);

  /** ONE NEUTRAL INSTANCE — standing at the origin, unturned, unscaled
   *  and white — which is how a mesh that is not instanced is drawn
   *  through a program that reads an instance. Made on the first ask. */
  const InstanceBuffers* single();

  /** Closes the frame: what no draw has named lately is released. */
  void endFrame();

 private:
  Device* m_device = nullptr;
  uint64_t m_frame = 0;
  /** An artefact's packings, one per primitive lane it is asked for
   *  with; the empty lane is the mesh's own shared vertices. */
  boost::container::map<std::pair<uint64_t, std::string>, MeshBuffers>
      m_meshes;
  boost::container::map<uint64_t, InstanceBuffers> m_instances;
  InstanceBuffers m_single;
  /** THE ONE PAIR OF BUFFERS a streamed mesh is written into. The two
   *  below own the storage and the growth; `m_streamed` carries the
   *  counts of THIS draw, which are not the buffers'. Each is made on
   *  the first stream, because a `DynamicBuffer` cannot be moved and a
   *  caller that never streams should hold no buffer at all. */
  std::unique_ptr<Diligent::DynamicBuffer> m_streamVertices;
  std::unique_ptr<Diligent::DynamicBuffer> m_streamIndices;
  MeshBuffers m_streamed;
};

}  // namespace sigil::geometry::device
