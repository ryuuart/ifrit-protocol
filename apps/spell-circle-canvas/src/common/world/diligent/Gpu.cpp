/** @file
 * The device side of the executor: textures for the frame's resources,
 * buffers for the meshes a view names, pipelines from compiled programs,
 * and the readback that brings one resource's pixels home.
 */

#include "Gpu.h"

#include <sigilskia/device/Pixels.h>

// clang-format off
// ORDER IS LOAD-BEARING HERE, which is why the sorter is held off: the
// engine's Vulkan interface names Vulkan's handle types and does not
// include the header that declares them, so an alphabetical sort of the
// two leaves every one of those names unknown.
#include <vulkan/vulkan.h>
#include <Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>
// clang-format on

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>
#include <sigilskia/device/GpuDevice.h>

#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sigil::world::diligent {

namespace {

/** ONE VERTEX, as every pipeline here reads it. A mesh that carries no
 *  normals, uvs or tint is filled in on upload rather than compiled
 *  against a second layout — which is what makes the vertex layout a
 *  constant of this backend instead of an axis a program varies on. */
struct Vertex {
  float position[3];
  float normal[3];
  float uv[2];
  float color[4];
  /** The PRIMITIVE lane the triangle this vertex belongs to carries, or
   *  ones where no lane was named. A triangle has nowhere of its own to
   *  hold a value, so its three vertices each carry it. */
  float prim[4];
};

/** @p mesh as the one vertex layout this backend reads, with the lanes
 *  it does not carry filled in.
 *
 *  A PRIMITIVE lane makes the vertices unshared: its value belongs to a
 *  triangle, and a vertex two triangles meet at cannot hold two of them.
 *  Without one the mesh's own indices stand. */
void fillVertices(const Mesh& mesh, std::string_view primColorLane,
                  std::vector<Vertex>* vertices,
                  std::vector<uint32_t>* indices) {
  const size_t n = mesh.vertexCount();
  const bool hasNormals = mesh.normals.size() == n;
  const bool hasUvs = mesh.uvs.size() == n;
  const bool hasColors = mesh.colors.size() == n;
  const std::vector<glm::vec4>* prim =
      primColorLane.empty() ? nullptr : mesh.primIf(primColorLane);
  if (prim && prim->size() != mesh.triangleCount()) prim = nullptr;

  const auto write = [&](size_t i, glm::vec4 tint) {
    Vertex v;
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

/** How long a mesh stays uploaded after the last view that named it. A
 *  window sliding along a curve resolves a fresh artefact every frame,
 *  so what it cooked last frame must not be held for the life of the
 *  scene. */
constexpr uint64_t kMeshLifetime = 2;

/** …and how long a map stays, on the same terms. */
constexpr uint64_t kMapLifetime = 2;

/** The description every map is given, whether it was uploaded or
 *  wrapped: a plain two-dimensional colour texture a shader reads. */
dg::TextureDesc mapDesc(const char* label, int width, int height) {
  dg::TextureDesc desc;
  desc.Name = label;
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)width;
  desc.Height = (dg::Uint32)height;
  desc.MipLevels = 1;
  desc.Format = kColorFormat;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_DEFAULT;
  return desc;
}

/** WHAT MULTIPLIES THE SOURCE, for every mode this backend has: one.
 *  Colour arrives premultiplied — its alpha is already in it — so the
 *  source is taken whole and the mode is decided entirely by what
 *  multiplies the destination. */
constexpr dg::BLEND_FACTOR kSrcFactor = dg::BLEND_FACTOR_ONE;

dg::BLEND_FACTOR dstFactorOf(SkBlendMode mode) {
  switch (mode) {
    case SkBlendMode::kSrc:
      return dg::BLEND_FACTOR_ZERO;
    case SkBlendMode::kPlus:
      return dg::BLEND_FACTOR_ONE;
    default:
      return dg::BLEND_FACTOR_INV_SRC_ALPHA;
  }
}

}  // namespace

Gpu::~Gpu() {
  // Everything below borrows the device's queue; nothing may be
  // recording when the objects behind it go.
  if (device && device->context()) device->context()->Flush();
}

void Uniforms::set(std::string_view name, const float* values, size_t count) {
  const UniformSlot* slot = m_program->uniform(name);
  if (!slot) return;
  const size_t perRow = slot->count ? count / slot->count : count;
  if (slot->stride == 0 || slot->count <= 1) {
    const size_t bytes = std::min(count * sizeof(float), slot->bytes);
    std::memcpy(m_bytes.data() + slot->offset, values, bytes);
    return;
  }
  for (size_t row = 0; row < slot->count; ++row) {
    const size_t at = slot->offset + row * slot->stride;
    if (at + perRow * sizeof(float) > m_bytes.size()) break;
    std::memcpy(m_bytes.data() + at, values + row * perRow,
                perRow * sizeof(float));
  }
}

void Uniforms::set(std::string_view name, const glm::mat4& m) {
  // The shader reads a matrix row by row, and glm holds it column by
  // column, so what is written is the transpose.
  float rows[16];
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) rows[r * 4 + c] = m[c][r];
  set(name, rows, 16);
}

