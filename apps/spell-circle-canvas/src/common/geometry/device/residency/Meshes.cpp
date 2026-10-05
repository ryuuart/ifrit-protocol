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
void fillVertices(const mesh::Mesh& mesh, std::string_view primitiveColorLane,
                  std::vector<MeshVertex>* vertices,
                  std::vector<uint32_t>* indices) {
  const size_t n = mesh.vertexCount();
  const bool hasNormals = mesh.normals.size() == n;
  const bool hasUvs = mesh.uvs.size() == n;
  const bool hasColors = mesh.colors.size() == n;
  const std::vector<glm::vec4>* primitive =
      primitiveColorLane.empty() ? nullptr
                                 : mesh.primitiveIf(primitiveColorLane);
  if (primitive && primitive->size() != mesh.triangleCount())
    primitive = nullptr;

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
    v.primitive[0] = tint.r;
    v.primitive[1] = tint.g;
    v.primitive[2] = tint.b;
    v.primitive[3] = tint.a;
    vertices->push_back(v);
  };

  if (!primitive) {
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
      write(mesh.indices[t + k], (*primitive)[t / 3]);
    }
}

}  // namespace

std::span<const dg::LayoutElement> meshLayout(bool withPrimitiveLane,
                                              bool instanced) {
  // THE STRIDE IS STATED rather than derived from the elements, because
  // a vertex carries the primitive lane whether or not the program that
  // reads the others declares it: a layout of four elements over these
  // vertices still steps a whole one.
  const auto vertex = [](dg::Uint32 input, dg::Uint32 components) {
    return dg::LayoutElement{input,
                             0,
                             components,
                             dg::VT_FLOAT32,
                             dg::False,
                             dg::LAYOUT_ELEMENT_AUTO_OFFSET,
                             sizeof(MeshVertex)};
  };
  // …and the instance elements step once per instance through the second
  // buffer, a whole `InstanceVertex` at a time.
  const auto instance = [](dg::Uint32 input) {
    return dg::LayoutElement{input,
                             1,
                             4,
                             dg::VT_FLOAT32,
                             dg::False,
                             dg::LAYOUT_ELEMENT_AUTO_OFFSET,
                             sizeof(InstanceVertex),
                             dg::INPUT_ELEMENT_FREQUENCY_PER_INSTANCE};
  };
  const auto build = [&](bool primitive, bool instances) {
    std::vector<dg::LayoutElement> elements = {vertex(0, 3), vertex(1, 3),
                                               vertex(2, 2), vertex(3, 4)};
    if (primitive) elements.push_back(vertex(4, 4));
    if (instances)
      for (dg::Uint32 i = 0; i < 6; ++i)
        elements.push_back(instance(kFirstInstanceInput + i));
    return elements;
  };
  static const std::vector<dg::LayoutElement> layouts[4] = {
      build(false, false), build(true, false), build(false, true),
      build(true, true)};
  const std::vector<dg::LayoutElement>& chosen =
      layouts[(withPrimitiveLane ? 1 : 0) + (instanced ? 2 : 0)];
  return {chosen.data(), chosen.size()};
}

namespace {

/** One instance as the device reads it. */
InstanceVertex instanceAt(const mesh::Instances& instances, size_t i,
                          const std::string (&names)[4]) {
  InstanceVertex out{};
  const glm::vec3 p = instances.positions[i];
  out.position[0] = p.x;
  out.position[1] = p.y;
  out.position[2] = p.z;
  const glm::quat q = i < instances.orientations.size()
                          ? instances.orientations[i]
                          : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  out.orientation[0] = q.x;
  out.orientation[1] = q.y;
  out.orientation[2] = q.z;
  out.orientation[3] = q.w;
  const glm::vec3 s =
      i < instances.scales.size() ? instances.scales[i] : glm::vec3(1.0f);
  out.scale[0] = s.x;
  out.scale[1] = s.y;
  out.scale[2] = s.z;
  const glm::vec4 c =
      i < instances.colors.size() ? instances.colors[i] : glm::vec4(1.0f);
  for (int k = 0; k < 4; ++k) out.color[k] = c[k];
  const glm::vec4 w = i < instances.textureWindows.size()
                          ? instances.textureWindows[i]
                          : glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
  for (int k = 0; k < 4; ++k) out.textureWindow[k] = w[k];
  for (int k = 0; k < 4; ++k) {
    const std::vector<float>* lane =
        names[k].empty() ? nullptr : instances.attributeIf(names[k]);
    out.attributes[k] = lane && i < lane->size() ? (*lane)[i] : 0.0f;
  }
  return out;
}

}  // namespace

