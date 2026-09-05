/** @file
 * Building a pipeline from a compiled program, writing a draw's uniforms
 * and sampled slots into its binding, and opening the target a stage
 * draws onto.
 */

#include "sigilgeometry/device/Pipelines.h"

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsTools/interface/CommonlyUsedStates.h>
#include <sigilgeometry/device/Meshes.h>

#include <Graphics/GraphicsEngine/interface/GraphicsTypesX.hpp>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <cstddef>
#include <cstring>
#include <utility>

namespace sigil::geometry::device {

namespace dg = Diligent;

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

PipelineCache::PipelineCache(Device& device) : m_device(&device) {}

const Pipeline* PipelineCache::pipeline(const PipelineKey& key) {
  if (!key.program || key.program->empty()) return nullptr;
  const auto found = m_pipelines.find(key);
  if (found != m_pipelines.end())
    return found->second.state ? &found->second : nullptr;

  dg::IRenderDevice* renderDevice = m_device->renderDevice();
  Pipeline built;
  dg::RefCntAutoPtr<dg::IShader> vs;
  dg::RefCntAutoPtr<dg::IShader> ps;
  {
    dg::ShaderCreateInfo ci;
    ci.Desc.Name = "device vertex";
    ci.Desc.ShaderType = dg::SHADER_TYPE_VERTEX;
    ci.Desc.UseCombinedTextureSamplers = true;
    ci.ByteCode = key.program->vertex.data();
    ci.ByteCodeSize = key.program->vertex.size() * sizeof(uint32_t);
    renderDevice->CreateShader(ci, &vs);
  }
  {
    dg::ShaderCreateInfo ci;
    ci.Desc.Name = "device fragment";
    ci.Desc.ShaderType = dg::SHADER_TYPE_PIXEL;
    ci.Desc.UseCombinedTextureSamplers = true;
    ci.ByteCode = key.program->fragment.data();
    ci.ByteCodeSize = key.program->fragment.size() * sizeof(uint32_t);
    renderDevice->CreateShader(ci, &ps);
  }
  if (!vs || !ps) {
    m_pipelines.emplace(key, Pipeline{});
    return nullptr;
  }

  dg::GraphicsPipelineStateCreateInfoX info{"device pipeline"};
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

  // The layout is the residency's, because the buffers are. A fullscreen
  // draw declares none: it reads no vertex buffer.
  if (!key.fullscreen) {
    const std::span<const dg::LayoutElement> elements = meshLayout(key.prim);
    info.SetInputLayout(
        dg::InputLayoutDesc{elements.data(), (dg::Uint32)elements.size()});
  }

  renderDevice->CreateGraphicsPipelineState(info, &built.state);
  if (built.state)
    built.state->CreateShaderResourceBinding(&built.binding, true);
  const auto placed = m_pipelines.emplace(key, std::move(built)).first;
  return placed->second.state ? &placed->second : nullptr;
}

void bindDraw(Resources& shared, const Pipeline& pipeline,
              const material::slang::Compiled& program,
              const material::slang::Uniforms& values,
              std::span<dg::ITexture* const> textures, SkFilterMode filter,
              bool tile, bool (*panoramaSlot)(std::string_view)) {
  dg::IDeviceContext* context = shared.device().context();
  dg::IBuffer* buffer = shared.uniformBuffer(program.uniformBytes);
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
    if (!texture) texture = shared.white();
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
                          shared.panoramaSampler() != nullptr;
    view->SetSampler(panorama ? shared.panoramaSampler()
                              : shared.samplerFor(filter, tile));
    if (dg::IShaderResourceVariable* variable =
            pipeline.binding->GetVariableByName(dg::SHADER_TYPE_PIXEL,
                                                program.textures[i].c_str()))
      variable->Set(view);
  }
  context->CommitShaderResources(pipeline.binding,
                                 dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void openTarget(Device& device, dg::ITexture* colour, dg::ITexture* depth,
                const float* clear) {
  dg::IDeviceContext* context = device.context();
  dg::ITextureView* rtv =
      colour->GetDefaultView(dg::TEXTURE_VIEW_RENDER_TARGET);
  dg::ITextureView* dsv =
      depth ? depth->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL) : nullptr;
  context->SetRenderTargets(1, &rtv, dsv,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearRenderTarget(rtv, clear,
                             dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  if (dsv)
    context->ClearDepthStencil(dsv, dg::CLEAR_DEPTH_FLAG, 1.0f, 0,
                               dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

}  // namespace sigil::geometry::device