void Uniforms::set(std::string_view name, float x, float y, float z, float w) {
  const float values[4] = {x, y, z, w};
  set(name, values, 4);
}

void Uniforms::setElement(std::string_view name, size_t index,
                          const float* values, size_t count) {
  const UniformSlot* slot = m_program->uniform(name);
  if (!slot || index >= slot->count) return;
  const size_t at = slot->offset + index * slot->stride;
  if (at + count * sizeof(float) > m_bytes.size()) return;
  std::memcpy(m_bytes.data() + at, values, count * sizeof(float));
}

dg::RefCntAutoPtr<dg::ITexture> Gpu::makeColor(const char* label) {
  dg::RefCntAutoPtr<dg::ITexture> texture;
  if (extent.isEmpty() || !device->renderDevice()) return texture;
  dg::TextureDesc desc;
  desc.Name = label;
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)extent.width();
  desc.Height = (dg::Uint32)extent.height();
  desc.Format = kColorFormat;
  desc.BindFlags = dg::BIND_RENDER_TARGET | dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_DEFAULT;
  device->renderDevice()->CreateTexture(desc, nullptr, &texture);
  return texture;
}

void Gpu::resize(SkISize size) {
  if (size == extent) return;
  extent = size;
  images.clear();
  depth.Release();
  scratch.clear();
  if (extent.isEmpty() || !device->renderDevice()) return;
  dg::TextureDesc desc;
  desc.Name = "world depth";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)extent.width();
  desc.Height = (dg::Uint32)extent.height();
  desc.Format = kDepthFormat;
  desc.BindFlags = dg::BIND_DEPTH_STENCIL;
  desc.Usage = dg::USAGE_DEFAULT;
  device->renderDevice()->CreateTexture(desc, nullptr, &depth);
}

dg::ITexture* Gpu::working(size_t index) {
  if (extent.isEmpty()) return nullptr;
  if (scratch.size() <= index) scratch.resize(index + 1);
  if (!scratch[index]) scratch[index] = makeColor("world working target");
  return scratch[index];
}

dg::ITexture* Gpu::target(std::string_view name) {
  auto it = images.find(name);
  if (it == images.end())
    it = images.emplace(std::string(name), DeviceImage{}).first;
  if (!it->second.current) it->second.current = makeColor("world target");
  it->second.written = true;
  return it->second.current;
}

dg::ITexture* Gpu::current(std::string_view name) {
  const auto it = images.find(name);
  return it == images.end() ? nullptr : it->second.current.RawPtr();
}

dg::ITexture* Gpu::previous(std::string_view name) {
  const auto it = images.find(name);
  return it == images.end() ? nullptr : it->second.previous.RawPtr();
}

void Gpu::beginFrame() {
  for (auto& [name, image] : images) {
    if (!image.written) continue;
    // THE ROTATION HAPPENS HERE and not at the end, because between one
    // frame's last pass and the next frame's first the resources must
    // still read as what that frame wrote: that is when the picture is
    // presented and when a readback is taken. Every stage either clears
    // its target or replaces it whole, which is what makes writing into
    // a texture with the frame-before-last's pixels in it safe.
    std::swap(image.current, image.previous);
    image.written = false;
  }
}

