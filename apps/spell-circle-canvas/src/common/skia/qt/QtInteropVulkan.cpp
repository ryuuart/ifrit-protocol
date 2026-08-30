// Qt -> Vulkan adapter over the Qt-free Graphite bring-up: extracts the
// VkInstance, device, queue and image a QRhi is built on and forwards to
// GraphiteContext::createVulkan() and the Vulkan OffscreenSurface
// constructor. Qt keeps ownership of every handle. Run the host with
// QSG_RHI_BACKEND=vulkan for this path; under any other QRhi backend the
// factory returns null.
//
// This path has no host that exercises it yet: it builds, and the first
// consumer with a Vulkan device is its test.

#include <QtGui/qtguiglobal.h>
#include <sigilskia/qt/QtInterop.h>

#include <QSize>

#if QT_CONFIG(vulkan)

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <QVersionNumber>
#include <QVulkanInstance>

namespace sigil::skia {

std::unique_ptr<GraphiteContext> createGraphiteContext(QRhi* rhi) {
  // One graphics API per build: null for any backend this adapter cannot
  // serve, so callers never assume Vulkan.
  if (!rhi || rhi->backend() != QRhi::Vulkan) return nullptr;

  const auto* nativeHandles =
      static_cast<const QRhiVulkanNativeHandles*>(rhi->nativeHandles());
  QVulkanInstance* instance = nativeHandles->inst;
  if (!instance) return nullptr;

  VulkanHandles handles;
  handles.instance = instance->vkInstance();
  handles.physicalDevice = nativeHandles->physDev;
  handles.device = nativeHandles->dev;
  handles.queue = nativeHandles->gfxQueue;
  handles.queueFamilyIndex = nativeHandles->gfxQueueFamilyIdx;
  const QVersionNumber apiVersion = instance->apiVersion();
  handles.apiVersion = VK_MAKE_API_VERSION(0, apiVersion.majorVersion(),
                                           apiVersion.minorVersion(), 0);
  // The loader's own vkGetInstanceProcAddr, queried from itself, so the
  // Qt-free path resolves every entry point without a Qt object in the
  // chain.
  handles.getInstanceProcAddr =
      reinterpret_cast<VulkanHandles::GetInstanceProcAddr>(
          instance->getInstanceProcAddr("vkGetInstanceProcAddr"));
  return GraphiteContext::createVulkan(handles);
}

OffscreenSurface wrapTexture(GraphiteContext& context, QRhiTexture* texture,
                             QSize pixelSize) {
  VulkanImage image;
  if (texture) {
    // QRhi packs the VkImage into nativeTexture().object and the image's
    // current layout into .layout.
    const QRhiTexture::NativeTexture native = texture->nativeTexture();
    image.image = native.object;
    image.layout = static_cast<uint32_t>(native.layout);
    // A QRhiTexture::RGBA8 texture, which QRhi maps to R8G8B8A8_UNORM on
    // Vulkan.
    image.format = VK_FORMAT_R8G8B8A8_UNORM;
    image.width = pixelSize.width();
    image.height = pixelSize.height();
  }
  return OffscreenSurface(context, image);
}

}  // namespace sigil::skia

#else  // !QT_CONFIG(vulkan)

namespace sigil::skia {

// Qt built without Vulkan: no Graphite backend to reach through this
// adapter, so the factory returns null and the wrap stays empty.
std::unique_ptr<GraphiteContext> createGraphiteContext(QRhi*) {
  return nullptr;
}

OffscreenSurface wrapTexture(GraphiteContext& context, QRhiTexture*, QSize) {
  return OffscreenSurface(context, VulkanImage{});
}

}  // namespace sigil::skia

#endif