MeshResidency::MeshResidency(Device& device) : m_device(&device) {}

MeshResidency::~MeshResidency() = default;

const MeshBuffers* MeshResidency::upload(uint64_t artefact,
                                         const mesh::Mesh& mesh,
                                         std::string_view primitiveColorLane) {
  if (mesh.positions.empty() || mesh.indices.size() < 3) return nullptr;
  MeshBuffers& buffers = m_meshes[artefact];
  buffers.used = m_frame;
  // The lane is half the key. One artefact drawn once with a primitive
  // lane and once without is two packings — unshared vertices carrying a
  // tint per triangle, and the mesh's own shared ones — so the held pair
  // answers only the lane it was packed for.
  if (buffers.vertices && buffers.primitiveColorLane == primitiveColorLane)
    return &buffers;

  std::vector<MeshVertex> vertices;
  std::vector<uint32_t> indices;
  fillVertices(mesh, primitiveColorLane, &vertices, &indices);
  buffers.primitiveColorLane = primitiveColorLane;

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
                                         std::string_view primitiveColorLane) {
  if (mesh.positions.empty() || mesh.indices.size() < 3) return nullptr;
  std::vector<MeshVertex> vertices;
  std::vector<uint32_t> indices;
  fillVertices(mesh, primitiveColorLane, &vertices, &indices);

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

const InstanceBuffers* MeshResidency::instances(
    uint64_t artefact, const mesh::Instances& instances) {
  if (instances.positions.empty()) return nullptr;
  InstanceBuffers& buffers = m_instances[artefact];
  buffers.used = m_frame;
  if (buffers.instances) return &buffers;

  // The first four names in the attributes' own order; the rest are not
  // drawn.
  size_t k = 0;
  for (const auto& [name, values] : instances.attributes) {
    if (k == 4) break;
    buffers.attributeNames[k++] = name;
  }
  std::vector<InstanceVertex> packed;
  packed.reserve(instances.size());
  for (size_t i = 0; i < instances.size(); ++i)
    packed.push_back(instanceAt(instances, i, buffers.attributeNames));

  dg::BufferDesc desc;
  desc.Name = "instances";
  desc.Size = packed.size() * sizeof(InstanceVertex);
  desc.BindFlags = dg::BIND_VERTEX_BUFFER;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::BufferData data{packed.data(), desc.Size};
  m_device->renderDevice()->CreateBuffer(desc, &data, &buffers.instances);
  buffers.count = (uint32_t)packed.size();
  return buffers.instances ? &buffers : nullptr;
}

const InstanceBuffers* MeshResidency::single() {
  if (m_single.instances) return &m_single;
  mesh::Instances one;
  one.positions.emplace_back(0.0f);
  const InstanceVertex neutral = instanceAt(one, 0, m_single.attributeNames);
  dg::BufferDesc desc;
  desc.Name = "one neutral instance";
  desc.Size = sizeof(InstanceVertex);
  desc.BindFlags = dg::BIND_VERTEX_BUFFER;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::BufferData data{&neutral, desc.Size};
  m_device->renderDevice()->CreateBuffer(desc, &data, &m_single.instances);
  m_single.count = 1;
  return m_single.instances ? &m_single : nullptr;
}

void MeshResidency::endFrame() {
  ++m_frame;
  for (auto it = m_instances.begin(); it != m_instances.end();) {
    if (m_frame - it->second.used > kMeshLifetime)
      it = m_instances.erase(it);
    else
      ++it;
  }
  for (auto it = m_meshes.begin(); it != m_meshes.end();) {
    if (m_frame - it->second.used > kMeshLifetime)
      it = m_meshes.erase(it);
    else
      ++it;
  }
}

}  // namespace sigil::geometry::device