dg::ITexture* Gpu::sample(const material::Texture& map) {
  if (!device->renderDevice()) return nullptr;

  // ZERO COPY: the pixels already stand on this very device, so what a
  // draw needs is a name for them and not a second copy of them.
  const material::DeviceImage where = map.deviceImage();
  if (where && where.device == device->gpu() && where.handle != 0) {
    SampledImage& held = wrapped[where.handle];
    held.used = frame;
    if (!held.texture) {
      auto* vk = static_cast<dg::IRenderDeviceVk*>(device->renderDevice());
      // The image was drawn into and then submitted, so what a sampler
      // reads is what it was left as: a shader resource.
      // The image arrives as a NUMBER, because the value that carried it
      // here belongs to a library that cannot spell a Vulkan type.
      vk->CreateTextureFromVulkanImage(
          reinterpret_cast<VkImage>(  // NOLINT(performance-no-int-to-ptr)
              where.handle),
          mapDesc("world sampled map", where.width, where.height),
          dg::RESOURCE_STATE_SHADER_RESOURCE, &held.texture);
    }
    if (held.texture) return held.texture;
    // The wrap was refused; the pixels are still readable the long way.
  }

  const sk_sp<SkImage> image = map.image();
  if (!image) return nullptr;
  SampledImage& held = uploaded[image->uniqueID()];
  held.used = frame;
  if (held.texture) return held.texture;

  SkBitmap bytes;
  if (!bytes.tryAllocPixels(SkImageInfo::Make(image->width(), image->height(),
                                              kRGBA_8888_SkColorType,
                                              kPremul_SkAlphaType)))
    return nullptr;
  if (!image->readPixels(nullptr, bytes.pixmap(), 0, 0)) return nullptr;

  dg::TextureSubResData level;
  level.pData = bytes.getPixels();
  level.Stride = (dg::Uint64)bytes.rowBytes();
  dg::TextureData data;
  data.pSubResources = &level;
  data.NumSubresources = 1;
  device->renderDevice()->CreateTexture(
      mapDesc("world sampled map", image->width(), image->height()), &data,
      &held.texture);
  return held.texture;
}

dg::ITexture* Gpu::environment(const material::EnvironmentMap& map) {
  if (!device->renderDevice() || !map.valid()) return nullptr;
  const sk_sp<SkImage> base = map.image(0);
  if (!base) return nullptr;
  SampledImage& held = environments[base->uniqueID()];
  held.used = frame;
  if (held.texture) return held.texture;

  const std::vector<sk_sp<SkImage>> levels = map.chain();
  if (levels.empty() || !levels.front()) return nullptr;

  // A SKY IS NOT EIGHT BITS. The values above one are what make a sun a
  // sun rather than a white disc the same brightness as the sky beside
  // it, and they are what a reflection is mostly made of. Half floats
  // keep them and are filterable everywhere; the thirty-two-bit form the
  // panorama was blurred in is not, on an Apple GPU.
  std::vector<std::vector<uint16_t>> pixels;
  std::vector<dg::TextureSubResData> subresources;
  pixels.reserve(levels.size());
  subresources.reserve(levels.size());
  for (const sk_sp<SkImage>& level : levels) {
    pixels.push_back(skia::halfFloatPixels(level));
    if (pixels.back().empty()) return nullptr;
    dg::TextureSubResData data;
    data.pData = pixels.back().data();
    data.Stride = (dg::Uint64)level->width() * 4 * sizeof(uint16_t);
    subresources.push_back(data);
  }

  dg::TextureDesc desc;
  desc.Name = "world environment";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)levels.front()->width();
  desc.Height = (dg::Uint32)levels.front()->height();
  desc.MipLevels = (dg::Uint32)levels.size();
  desc.Format = dg::TEX_FORMAT_RGBA16_FLOAT;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::TextureData data;
  data.pSubResources = subresources.data();
  data.NumSubresources = (dg::Uint32)subresources.size();
  device->renderDevice()->CreateTexture(desc, &data, &held.texture);
  return held.texture;
}

