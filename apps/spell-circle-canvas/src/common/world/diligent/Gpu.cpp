/** @file
 * The device side of the executor: textures for the frame's resources,
 * buffers for the meshes a view names, pipelines from compiled programs,
 * and the readback that brings one resource's pixels home.
 */

#include "Gpu.h"

#include <Graphics/GraphicsTools/interface/CommonlyUsedStates.h>
#include <sigilworld/advanced/Skia.h>

#include <Graphics/GraphicsEngine/interface/GraphicsTypesX.hpp>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <algorithm>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::world::diligent {

Gpu::~Gpu() {
  // Everything below borrows the device's queue; nothing may be
  // recording when the objects behind it go.
  if (device && device->context()) device->context()->Flush();
}

dg::RefCntAutoPtr<dg::ITexture> Gpu::makeColor(const char* label,
                                               dg::TEXTURE_FORMAT format) {
  dg::RefCntAutoPtr<dg::ITexture> texture;
  if (extent.isEmpty() || !device->renderDevice()) return texture;
  dg::TextureDesc desc;
  desc.Name = label;
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)extent.width();
  desc.Height = (dg::Uint32)extent.height();
  desc.Format = format;
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
  opaqueDepthView.Release();
  opaqueDepth.Release();
  sceneColor.Release();
  display.Release();
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

dg::ITexture* Gpu::depthForReading() {
  if (opaqueDepth || extent.isEmpty() || !device->renderDevice())
    return opaqueDepth;
  dg::TextureDesc desc;
  desc.Name = "world opaque depth";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)extent.width();
  desc.Height = (dg::Uint32)extent.height();
  desc.Format = kDepthFormat;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_DEFAULT;
  device->renderDevice()->CreateTexture(desc, nullptr, &opaqueDepth);
  if (!opaqueDepth) return nullptr;
  dg::TextureViewDesc view;
  view.ViewType = dg::TEXTURE_VIEW_SHADER_RESOURCE;
  view.Format = kDepthFormat;
  opaqueDepth->CreateView(view, &opaqueDepthView);
  if (!opaqueDepthView) opaqueDepth.Release();
  return opaqueDepth;
}

dg::ITexture* Gpu::sceneColorForReading() {
  if (sceneColor || extent.isEmpty() || !device->renderDevice())
    return sceneColor;
  dg::TextureDesc desc;
  desc.Name = "world scene colour";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)extent.width();
  desc.Height = (dg::Uint32)extent.height();
  desc.Format = kRadianceFormat;
  // Every level down to one texel, so the roughest surface reads the
  // average of the whole picture.
  dg::Uint32 levels = 1;
  for (int side = std::max(extent.width(), extent.height()); side > 1;
       side /= 2)
    ++levels;
  desc.MipLevels = levels;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE | dg::BIND_RENDER_TARGET;
  desc.MiscFlags = dg::MISC_TEXTURE_FLAG_GENERATE_MIPS;
  desc.Usage = dg::USAGE_DEFAULT;
  device->renderDevice()->CreateTexture(desc, nullptr, &sceneColor);
  return sceneColor;
}

sk_sp<SkImage> Gpu::read(std::string_view name) {
  dg::ITexture* texture = current(name);
  if (!texture) texture = previous(name);
  if (!texture) return nullptr;
  // A RESOURCE LEAVES THE FRAME THROUGH THE FINAL STAGE: what the passes
  // hold is light, and what a display or a readback is handed is that
  // light as the camera reads it.
  if (!display) display = makeColor("world display", kColorFormat);
  if (!display) return nullptr;
  finalStage(*this, texture, display);
  return shared.read(display);
}

glm::mat4 mapMatrix(const SkMatrix& uv) {
  glm::mat4 out(1.0f);
  out[0] = {uv.getScaleX(), uv.getSkewY(), 0.0f, 0.0f};
  out[1] = {uv.getSkewX(), uv.getScaleY(), 0.0f, 0.0f};
  out[3] = {uv.getTranslateX(), uv.getTranslateY(), 0.0f, 1.0f};
  return out;
}

void resumeTarget(Gpu& gpu, dg::ITexture* colour, bool withDepth) {
  dg::ITextureView* rtv =
      colour->GetDefaultView(dg::TEXTURE_VIEW_RENDER_TARGET);
  dg::ITextureView* dsv =
      withDepth && gpu.depth
          ? gpu.depth->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL)
          : nullptr;
  gpu.device->context()->SetRenderTargets(
      1, &rtv, dsv, dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

float curveNumber(ToneCurve curve) {
  switch (curve) {
    case ToneCurve::Reinhard:
      return 0.0f;
    case ToneCurve::Neutral:
      return 1.0f;
    case ToneCurve::Linear:
      return 2.0f;
  }
  return 0.0f;
}

void openTarget(Gpu& gpu, dg::ITexture* colour, const float* clear,
                bool withDepth) {
  ::sigil::geometry::device::openTarget(
      *gpu.device, colour, withDepth ? gpu.depth.RawPtr() : nullptr, clear);
}

std::shared_ptr<Gpu> makeGpu(Device& device) {
  return std::make_shared<Gpu>(device);
}

}  // namespace sigil::world::diligent
