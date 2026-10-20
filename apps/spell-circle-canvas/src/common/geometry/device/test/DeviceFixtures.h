#pragma once

/** @file
 * What the device cases share: a clear submitted through Diligent and the
 * small texture the adopted-device cases make, beside the Graphite context
 * on the device's Vulkan queue that `VulkanGraphite.h` holds. Each is
 * inline so a case in any file of the binary reaches the same one, the
 * kept Vulkan context among them.
 */

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>

#include <Common/interface/RefCntAutoPtr.hpp>

#include "VulkanGraphite.h"

namespace sigil::geometry::device::test {

/** A clear of a fresh render target through Diligent, submitted and
 *  waited out. False when the target could not be made. */
inline bool clearThroughDiligent(geometry::device::Device& device) {
  using namespace Diligent;
  TextureDesc desc;
  desc.Name = "diligent still drives";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = 8;
  desc.Height = 8;
  desc.MipLevels = 1;
  desc.Format = TEX_FORMAT_RGBA8_UNORM;
  desc.BindFlags = BIND_RENDER_TARGET;
  RefCntAutoPtr<ITexture> target;
  device.renderDevice()->CreateTexture(desc, nullptr, &target);
  if (!target) return false;
  ITextureView* view = target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  if (!view) return false;
  IDeviceContext* context = device.context();
  ITextureView* views[] = {view};
  context->SetRenderTargets(1, views,
                            /*pDepthStencil=*/nullptr,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float green[] = {0, 1, 0, 1};
  context->ClearRenderTarget(view, green,
                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->Flush();
  context->WaitForIdle();
  return true;
}

/** An 8x8 texture description the adopted-device cases make and destroy. */
inline core::hardware::TextureDescription smallTexture() {
  core::hardware::TextureDescription desc;
  desc.width = 8;
  desc.height = 8;
  desc.label = "geometry_device_test";
  return desc;
}

}  // namespace sigil::geometry::device::test
