// The Metal objects behind this device's Vulkan ones, on a driver that
// runs Vulkan over Metal and exports what it made; and, on a driver that
// imports them, the Vulkan image over a texture Metal made and the
// timeline semaphore over an event a queue on Metal raises.
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
#include <algorithm>
#include <cstdint>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>

#include "MetalObjects.h"
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

VkFormat vkFormatOf(core::hardware::TextureFormat format) {
  switch (format) {
    case core::hardware::TextureFormat::RGBA8Unorm:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case core::hardware::TextureFormat::BGRA8Unorm:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case core::hardware::TextureFormat::RGBA16Float:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
  }
  return VK_FORMAT_R8G8B8A8_UNORM;
}

/** WHAT AN IMPORTED IMAGE MAY BE USED FOR: no more than the usage Metal
 *  made the texture with. The driver derives a Metal usage from the
 *  Vulkan one, and a texture that lacks what was derived refuses the
 *  operation that needed it. Every texture is sampled and copied both
 *  ways — a copy needs no Metal usage — and the rest follows the
 *  description: a render target can be drawn into, and is what the
 *  smaller levels of a chain are derived through; a texture written by
 *  shaders can be stored into. */
VkImageUsageFlags importedUsage(core::hardware::TextureUsage usage) {
  using core::hardware::TextureUsage;
  VkImageUsageFlags flags = VK_IMAGE_USAGE_SAMPLED_BIT |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (core::hardware::has(usage, TextureUsage::RenderTarget))
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (core::hardware::has(usage, TextureUsage::ShaderWrite))
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  return flags;
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
  if (!shared || shared->backend() != core::hardware::Backend::Vulkan)
    return nullptr;
  // On Vulkan a fence IS a timeline semaphore, and that is what the
  // driver is asked about.
  auto* semaphore = static_cast<VkSemaphore>(shared->exportNative(fence));
  if (!semaphore) return nullptr;
  const auto exportObjects = exportOf(renderDevice());
  if (!exportObjects) return nullptr;
  VkExportMetalSharedEventInfoEXT wanted{};
  wanted.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_SHARED_EVENT_INFO_EXT;
  wanted.semaphore = semaphore;
  VkExportMetalObjectsInfoEXT objects{};
  objects.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT;
  objects.pNext = &wanted;
  exportObjects(vkDeviceOf(renderDevice()), &objects);
  return (void*)wanted.mtlSharedEvent;
}

void* Device::metalTexture(core::hardware::TextureHandle texture) const {
  core::hardware::GpuDevice* const shared = gpu();
  if (!shared || shared->backend() != core::hardware::Backend::Vulkan)
    return nullptr;
  const core::hardware::NativeTexture native = shared->exportNative(texture);
  if (!native.vkImage) return nullptr;
  const auto exportObjects = exportOf(renderDevice());
  if (!exportObjects) return nullptr;
  VkExportMetalTextureInfoEXT wanted{};
  wanted.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT;
  // The image travels as the 64-bit word Vulkan defines a handle to be.
  wanted.image =
      reinterpret_cast<VkImage>(  // NOLINT(performance-no-int-to-ptr)
          native.vkImage);
  wanted.plane = VK_IMAGE_ASPECT_PLANE_0_BIT;
  VkExportMetalObjectsInfoEXT objects{};
  objects.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT;
  objects.pNext = &wanted;
  exportObjects(vkDeviceOf(renderDevice()), &objects);
  return (void*)wanted.mtlTexture;
}