dg::ITexture* Gpu::irradiance(const material::EnvironmentMap& map) {
  if (!device->renderDevice() || !map.valid()) return nullptr;
  const sk_sp<SkImage> base = map.image(0);
  if (!base) return nullptr;
  SampledImage& held = irradiances[base->uniqueID()];
  held.used = frame;
  if (held.texture) return held.texture;

  const sk_sp<SkImage> lobe = map.irradiance();
  if (!lobe) return nullptr;
  const std::vector<uint16_t> pixels = skia::halfFloatPixels(lobe);
  if (pixels.empty()) return nullptr;
  dg::TextureSubResData level;
  level.pData = pixels.data();
  level.Stride = (dg::Uint64)lobe->width() * 4 * sizeof(uint16_t);
  dg::TextureDesc desc;
  desc.Name = "world irradiance";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)lobe->width();
  desc.Height = (dg::Uint32)lobe->height();
  desc.MipLevels = 1;
  desc.Format = dg::TEX_FORMAT_RGBA16_FLOAT;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::TextureData data{&level, 1};
  device->renderDevice()->CreateTexture(desc, &data, &held.texture);
  return held.texture;
}

void Gpu::endFrame() {
  // THE FRAME IS CLOSED ON THE DEVICE TOO. Every draw's uniforms come
  // from a heap the device refills once a frame, and a texture let go of
  // here is released once the frames that could still name it are done —
  // and neither happens until the frame is finished, so a run of frames
  // that never finished one would exhaust the heap and hold every
  // texture it ever made.
  if (dg::IDeviceContext* context = device->context()) {
    context->Flush();
    context->FinishFrame();
  }
  ++frame;
  for (auto it = meshes.begin(); it != meshes.end();) {
    if (frame - it->second.used > kMeshLifetime)
      it = meshes.erase(it);
    else
      ++it;
  }
  for (auto it = wrapped.begin(); it != wrapped.end();) {
    if (frame - it->second.used > kMapLifetime)
      it = wrapped.erase(it);
    else
      ++it;
  }
  for (auto it = uploaded.begin(); it != uploaded.end();) {
    if (frame - it->second.used > kMapLifetime)
      it = uploaded.erase(it);
    else
      ++it;
  }
}

const MeshBuffers* Gpu::upload(uint64_t artefact, const Mesh& mesh,
                               std::string_view primColorLane) {
  if (mesh.positions.empty() || mesh.indices.size() < 3) return nullptr;
  MeshBuffers& buffers = meshes[artefact];
  buffers.used = frame;
  if (buffers.vertices) return &buffers;

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  fillVertices(mesh, primColorLane, &vertices, &indices);

  buffers.vertices.Release();
  buffers.indices.Release();
  dg::BufferDesc vd;
  vd.Name = "world mesh vertices";
  vd.Size = vertices.size() * sizeof(Vertex);
  vd.BindFlags = dg::BIND_VERTEX_BUFFER;
  vd.Usage = dg::USAGE_IMMUTABLE;
  dg::BufferData vdata{vertices.data(), vd.Size};
  device->renderDevice()->CreateBuffer(vd, &vdata, &buffers.vertices);

  dg::BufferDesc id;
  id.Name = "world mesh indices";
  id.Size = indices.size() * sizeof(uint32_t);
  id.BindFlags = dg::BIND_INDEX_BUFFER;
  id.Usage = dg::USAGE_IMMUTABLE;
  dg::BufferData idata{indices.data(), id.Size};
  device->renderDevice()->CreateBuffer(id, &idata, &buffers.indices);

  buffers.vertexCount = vertices.size();
  buffers.indexCount = (uint32_t)indices.size();
  return buffers.vertices && buffers.indices ? &buffers : nullptr;
}

