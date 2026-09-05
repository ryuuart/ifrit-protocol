/** @file
 * The device side of the executor: textures for the frame's resources,
 * buffers for the meshes a view names, pipelines from compiled programs,
 * and the readback that brings one resource's pixels home.
 */

#include "Gpu.h"

#include <Graphics/GraphicsTools/interface/CommonlyUsedStates.h>

#include <Graphics/GraphicsEngine/interface/GraphicsTypesX.hpp>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::world::diligent {

namespace {

/** HOW A MODE BLENDS, as the engine's own named states.
 *
 *  Colour arrives PREMULTIPLIED — its alpha is already in it — so the
 *  source is taken whole and the mode is decided entirely by what
 *  multiplies the destination, which is exactly the distinction the
 *  three states below draw. A draw that replaces what stands blends
 *  nothing at all, and the default state is the one with blending off.
 *
 *  This mapping is the one thing here the engine cannot spell: an
 *  `SkBlendMode` is Skia's word and no Diligent type names it. */
const dg::BlendStateDesc& blendFor(SkBlendMode mode) {
  switch (mode) {
    case SkBlendMode::kSrc:
      return dg::BS_Default;
    case SkBlendMode::kPlus:
      return dg::BS_AdditiveBlend;
    default:
      return dg::BS_PremultipliedAlphaBlend;
  }
}

}  // namespace

Gpu::~Gpu() {
  // Everything below borrows the device's queue; nothing may be
  // recording when the objects behind it go.
  if (device && device->context()) device->context()->Flush();
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
  meshes.endFrame();
  maps.endFrame();
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

  dg::GraphicsPipelineStateCreateInfoX info{"world pipeline"};
  // DYNAMIC, because every one of these is rebound per draw: one uniform
  // buffer rewritten for each body, and the sampled slots a post stage
  // reads changing between one stage and the next.
  info.PSODesc.ResourceLayout.DefaultVariableType =
      dg::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
  info.AddShader(vs)
      .AddShader(ps)
      .AddRenderTarget(kColorFormat)
      .SetDepthFormat(key.depth ? kDepthFormat : dg::TEX_FORMAT_UNKNOWN)
      .SetPrimitiveTopology(dg::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
      .SetBlendDesc(blendFor(key.blend));

  // Front faces wind counter-clockwise in a y-up space and arrive
  // clockwise after the viewport's flip, which is what this convention
  // calls front — the same reading the CPU executor's cull makes. Where
  // nothing is culled the winding decides nothing, so the no-cull state
  // is taken as it comes.
  info.SetRasterizerDesc(key.fullscreen || !key.cull
                             ? dg::RS_SolidFillNoCull
                             : dg::RS_SolidFillCullBackCCW);

  // THE COMPARISON IS LESS-OR-EQUAL and no named state spells that: the
  // engine's two depth states both compare strictly, and a body redrawn
  // over itself — a variant surface laid on the bodies a selector names —
  // must not lose to the depth it wrote the first time.
  dg::DepthStencilStateDesc depth;
  depth.DepthEnable = key.depth;
  depth.DepthWriteEnable = key.depthWrite;
  depth.DepthFunc = dg::COMPARISON_FUNC_LESS_EQUAL;
  info.SetDepthStencilDesc(depth);

  // THE LAYOUT IS THE RESIDENCY'S, because the buffers are: whoever
  // packs the vertices decides what a pipeline over them declares, and a
  // fullscreen draw declares none at all — it reads no vertex buffer.
  if (!key.fullscreen) {
    const std::span<const dg::LayoutElement> elements =
        ::sigil::geometry::device::meshLayout(key.prim);
    info.SetInputLayout(
        dg::InputLayoutDesc{elements.data(), (dg::Uint32)elements.size()});
  }

  renderDevice->CreateGraphicsPipelineState(info, &built.state);
  if (built.state)
    built.state->CreateShaderResourceBinding(&built.binding, true);
  const auto placed = pipelines.emplace(key, std::move(built)).first;
  return placed->second.state ? &placed->second : nullptr;
}

void bindAndCommit(Gpu& gpu, const Pipeline& pipeline,
                   const material::slang::Compiled& program,
                   const material::slang::Uniforms& values,
                   const std::vector<dg::ITexture*>& textures,
                   SkFilterMode filter, bool tile,
                   bool (*panoramaSlot)(std::string_view)) {
  dg::IDeviceContext* context = gpu.device->context();
  dg::IBuffer* buffer = gpu.shared.uniformBuffer(program.uniformBytes);
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
    if (!texture) texture = gpu.shared.white();
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
    const bool panorama = panoramaSlot && panoramaSlot(program.textures[i]) &&
                          gpu.shared.panoramaSampler() != nullptr;
    view->SetSampler(panorama ? gpu.shared.panoramaSampler()
                              : gpu.shared.samplerFor(filter, tile));
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
  return shared.read(texture);
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
  return std::make_shared<Gpu>(device);
}

}  // namespace sigil::world::diligent
