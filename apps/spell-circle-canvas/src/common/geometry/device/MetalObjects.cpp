// The Metal objects behind this device's Vulkan ones, on a driver that
// runs Vulkan over Metal and exports what it made. A texture the engine
// made is asked of the driver here; everything named on the adopted
// device answers its own through `GpuDevice::exportNative`.
//
// The same include order as AdoptDevice.cpp, for the same reason: volk
// declares every entry point as a pointer, and the Diligent interfaces
// below name Vulkan types without a header of their own.
// clang-format off
#include <volk.h>
#include <vulkan/vulkan_metal.h>

#include <Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>
#include <Graphics/GraphicsEngineVulkan/interface/TextureVk.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
// clang-format on

namespace sigil::geometry::device {

namespace {

/** The export, read once per device off the device itself: an entry
 *  point a device was not created with the extension for is null, and
 *  so is every answer. */
PFN_vkExportMetalObjectsEXT exportOf(Diligent::IRenderDevice* device) {
  Diligent::RefCntAutoPtr<Diligent::IRenderDeviceVk> vk{
      device, Diligent::IID_RenderDeviceVk};
  if (!vk || !vkGetDeviceProcAddr) return nullptr;
  return reinterpret_cast<PFN_vkExportMetalObjectsEXT>(
      vkGetDeviceProcAddr(vk->GetVkDevice(), "vkExportMetalObjectsEXT"));
}

VkDevice vkDeviceOf(Diligent::IRenderDevice* device) {
  Diligent::RefCntAutoPtr<Diligent::IRenderDeviceVk> vk{
      device, Diligent::IID_RenderDeviceVk};
  return vk ? vk->GetVkDevice() : VK_NULL_HANDLE;
}

}  // namespace

void* Device::metalTexture(Diligent::ITexture* texture) const {
  if (!texture) return nullptr;
  const auto exportObjects = exportOf(renderDevice());
  if (!exportObjects) return nullptr;
  Diligent::RefCntAutoPtr<Diligent::ITextureVk> vk{texture,
                                                   Diligent::IID_TextureVk};
  if (!vk) return nullptr;
  VkExportMetalTextureInfoEXT wanted{};
  wanted.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT;
  wanted.image = vk->GetVkImage();
  wanted.plane = VK_IMAGE_ASPECT_PLANE_0_BIT;
  VkExportMetalObjectsInfoEXT objects{};
  objects.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT;
  objects.pNext = &wanted;
  exportObjects(vkDeviceOf(renderDevice()), &objects);
  return (void*)wanted.mtlTexture;
}

void* Device::metalEvent(core::hardware::FenceHandle fence) const {
  core::hardware::GpuDevice* const shared = gpu();
  return shared ? shared->exportNative(fence).mtlSharedEvent : nullptr;
}

}  // namespace sigil::geometry::device