const MeshBuffers* Gpu::stream(const Mesh& mesh,
                               std::string_view primColorLane) {
  if (mesh.positions.empty() || mesh.indices.size() < 3) return nullptr;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  fillVertices(mesh, primColorLane, &vertices, &indices);

  dg::IRenderDevice* renderDevice = device->renderDevice();
  // GROWN, NEVER SHRUNK, and the capacities are held APART from the
  // counts: a draw reads how many indices THIS mesh has, and a smaller
  // mesh after a larger one must still write into the buffer that is
  // already big enough rather than make a smaller one.
  const size_t vertexBytes = vertices.size() * sizeof(Vertex);
  const size_t indexBytes = indices.size() * sizeof(uint32_t);
  if (!streamed.vertices || streamedVertices < vertices.size()) {
    streamed.vertices.Release();
    dg::BufferDesc vd;
    vd.Name = "world streamed vertices";
    vd.Size = vertexBytes;
    vd.BindFlags = dg::BIND_VERTEX_BUFFER;
    vd.Usage = dg::USAGE_DEFAULT;
    renderDevice->CreateBuffer(vd, nullptr, &streamed.vertices);
    streamedVertices = streamed.vertices ? vertices.size() : 0;
  }
  if (!streamed.indices || streamedIndices < indices.size()) {
    streamed.indices.Release();
    dg::BufferDesc id;
    id.Name = "world streamed indices";
    id.Size = indexBytes;
    id.BindFlags = dg::BIND_INDEX_BUFFER;
    id.Usage = dg::USAGE_DEFAULT;
    renderDevice->CreateBuffer(id, nullptr, &streamed.indices);
    streamedIndices = streamed.indices ? indices.size() : 0;
  }
  if (!streamed.vertices || !streamed.indices) return nullptr;
  dg::IDeviceContext* context = device->context();
  context->UpdateBuffer(streamed.vertices, 0, vertexBytes, vertices.data(),
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->UpdateBuffer(streamed.indices, 0, indexBytes, indices.data(),
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  streamed.used = frame;
  streamed.vertexCount = vertices.size();
  streamed.indexCount = (uint32_t)indices.size();
  return &streamed;
}

dg::IBuffer* Gpu::uniformBuffer(size_t bytes) {
  const size_t wanted = std::max<size_t>(bytes, 256);
  if (uniforms && uniformCapacity >= wanted) return uniforms;
  uniforms.Release();
  dg::BufferDesc desc;
  desc.Name = "world draw uniforms";
  desc.Size = wanted;
  desc.BindFlags = dg::BIND_UNIFORM_BUFFER;
  desc.Usage = dg::USAGE_DYNAMIC;
  desc.CPUAccessFlags = dg::CPU_ACCESS_WRITE;
  device->renderDevice()->CreateBuffer(desc, nullptr, &uniforms);
  uniformCapacity = uniforms ? wanted : 0;
  return uniforms;
}

const Pipeline* Gpu::pipeline(const PipelineKey& key) {
  if (!key.program || key.program->empty()) return nullptr;
  const auto found = pipelines.find(key);
  if (found != pipelines.end())
    return found->second.state ? &found->second : nullptr;

  dg::IRenderDevice* renderDevice = device->renderDevice();
  Pipeline built;
  dg::RefCntAutoPtr<dg::IShader> vs;
  dg::RefCntAutoPtr<dg::IShader> ps;
  {
    dg::ShaderCreateInfo ci;
    ci.Desc.Name = "world vertex";
    ci.Desc.ShaderType = dg::SHADER_TYPE_VERTEX;
    ci.Desc.UseCombinedTextureSamplers = true;
    ci.ByteCode = key.program->vertex.data();
    ci.ByteCodeSize = key.program->vertex.size() * sizeof(uint32_t);
    renderDevice->CreateShader(ci, &vs);
  }
  {
    dg::ShaderCreateInfo ci;
    ci.Desc.Name = "world fragment";
    ci.Desc.ShaderType = dg::SHADER_TYPE_PIXEL;
    ci.Desc.UseCombinedTextureSamplers = true;
    ci.ByteCode = key.program->fragment.data();
    ci.ByteCodeSize = key.program->fragment.size() * sizeof(uint32_t);
    renderDevice->CreateShader(ci, &ps);
  }
  if (!vs || !ps) {
    pipelines.emplace(key, Pipeline{});
    return nullptr;
  }

  dg::GraphicsPipelineStateCreateInfo info;
  info.PSODesc.Name = "world pipeline";
  // DYNAMIC, because every one of these is rebound per draw: one uniform
  // buffer rewritten for each body, and the sampled slots a post stage
  // reads changing between one stage and the next.
  info.PSODesc.ResourceLayout.DefaultVariableType =
      dg::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
  info.pVS = vs;
  info.pPS = ps;
  dg::GraphicsPipelineDesc& graphics = info.GraphicsPipeline;
  graphics.NumRenderTargets = 1;
  graphics.RTVFormats[0] = kColorFormat;
  graphics.DSVFormat = key.depth ? kDepthFormat : dg::TEX_FORMAT_UNKNOWN;
  graphics.PrimitiveTopology = dg::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  // Front faces wind counter-clockwise in a y-up space and arrive
  // clockwise after the viewport's flip, which is what this convention
  // calls front — the same reading the CPU executor's cull makes.
  graphics.RasterizerDesc.CullMode =
      key.fullscreen || !key.cull ? dg::CULL_MODE_NONE : dg::CULL_MODE_BACK;
  graphics.RasterizerDesc.FrontCounterClockwise = true;
  graphics.DepthStencilDesc.DepthEnable = key.depth;
  graphics.DepthStencilDesc.DepthWriteEnable = key.depthWrite;
  graphics.DepthStencilDesc.DepthFunc = dg::COMPARISON_FUNC_LESS_EQUAL;

  dg::RenderTargetBlendDesc& blend = graphics.BlendDesc.RenderTargets[0];
  blend.BlendEnable = key.blend != SkBlendMode::kSrc;
  blend.SrcBlend = kSrcFactor;
  blend.DestBlend = dstFactorOf(key.blend);
  blend.BlendOp = dg::BLEND_OPERATION_ADD;
  blend.SrcBlendAlpha = blend.SrcBlend;
  blend.DestBlendAlpha = blend.DestBlend;
  blend.BlendOpAlpha = dg::BLEND_OPERATION_ADD;

  // THE STRIDE IS STATED rather than derived from the elements, because
  // a vertex carries the primitive lane whether or not the program that
  // reads the others declares it: a layout of four elements over these
  // vertices still steps a whole one.
  dg::LayoutElement elements[] = {
      dg::LayoutElement{0, 0, 3, dg::VT_FLOAT32, dg::False},
      dg::LayoutElement{1, 0, 3, dg::VT_FLOAT32, dg::False},
      dg::LayoutElement{2, 0, 2, dg::VT_FLOAT32, dg::False},
      dg::LayoutElement{3, 0, 4, dg::VT_FLOAT32, dg::False},
      dg::LayoutElement{4, 0, 4, dg::VT_FLOAT32, dg::False},
  };
  for (dg::LayoutElement& element : elements) element.Stride = sizeof(Vertex);
  if (!key.fullscreen) {
    graphics.InputLayout.LayoutElements = elements;
    graphics.InputLayout.NumElements = key.prim ? 5 : 4;
  }

  renderDevice->CreateGraphicsPipelineState(info, &built.state);
  if (built.state)
    built.state->CreateShaderResourceBinding(&built.binding, true);
  const auto placed = pipelines.emplace(key, std::move(built)).first;
  return placed->second.state ? &placed->second : nullptr;
}

dg::ISampler* Gpu::samplerFor(SkFilterMode filter, bool tile) const {
  if (filter == SkFilterMode::kNearest)
    return tile ? nearestTiled.RawPtr() : nearestSampler.RawPtr();
  return tile ? linearTiled.RawPtr() : linearSampler.RawPtr();
}

void bindAndCommit(Gpu& gpu, const Pipeline& pipeline, const Compiled& program,
                   const Uniforms& values,
                   const std::vector<dg::ITexture*>& textures,
                   SkFilterMode filter, bool tile,
                   bool (*panoramaSlot)(std::string_view)) {
  dg::IDeviceContext* context = gpu.device->context();
  dg::IBuffer* buffer = gpu.uniformBuffer(program.uniformBytes);
  if (buffer && !values.bytes().empty()) {
    dg::MapHelper<std::byte> mapped(context, buffer, dg::MAP_WRITE,
                                    dg::MAP_FLAG_DISCARD);
    std::memcpy(static_cast<std::byte*>(mapped), values.bytes().data(),
                values.bytes().size());
  }
  if (dg::IShaderResourceVariable* variable =
          pipeline.binding->GetVariableByName(dg::SHADER_TYPE_PIXEL,
                                              "globalParams"))
    variable->Set(buffer);
  if (dg::IShaderResourceVariable* variable =
          pipeline.binding->GetVariableByName(dg::SHADER_TYPE_VERTEX,
                                              "globalParams"))
    variable->Set(buffer);

  for (size_t i = 0; i < program.textures.size(); ++i) {
    dg::ITexture* texture = i < textures.size() ? textures[i] : nullptr;
    if (!texture) texture = gpu.white;
    if (!texture) continue;
    dg::ITextureView* view =
        texture->GetDefaultView(dg::TEXTURE_VIEW_SHADER_RESOURCE);
    if (!view) continue;
    // A combined sampler reaches the shader through the view, which is
    // why the filter is set on the view here rather than through a
    // sampler variable of its own — and set on every draw, because one
    // view may be read by two draws that asked for different filters.
    // ONE FILTER AND ONE WRAP FOR EVERY SLOT IN A DRAW, taken from the
    // base-colour map — except a panorama, which cannot be read that
    // way at all: its u axis is periodic where its v axis ends at the
    // poles, and its levels are prefiltered images a roughness reads
    // across rather than a filtering aid.
    const bool panorama =
        panoramaSlot && panoramaSlot(program.textures[i]) && gpu.panoramaSampler;
    view->SetSampler(panorama ? gpu.panoramaSampler.RawPtr()
                              : gpu.samplerFor(filter, tile));
    if (dg::IShaderResourceVariable* variable =
            pipeline.binding->GetVariableByName(dg::SHADER_TYPE_PIXEL,
                                                program.textures[i].c_str()))
      variable->Set(view);
  }
  context->CommitShaderResources(pipeline.binding,
                                 dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

sk_sp<SkImage> Gpu::read(std::string_view name) {
  dg::ITexture* texture = current(name);
  if (!texture) texture = previous(name);
  return readTexture(texture);
}

sk_sp<SkImage> Gpu::readTexture(dg::ITexture* texture) {
  if (!texture || extent.isEmpty()) return nullptr;

  dg::TextureDesc desc;
  desc.Name = "world readback";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)extent.width();
  desc.Height = (dg::Uint32)extent.height();
  desc.Format = kColorFormat;
  desc.Usage = dg::USAGE_STAGING;
  desc.CPUAccessFlags = dg::CPU_ACCESS_READ;
  desc.BindFlags = dg::BIND_NONE;
  dg::RefCntAutoPtr<dg::ITexture> staging;
  device->renderDevice()->CreateTexture(desc, nullptr, &staging);
  if (!staging) return nullptr;

  // No queue lock here, and none anywhere in this executor: Diligent
  // takes that lock from inside its own submissions, and this path only
  // issues Diligent commands. The lock belongs to a caller mixing
  // Graphite's submissions into the same stream.
  dg::IDeviceContext* context = device->context();
  {
    dg::CopyTextureAttribs copy;
    copy.pSrcTexture = texture;
    copy.SrcTextureTransitionMode =
        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copy.pDstTexture = staging;
    copy.DstTextureTransitionMode =
        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    context->CopyTexture(copy);
    context->WaitForIdle();
  }

  dg::MappedTextureSubresource mapped;
  context->MapTextureSubresource(staging, 0, 0, dg::MAP_READ,
                                 dg::MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
  if (!mapped.pData) {
    context->UnmapTextureSubresource(staging, 0, 0);
    return nullptr;
  }
  SkBitmap bitmap;
  // The device holds premultiplied RGBA, which is what the target's
  // format says; the conversion to whatever a caller draws onto is
  // Skia's.
  bitmap.allocPixels(SkImageInfo::Make(extent.width(), extent.height(),
                                       kRGBA_8888_SkColorType,
                                       kPremul_SkAlphaType));
  const auto* source = static_cast<const uint8_t*>(mapped.pData);
  for (int y = 0; y < extent.height(); ++y)
    std::memcpy(bitmap.pixmap().writable_addr(0, y),
                source + (size_t)y * mapped.Stride,
                std::min<size_t>(mapped.Stride, bitmap.rowBytes()));
  context->UnmapTextureSubresource(staging, 0, 0);
  bitmap.setImmutable();
  return bitmap.asImage();
}

glm::mat4 clipFor(const ::sigil::geometry::mesh::camera::Camera& camera,
                  SkISize extent) {
  const float aspect = extent.height() > 0
                           ? (float)extent.width() / (float)extent.height()
                           : 1.0f;
  glm::mat4 depth(1.0f);
  depth[2][2] = -0.5f;
  depth[3][2] = 0.5f;
  return depth * camera.projection(aspect) * camera.view();
}

glm::mat4 mapMatrix(const SkMatrix& uv) {
  glm::mat4 out(1.0f);
  out[0] = {uv.getScaleX(), uv.getSkewY(), 0.0f, 0.0f};
  out[1] = {uv.getSkewX(), uv.getScaleY(), 0.0f, 0.0f};
  out[3] = {uv.getTranslateX(), uv.getTranslateY(), 0.0f, 1.0f};
  return out;
}

void openTarget(Gpu& gpu, dg::ITexture* colour, const float* clear,
                bool withDepth) {
  dg::IDeviceContext* context = gpu.device->context();
  dg::ITextureView* rtv =
      colour->GetDefaultView(dg::TEXTURE_VIEW_RENDER_TARGET);
  dg::ITextureView* dsv =
      withDepth && gpu.depth
          ? gpu.depth->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL)
          : nullptr;
  context->SetRenderTargets(1, &rtv, dsv,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearRenderTarget(rtv, clear,
                             dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  if (dsv)
    context->ClearDepthStencil(dsv, dg::CLEAR_DEPTH_FLAG, 1.0f, 0,
                               dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

std::shared_ptr<Gpu> makeGpu(Device& device) {
  auto gpu = std::make_shared<Gpu>(device);

  // ONE SAMPLER PER ANSWER, made once and picked per draw: a texture
  // states how it wants to be read between texels and what lies outside
  // it, and a map that asked for hard texel edges must not have them
  // blended away.
  const auto makeSampler = [&](dg::FILTER_TYPE type,
                               dg::TEXTURE_ADDRESS_MODE address,
                               dg::ISampler** into) {
    dg::SamplerDesc desc;
    desc.MinFilter = type;
    desc.MagFilter = type;
    desc.MipFilter = type;
    desc.AddressU = address;
    desc.AddressV = address;
    desc.AddressW = address;
    device.renderDevice()->CreateSampler(desc, into);
  };
  makeSampler(dg::FILTER_TYPE_LINEAR, dg::TEXTURE_ADDRESS_CLAMP,
              &gpu->linearSampler);
  makeSampler(dg::FILTER_TYPE_POINT, dg::TEXTURE_ADDRESS_CLAMP,
              &gpu->nearestSampler);
  makeSampler(dg::FILTER_TYPE_LINEAR, dg::TEXTURE_ADDRESS_WRAP,
              &gpu->linearTiled);
  makeSampler(dg::FILTER_TYPE_POINT, dg::TEXTURE_ADDRESS_WRAP,
              &gpu->nearestTiled);
  {
    // THE PANORAMA'S SAMPLER, which none of the four above can be: an
    // equirect map's u axis is periodic and its v axis ends at the
    // poles, so the two want different wraps, and its levels are
    // different prefiltered images rather than a filtering aid, so a
    // roughness between two of them has to read across both.
    dg::SamplerDesc desc;
    desc.Name = "world panorama";
    desc.MinFilter = dg::FILTER_TYPE_LINEAR;
    desc.MagFilter = dg::FILTER_TYPE_LINEAR;
    desc.MipFilter = dg::FILTER_TYPE_LINEAR;
    desc.AddressU = dg::TEXTURE_ADDRESS_WRAP;
    desc.AddressV = dg::TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = dg::TEXTURE_ADDRESS_CLAMP;
    desc.MaxLOD = 32;
    device.renderDevice()->CreateSampler(desc, &gpu->panoramaSampler);
  }

  // What an unfilled sampled slot reads: one white texel, so a body
  // multiplied by a map it was not given is the body.
  const uint32_t white = 0xFFFFFFFFu;
  dg::TextureDesc desc;
  desc.Name = "world white";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = 1;
  desc.Height = 1;
  desc.Format = kColorFormat;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::TextureSubResData level{&white, sizeof(white)};
  dg::TextureData data{&level, 1};
  device.renderDevice()->CreateTexture(desc, &data, &gpu->white);
  return gpu;
}

}  // namespace sigil::world::diligent
