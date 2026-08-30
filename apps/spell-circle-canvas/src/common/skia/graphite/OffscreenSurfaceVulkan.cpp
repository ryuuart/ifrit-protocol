// Vulkan wrap of OffscreenSurface: a VkImage as an SkSurface on a Vulkan
// GraphiteContext, so SkCanvas draws land directly in the caller's image.
// Compiles on every platform, with the body present only when the linked
// Skia carries the backend (SK_VULKAN).

#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#ifdef SK_VULKAN

#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/vk/VulkanGraphiteTypes.h>
#include <include/gpu/vk/VulkanTypes.h>

namespace sigil::skia {

OffscreenSurface::OffscreenSurface(GraphiteContext& context,
                                   const VulkanImage& image)
    : m_context(&context) {
  if (!image.image) return;

  skgpu::graphite::VulkanTextureInfo info;
  info.fFormat = static_cast<VkFormat>(image.format);
  info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
  // What the wrap promises about the image; Skia validates these against
  // what it needs (colour attachment to draw, transfer for clears and
  // readbacks), so an image created with less fails the wrap rather than
  // the draw.
  info.fImageUsageFlags =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  info.fSharingMode = VK_SHARING_MODE_EXCLUSIVE;

  const skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeVulkan(
          SkISize::Make(image.width, image.height), info,
          static_cast<VkImageLayout>(image.layout),
          // Exclusive-mode image used on the one graphics queue Graphite
          // shares with the host: no queue-family ownership transfer to
          // track.
          VK_QUEUE_FAMILY_IGNORED,
          reinterpret_cast<VkImage>(static_cast<uintptr_t>(image.image)),
          // The caller owns the backing VkDeviceMemory; an empty alloc
          // marks the texture as externally managed so Skia never frees
          // it.
          skgpu::VulkanAlloc{});

  m_surface = SkSurfaces::WrapBackendTexture(context.recorder(), backendTexture,
                                             /*colorSpace=*/nullptr,
                                             /*props=*/nullptr);
}

}  // namespace sigil::skia

#else  // !SK_VULKAN

namespace sigil::skia {

// A Skia without the Vulkan backend never made a context to wrap on;
// the surface stays empty and canvas() null.
OffscreenSurface::OffscreenSurface(GraphiteContext& context, const VulkanImage&)
    : m_context(&context) {}

}  // namespace sigil::skia

#endif
