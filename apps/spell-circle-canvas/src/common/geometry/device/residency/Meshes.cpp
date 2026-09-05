/** @file
 * The crossing itself: a mesh packed into the one vertex layout this
 * device reads, the buffers it lands in, and how long they are kept.
 */

#include "Meshes.h"

#include <Graphics/GraphicsTools/interface/DynamicBuffer.hpp>
#include <utility>
#include <vector>

namespace sigil::geometry::device {

namespace dg = Diligent;

namespace {

/** How long a mesh stays uploaded after the last draw that named it. A
 *  window sliding along a curve resolves a fresh artefact every frame,
 *  so what it cooked last frame must not be held for the life of the
 *  scene. */
constexpr uint64_t kMeshLifetime = 2;

/** @p mesh as the one vertex layout this device reads, with the lanes it
 *  does not carry filled in.
 *
 *  A PRIMITIVE lane makes the vertices unshared: its value belongs to a
 *  triangle, and a vertex two triangles meet at cannot hold two of them.
 *  Without one the mesh's own indices stand. */
void fillVertices(const mesh::Mesh& mesh, std::string_view primColorLane,
                  std::vector<MeshVertex>* vertices,
                  std::vector<uint32_t>* indices) {
  const size_t n = mesh.vertexCount();
  const bool hasNormals = mesh.normals.size() == n;
  const bool hasUvs = mesh.uvs.size() == n;
  const bool hasColors = mesh.colors.size() == n;
  const std::vector<glm::vec4>* prim =
      primColorLane.empty() ? nullptr : mesh.primIf(primColorLane);
  if (prim && prim->size() != mesh.triangleCount()) prim = nullptr;

  const auto write = [&](size_t i, glm::vec4 tint) {
    MeshVertex v;
    v.position[0] = mesh.positions[i].x;
    v.position[1] = mesh.positions[i].y;
    v.position[2] = mesh.positions[i].z;
    const glm::vec3 n3 = hasNormals ? mesh.normals[i] : glm::vec3{0, 0, 1};
    v.normal[0] = n3.x;
    v.normal[1] = n3.y;
    v.normal[2] = n3.z;
    const glm::vec2 uv = hasUvs ? mesh.uvs[i] : glm::vec2{0, 0};
    v.uv[0] = uv.x;
    v.uv[1] = uv.y;
    const glm::vec4 c = hasColors ? mesh.colors[i] : glm::vec4{1, 1, 1, 1};
    v.color[0] = c.r;
    v.color[1] = c.g;
    v.color[2] = c.b;
    v.color[3] = c.a;
    v.prim[0] = tint.r;
    v.prim[1] = tint.g;
    v.prim[2] = tint.b;
    v.prim[3] = tint.a;
    vertices->push_back(v);
  };

  if (!prim) {
    vertices->reserve(n);
    for (size_t i = 0; i < n; ++i) write(i, {1, 1, 1, 1});
    *indices = mesh.indices;
    return;
  }
  vertices->reserve(mesh.indices.size());
  indices->reserve(mesh.indices.size());
  for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
    for (size_t k = 0; k < 3; ++k) {
      indices->push_back((uint32_t)vertices->size());
      write(mesh.indices[t + k], (*prim)[t / 3]);
    }
}

}  // namespace

std::span<const dg::LayoutElement> meshLayout(bool withPrimitiveLane) {
  // THE STRIDE IS STATED rather than derived from the elements, because
  // a vertex carries the primitive lane whether or not the program that
  // reads the others declares it: a layout of four elements over these
  // vertices still steps a whole one.
  static const dg::LayoutElement elements[] = {
      dg::LayoutElement{0, 0, 3, dg::VT_FLOAT32, dg::False,
                        dg::LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(MeshVertex)},
      dg::LayoutElement{1, 0, 3, dg::VT_FLOAT32, dg::False,
                        dg::LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(MeshVertex)},
      dg::LayoutElement{2, 0, 2, dg::VT_FLOAT32, dg::False,
                        dg::LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(MeshVertex)},
      dg::LayoutElement{3, 0, 4, dg::VT_FLOAT32, dg::False,
                        dg::LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(MeshVertex)},
      dg::LayoutElement{4, 0, 4, dg::VT_FLOAT32, dg::False,
                        dg::LAYOUT_ELEMENT_AUTO_OFFSET, sizeof(MeshVertex)},
  };
  return {elements, withPrimitiveLane ? 5u : 4u};
}

MeshResidency::MeshResidency(Device& device) : m_device(&device) {}

MeshResidency::~MeshResidency() = default;

const MeshBuffers* MeshResidency::upload(uint64_t artefact,
                                         const mesh::Mesh& mesh,
                                         std::string_view primColorLane) {
  if (mesh.positions.empty() || mesh.indices.size() < 3) return nullptr;
  MeshBuffers& buffers = m_meshes[artefact];
  buffers.used = m_frame;
  if (buffers.vertices) return &buffers;

  std::vector<MeshVertex> vertices;
  std::vector<uint32_t> indices;
  fillVertices(mesh, primColorLane, &vertices, &indices);

  buffers.vertices.Release();
  buffers.indices.Release();
  dg::BufferDesc vd;
  vd.Name = "mesh vertices";
  vd.Size = vertices.size() * sizeof(MeshVertex);
  vd.BindFlags = dg::BIND_VERTEX_BUFFER;
  vd.Usage = dg::USAGE_IMMUTABLE;
  dg::BufferData vdata{vertices.data(), vd.Size};
  m_device->renderDevice()->CreateBuffer(vd, &vdata, &buffers.vertices);

  dg::BufferDesc id;
  id.Name = "mesh indices";
  id.Size = indices.size() * sizeof(uint32_t);
  id.BindFlags = dg::BIND_INDEX_BUFFER;
  id.Usage = dg::USAGE_IMMUTABLE;
  dg::BufferData idata{indices.data(), id.Size};
  m_device->renderDevice()->CreateBuffer(id, &idata, &buffers.indices);

  buffers.vertexCount = vertices.size();
  buffers.indexCount = (uint32_t)indices.size();
  return buffers.vertices && buffers.indices ? &buffers : nullptr;
}

const MeshBuffers* MeshResidency::stream(const mesh::Mesh& mesh,
                                         std::string_view primColorLane) {
  if (mesh.positions.empty() || mesh.indices.size() < 3) return nullptr;
  std::vector<MeshVertex> vertices;
  std::vector<uint32_t> indices;
  fillVertices(mesh, primColorLane, &vertices, &indices);

  dg::IRenderDevice* renderDevice = m_device->renderDevice();
  dg::IDeviceContext* context = m_device->context();
  const size_t vertexBytes = vertices.size() * sizeof(MeshVertex);
  const size_t indexBytes = indices.size() * sizeof(uint32_t);

  // GROWN, NEVER SHRUNK: a smaller mesh after a larger one writes into
  // the buffer that is already big enough rather than making one its own
  // size, so a stream that has settled allocates nothing. The resize
  // discards, because every draw overwrites the whole of what it reads
  // and copying the last draw's triangles into the new buffer would be
  // work nobody looks at.
  const auto grow = [&](std::unique_ptr<dg::DynamicBuffer>& held,
                        const char* name, dg::BIND_FLAGS bind,
                        size_t bytes) -> dg::IBuffer* {
    if (!held) {
      dg::DynamicBufferCreateInfo info;
      info.Desc.Name = name;
      info.Desc.Size = bytes;
      info.Desc.BindFlags = bind;
      info.Desc.Usage = dg::USAGE_DEFAULT;
      held = std::make_unique<dg::DynamicBuffer>(renderDevice, info);
    } else if (held->GetDesc().Size < bytes) {
      held->Resize(renderDevice, context, bytes, /*DiscardContent=*/true);
    }
    return held->Update(renderDevice, context);
  };
  m_streamed.vertices = grow(m_streamVertices, "streamed mesh vertices",
                             dg::BIND_VERTEX_BUFFER, vertexBytes);
  m_streamed.indices = grow(m_streamIndices, "streamed mesh indices",
                            dg::BIND_INDEX_BUFFER, indexBytes);
  if (!m_streamed.vertices || !m_streamed.indices) return nullptr;
  context->UpdateBuffer(m_streamed.vertices, 0, vertexBytes, vertices.data(),
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->UpdateBuffer(m_streamed.indices, 0, indexBytes, indices.data(),
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  m_streamed.used = m_frame;
  m_streamed.vertexCount = vertices.size();
  m_streamed.indexCount = (uint32_t)indices.size();
  return &m_streamed;
}

void MeshResidency::endFrame() {
  ++m_frame;
  for (auto it = m_meshes.begin(); it != m_meshes.end();) {
    if (m_frame - it->second.used > kMeshLifetime)
      it = m_meshes.erase(it);
    else
      ++it;
  }
}

}  // namespace sigil::geometry::device
