// Vulkan sibling of SkiaGraphiteContextMetal.mm: stands Graphite up on the
// VkDevice/VkQueue Qt's QRhi already renders with, so Graphite submissions
// and Qt's render pass are ordered on one queue (the property
// SkiaOffscreenSurface::submit() relies on).
//
// Windows/Linux bring-up draft: this TU never compiles on Apple builds (see
// CMakeLists.txt) and is untested until the Windows port lands. Run the app
// with QSG_RHI_BACKEND=vulkan for this path; under D3D11 create() returns
// null and the renderer falls back to QCanvasPainter.

#include "SkiaGraphiteContext.h"

#include <QtGui/qtguiglobal.h>

#if QT_CONFIG(vulkan)

#include <QVersionNumber>
#include <QVulkanInstance>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/vk/VulkanGraphiteContext.h>
#include <gpu/vk/VulkanBackendContext.h>
// Semi-private, but installed by the vcpkg port and its symbol ships in
// libskia: Skia refuses a null fMemoryAllocator, and this is the only
// public-ish way to reuse the VMA-backed allocator Skia was built with
// (skia[vulkan] carries vulkan-memory-allocator).
#include <src/gpu/vk/vulkanmemoryallocator/VulkanMemoryAllocatorPriv.h>

std::unique_ptr<SkiaGraphiteContext> SkiaGraphiteContext::create(QRhi *rhi) {
  // Single Graphite bring-up point per graphics API (see the Metal TU for
  // the pattern): null for any backend this TU cannot serve.
  if (!rhi || rhi->backend() != QRhi::Vulkan)
    return nullptr;

  const auto *nativeHandles =
      static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
  QVulkanInstance *instance = nativeHandles->inst;
  if (!instance || nativeHandles->physDev == VK_NULL_HANDLE ||
      nativeHandles->dev == VK_NULL_HANDLE ||
      nativeHandles->gfxQueue == VK_NULL_HANDLE)
    return nullptr;

  // Skia resolves every Vulkan entry point through fGetProc. Chain through
  // vkGetDeviceProcAddr for device-level calls so they dispatch without a
  // trampoline. The captured QVulkanInstance is owned by Qt and outlives
  // both the QRhi and this context.
  const auto getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      instance->getInstanceProcAddr("vkGetDeviceProcAddr"));
  if (!getDeviceProcAddr)
    return nullptr;

  skgpu::VulkanBackendContext backendContext;
  backendContext.fInstance = instance->vkInstance();
  backendContext.fPhysicalDevice = nativeHandles->physDev;
  backendContext.fDevice = nativeHandles->dev;
  backendContext.fQueue = nativeHandles->gfxQueue;
  backendContext.fGraphicsQueueIndex = nativeHandles->gfxQueueFamilyIdx;
  const QVersionNumber apiVersion = instance->apiVersion();
  backendContext.fMaxAPIVersion = VK_MAKE_API_VERSION(
      0, apiVersion.majorVersion(), apiVersion.minorVersion(), 0);
  backendContext.fGetProc = [instance, getDeviceProcAddr](
                                const char *name, VkInstance,
                                VkDevice device) -> PFN_vkVoidFunction {
    if (device != VK_NULL_HANDLE)
      return getDeviceProcAddr(device, name);
    return instance->getInstanceProcAddr(name);
  };
  // Qt exposes no list of the device extensions/features it enabled, so
  // none are declared here and Skia assumes the Vulkan 1.1 baseline — a
  // deliberate under-promise that costs optional fast paths, never
  // correctness.

  // Single-threaded (kNo): the context and its one recorder live on the
  // render thread, matching the Metal setup.
  backendContext.fMemoryAllocator = skgpu::VulkanMemoryAllocators::Make(
      backendContext, skgpu::ThreadSafe::kNo);
  if (!backendContext.fMemoryAllocator)
    return nullptr;

  std::unique_ptr<skgpu::graphite::Context> context =
      skgpu::graphite::ContextFactory::MakeVulkan(backendContext, {});
  if (!context)
    return nullptr;

  std::unique_ptr<skgpu::graphite::Recorder> recorder =
      context->makeRecorder(SkiaGraphiteContext::makeRecorderOptions());
  if (!recorder)
    return nullptr;

  return std::unique_ptr<SkiaGraphiteContext>(
      new SkiaGraphiteContext(std::move(context), std::move(recorder)));
}

#else // !QT_CONFIG(vulkan)

// Qt built without Vulkan: no Graphite backend on this platform yet, and
// the renderer falls back to QCanvasPainter.
std::unique_ptr<SkiaGraphiteContext> SkiaGraphiteContext::create(QRhi *) {
  return nullptr;
}

#endif
