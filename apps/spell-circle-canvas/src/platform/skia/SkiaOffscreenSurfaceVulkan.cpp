// Vulkan sibling of SkiaOffscreenSurfaceMetal.mm: wraps the VkImage behind
// a QRhiTexture in an SkSurface on the SkiaGraphiteContextVulkan.cpp
// context, so SkCanvas draws land directly in Qt's texture.
//
// Windows/Linux bring-up draft: never compiled on Apple builds (see
// CMakeLists.txt), untested until the Windows port lands.

#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"

#include <QtGui/qtguiglobal.h>

#if QT_CONFIG(vulkan)

#include <rhi/qrhi.h>

#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/vk/VulkanGraphiteTypes.h>
#include <include/gpu/vk/VulkanTypes.h>

SkiaOffscreenSurface::SkiaOffscreenSurface(SkiaGraphiteContext &context,
                                           QRhiTexture *texture,
                                           QSize pixelSize)
    : m_context(context) {
  if (!texture)
    return;

  // QRhi packs the VkImage into nativeTexture().object and the image's
  // current layout into .layout.
  const QRhiTexture::NativeTexture native = texture->nativeTexture();
  if (!native.object)
    return;

  skgpu::graphite::VulkanTextureInfo info;
  // The offscreen canvas is QRhiTexture::RGBA8; QRhi maps it to
  // R8G8B8A8_UNORM on Vulkan.
  info.fFormat = VK_FORMAT_R8G8B8A8_UNORM;
  info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
  // Mirrors the usage QRhi requests for a RenderTarget texture; Skia
  // validates these against what it needs (color attachment to draw,
  // transfer for clears/readbacks). Verify against qrhivulkan.cpp at
  // Windows bring-up.
  info.fImageUsageFlags =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  info.fSharingMode = VK_SHARING_MODE_EXCLUSIVE;

  const skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeVulkan(
          SkISize::Make(pixelSize.width(), pixelSize.height()), info,
          static_cast<VkImageLayout>(native.layout),
          // Exclusive-mode image used on the one graphics queue Graphite
          // shares with Qt: no queue-family ownership transfer to track.
          VK_QUEUE_FAMILY_IGNORED,
          reinterpret_cast<VkImage>(static_cast<uintptr_t>(native.object)),
          // Qt owns the backing VkDeviceMemory; an empty alloc marks the
          // texture as externally managed so Skia never frees it.
          skgpu::VulkanAlloc{});

  m_surface = SkSurfaces::WrapBackendTexture(context.recorder(), backendTexture,
                                             /*colorSpace=*/nullptr,
                                             /*props=*/nullptr);
}

#else // !QT_CONFIG(vulkan)

// Qt built without Vulkan: create() already returned null, so no
// SkiaGraphiteContext exists to construct a surface from; this definition
// only satisfies the linker.
SkiaOffscreenSurface::SkiaOffscreenSurface(SkiaGraphiteContext &context,
                                           QRhiTexture *, QSize)
    : m_context(context) {}

#endif