core::hardware::TextureHandle Device::importMetalTexture(
    void* mtlTexture,
    const core::hardware::TextureDescription& description) const {
  core::hardware::GpuDevice* const shared = gpu();
  if (!mtlTexture || !shared ||
      shared->backend() != core::hardware::Backend::Vulkan ||
      description.width <= 0 || description.height <= 0)
    return {};
  // The import is the export's extension read the other way, so a device
  // that cannot export cannot import either.
  if (!exportOf(renderDevice())) return {};
  const VkDevice device = vkDeviceOf(renderDevice());
  const auto createImage = reinterpret_cast<PFN_vkCreateImage>(
      vkGetDeviceProcAddr(device, "vkCreateImage"));
  if (!createImage) return {};

  // THE TEXTURE IS THE IMAGE'S MEMORY. The driver takes the texture as
  // the image's backing in place of memory bound to it, so the image is
  // created and nothing is allocated; its size, format and levels are
  // the texture's own and are stated here only because a Vulkan image
  // is created from a description.
  VkImportMetalTextureInfoEXT imported{};
  imported.sType = VK_STRUCTURE_TYPE_IMPORT_METAL_TEXTURE_INFO_EXT;
  imported.plane = VK_IMAGE_ASPECT_PLANE_0_BIT;
  imported.mtlTexture = (MTLTexture_id)mtlTexture;
  // Vulkan's create-info structs are zero-filled and then written field
  // by field; every field this call depends on, `samples` included, is
  // given its value below.
  // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
  VkImageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = &imported;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = vkFormatOf(description.format);
  info.extent = {(uint32_t)description.width, (uint32_t)description.height, 1};
  const int possible =
      core::hardware::mipLevelsFor(description.width, description.height);
  info.mipLevels = (uint32_t)std::clamp(description.mipLevels, 1, possible);
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = importedUsage(description.usage);
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImage image = VK_NULL_HANDLE;
  if (createImage(device, &info, nullptr, &image) != VK_SUCCESS) return {};

  // Named with ownership, so the handle's destroy is the image's; there
  // is no memory of this device's behind it to free.
  core::hardware::NativeTexture native;
  native.backend = core::hardware::Backend::Vulkan;
  native.vkImage = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(image));
  native.vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  native.vkFormat = info.format;
  native.width = description.width;
  native.height = description.height;
  native.mipLevels = (int)info.mipLevels;
  return shared->importNative(native, /*takeOwnership=*/true);
}

MetalEvents::MetalEvents(Diligent::IRenderDevice* device) : m_device(device) {}

MetalEvents::~MetalEvents() {
  const VkDevice device = vkDeviceOf(m_device);
  const auto destroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(
      device ? vkGetDeviceProcAddr(device, "vkDestroySemaphore") : nullptr);
  if (!destroySemaphore) return;
  for (const Brought& brought : m_brought)
    destroySemaphore(
        device,
        reinterpret_cast<VkSemaphore>(  // NOLINT(performance-no-int-to-ptr)
            brought.semaphore),
        nullptr);
}

bool MetalEvents::wait(void* queue, void* mtlSharedEvent, uint64_t value) {
  if (!queue || !mtlSharedEvent) return false;
  // The import is the export's extension read the other way, so a device
  // that cannot export cannot import either.
  if (!exportOf(m_device)) return false;
  const VkDevice device = vkDeviceOf(m_device);
  const auto createSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(
      vkGetDeviceProcAddr(device, "vkCreateSemaphore"));
  const auto queueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
      vkGetDeviceProcAddr(device, "vkQueueSubmit"));
  if (!createSemaphore || !queueSubmit) return false;

  VkSemaphore semaphore = VK_NULL_HANDLE;
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    for (const Brought& brought : m_brought)
      if (brought.event == mtlSharedEvent)
        semaphore =
            reinterpret_cast<VkSemaphore>(  // NOLINT(performance-no-int-to-ptr)
                brought.semaphore);
    if (!semaphore) {
      // A TIMELINE OVER THE VERY SAME EVENT: the driver takes the event as
      // the semaphore's payload, so the value the other queue raises it to
      // is the value a wait here reads.
      VkImportMetalSharedEventInfoEXT imported{};
      imported.sType = VK_STRUCTURE_TYPE_IMPORT_METAL_SHARED_EVENT_INFO_EXT;
      imported.mtlSharedEvent = (MTLSharedEvent_id)mtlSharedEvent;
      VkSemaphoreTypeCreateInfo type{};
      type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
      type.pNext = &imported;
      type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
      VkSemaphoreCreateInfo info{};
      info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      info.pNext = &type;
      if (createSemaphore(device, &info, nullptr, &semaphore) != VK_SUCCESS)
        return false;
      m_brought.push_back(
          {mtlSharedEvent,
           static_cast<uint64_t>(reinterpret_cast<uintptr_t>(semaphore))});
    }
  }

  // An empty submission that waits: queued behind everything submitted
  // so far and ahead of everything after.
  VkTimelineSemaphoreSubmitInfo timeline{};
  timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timeline.waitSemaphoreValueCount = 1;
  timeline.pWaitSemaphoreValues = &value;
  const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = &timeline;
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &semaphore;
  submit.pWaitDstStageMask = &stage;
  return queueSubmit(static_cast<VkQueue>(queue), 1, &submit, VK_NULL_HANDLE) ==
         VK_SUCCESS;
}

}  // namespace sigil::geometry::device
