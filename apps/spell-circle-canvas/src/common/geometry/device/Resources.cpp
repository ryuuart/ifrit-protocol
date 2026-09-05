/** @file
 * The device-wide resources: the samplers and the white texel made once
 * on the device, the uniform buffer grown to what a draw asks for, and
 * the staging copy a readback goes through.
 */

#include "Resources.h"

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>

#include <Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp>
#include <Graphics/GraphicsTools/interface/CommonlyUsedStates.h>
#include <Graphics/GraphicsTools/interface/GraphicsUtilities.h>
#include <algorithm>
#include <cstdint>

namespace sigil::geometry::device {

namespace dg = Diligent;

Resources::Resources(Device& device) : m_device(&device) {
  if (!device.renderDevice()) return;

  // ONE SAMPLER PER ANSWER, made once and picked per draw: a texture
  // states how it wants to be read between texels and what lies outside
  // it, and a map that asked for hard texel edges must not have them
  // blended away. All four are the engine's own named descriptions —
  // one filter on all three stages, one address mode on all three axes —
  // so the four answers are spelled where every renderer on this engine
  // spells them.
  device.renderDevice()->CreateSampler(dg::Sam_LinearClamp, &m_linear);
  device.renderDevice()->CreateSampler(dg::Sam_PointClamp, &m_nearest);
  device.renderDevice()->CreateSampler(dg::Sam_LinearWrap, &m_linearTiled);
  device.renderDevice()->CreateSampler(dg::Sam_PointWrap, &m_nearestTiled);
  {
    // THE PANORAMA'S SAMPLER, which none of the four above can be: an
    // equirect map's u axis is periodic and its v axis ends at the
    // poles, so the two want different wraps, and its levels are
    // different prefiltered images rather than a filtering aid, so a
    // roughness between two of them has to read across both.
    dg::SamplerDesc desc;
    desc.Name = "device panorama";
    desc.MinFilter = dg::FILTER_TYPE_LINEAR;
    desc.MagFilter = dg::FILTER_TYPE_LINEAR;
    desc.MipFilter = dg::FILTER_TYPE_LINEAR;
    desc.AddressU = dg::TEXTURE_ADDRESS_WRAP;
    desc.AddressV = dg::TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = dg::TEXTURE_ADDRESS_CLAMP;
    desc.MaxLOD = 32;
    device.renderDevice()->CreateSampler(desc, &m_panorama);
  }

  // What an unfilled sampled slot reads: one white texel, so a body
  // multiplied by a map it was not given is the body.
  const uint32_t white = 0xFFFFFFFFu;
  dg::TextureDesc desc;
  desc.Name = "device white";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = 1;
  desc.Height = 1;
  desc.Format = kColorFormat;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::TextureSubResData level{&white, sizeof(white)};
  dg::TextureData data{&level, 1};
  device.renderDevice()->CreateTexture(desc, &data, &m_white);
}

dg::IBuffer* Resources::uniformBuffer(size_t bytes) {
  const size_t wanted = std::max<size_t>(bytes, 256);
  if (m_uniforms && m_uniformCapacity >= wanted) return m_uniforms;
  m_uniforms.Release();
  // Dynamic and CPU-writable, which is what a buffer rewritten once per
  // draw is; those are the engine's own defaults for a uniform buffer,
  // so they are not restated here.
  dg::CreateUniformBuffer(m_device->renderDevice(), wanted,
                          "device draw uniforms", &m_uniforms);
  m_uniformCapacity = m_uniforms ? wanted : 0;
  return m_uniforms;
}

dg::ISampler* Resources::samplerFor(SkFilterMode filter, bool tile) const {
  if (filter == SkFilterMode::kNearest)
    return tile ? m_nearestTiled.RawPtr() : m_nearest.RawPtr();
  return tile ? m_linearTiled.RawPtr() : m_linear.RawPtr();
}

sk_sp<SkImage> Resources::read(dg::ITexture* texture) {
  if (!texture) return nullptr;
  // The size is the TEXTURE'S, not a frame's: a readback of something
  // smaller than the frame is still a readback of the whole of it.
  const int width = (int)texture->GetDesc().Width;
  const int height = (int)texture->GetDesc().Height;
  if (width <= 0 || height <= 0) return nullptr;

  dg::TextureDesc desc;
  desc.Name = "device readback";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)width;
  desc.Height = (dg::Uint32)height;
  desc.Format = kColorFormat;
  desc.Usage = dg::USAGE_STAGING;
  desc.CPUAccessFlags = dg::CPU_ACCESS_READ;
  desc.BindFlags = dg::BIND_NONE;
  dg::RefCntAutoPtr<dg::ITexture> staging;
  m_device->renderDevice()->CreateTexture(desc, nullptr, &staging);
  if (!staging) return nullptr;

  // No queue lock here, and none anywhere in an executor on this device:
  // Diligent takes that lock from inside its own submissions, and this
  // path only issues Diligent commands. The lock belongs to a caller
  // mixing Graphite's submissions into the same stream.
  dg::IDeviceContext* context = m_device->context();
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
  bitmap.allocPixels(SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                       kPremul_SkAlphaType));
  // THE TWO STRIDES DIFFER: what the device mapped is padded to its own
  // alignment and what Skia allocated is not, so the rows are copied one
  // at a time rather than the block at once.
  dg::CopyTextureSubresource(
      dg::TextureSubResData{mapped.pData, mapped.Stride}, (dg::Uint32)height,
      /*NumDepthSlices=*/1,
      std::min<dg::Uint64>(mapped.Stride, bitmap.rowBytes()),
      bitmap.getPixels(), (dg::Uint64)bitmap.rowBytes(),
      /*DstDepthStride=*/0);
  context->UnmapTextureSubresource(staging, 0, 0);
  bitmap.setImmutable();
  return bitmap.asImage();
}

}  // namespace sigil::geometry::device
