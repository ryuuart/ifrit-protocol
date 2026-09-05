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
#include <sigilgeometry/mesh/Mesh.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <boost/container/map.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

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
  float color[4];
  /** The PRIMITIVE lane the triangle this vertex belongs to carries, or
   *  ones where no lane was named. A triangle has nowhere of its own to
   *  hold a value, so its three vertices each carry it. */
  float prim[4];
};

/** THE LAYOUT A PIPELINE DECLARES over these buffers, in the order the
 *  elements are numbered, with the fifth — the primitive lane — present
 *  only when @p withPrimitiveLane. Every vertex carries one either way,
 *  because it is the same buffer, so the stride is a whole `MeshVertex`
 *  in both answers and a program that does not read the lane is simply
 *  not given an attribute it never declared. */
std::span<const Diligent::LayoutElement> meshLayout(bool withPrimitiveLane);

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
   *  for. A caller cooking a mesh of its own — the stamps of a point
   *  set — has no artefact to name, and passes an id of its own that no
   *  frame after it repeats. Null when the mesh has no triangles or the
   *  device refused the buffers. */
  const MeshBuffers* upload(uint64_t artefact, const mesh::Mesh& mesh,
                            std::string_view primColorLane = {});

  /** @p mesh in the streaming buffers, overwriting whatever draw wrote
   *  them last. For a caller whose seam carries no artefact number. */
  const MeshBuffers* stream(const mesh::Mesh& mesh,
                            std::string_view primColorLane = {});

  /** Closes the frame: what no draw has named lately is released. */
  void endFrame();

 private:
  Device* m_device = nullptr;
  uint64_t m_frame = 0;
  boost::container::map<uint64_t, MeshBuffers> m_meshes;
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
