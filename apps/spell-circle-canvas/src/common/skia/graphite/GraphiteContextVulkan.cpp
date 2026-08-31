// Vulkan bring-up of GraphiteContext: Graphite on a VkDevice and VkQueue
// the caller owns, so Graphite submissions and the caller's own work are
// ordered on that one queue (the property OffscreenSurface::submit()
// relies on). Compiles on every platform; whether it can ever run is
// decided by the Skia this build links, which defines SK_VULKAN when it
// carries the backend.
//
// This path has no host that exercises it yet: it builds, and the first
// consumer with a Vulkan device is its test.

#include <sigilskia/graphite/GraphiteContext.h>

#ifdef SK_VULKAN

#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/vk/VulkanGraphiteContext.h>
#include <gpu/vk/VulkanBackendContext.h>
// Semi-private, but installed by the vcpkg port and its symbol ships in
// libskia: Skia refuses a null fMemoryAllocator, and this is the only
// public-ish way to reuse the VMA-backed allocator Skia was built with
// (skia[vulkan] carries vulkan-memory-allocator). The ThreadSafe
// enumerator it takes lives beside it.
#include <src/gpu/GpuTypesPriv.h>
#include <src/gpu/vk/vulkanmemoryallocator/VulkanMemoryAllocatorPriv.h>

namespace sigil::skia {

std::unique_ptr<GraphiteContext> GraphiteContext::createVulkan(
    const VulkanHandles& handles) {
  if (!handles.instance || !handles.physicalDevice || !handles.device ||
      !handles.queue || !handles.getInstanceProcAddr)
    return nullptr;

  const auto instance = static_cast<VkInstance>(handles.instance);
  const auto getInstanceProcAddr = handles.getInstanceProcAddr;

  // Skia resolves every Vulkan entry point through fGetProc. Chain through
  // vkGetDeviceProcAddr for device-level calls so they dispatch without a
  // trampoline.
  const auto getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      getInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
  if (!getDeviceProcAddr) return nullptr;

  skgpu::VulkanBackendContext backendContext;
  backendContext.fInstance = instance;
  backendContext.fPhysicalDevice =
      static_cast<VkPhysicalDevice>(handles.physicalDevice);
  backendContext.fDevice = static_cast<VkDevice>(handles.device);
  backendContext.fQueue = static_cast<VkQueue>(handles.queue);
  backendContext.fGraphicsQueueIndex = handles.queueFamilyIndex;
  backendContext.fMaxAPIVersion = handles.apiVersion;
  backendContext.fGetProc = [getInstanceProcAddr, getDeviceProcAddr](
                                const char* name, VkInstance instance,
                                VkDevice device) -> PFN_vkVoidFunction {
    if (device != VK_NULL_HANDLE) return getDeviceProcAddr(device, name);
    return reinterpret_cast<PFN_vkVoidFunction>(
        getInstanceProcAddr(instance, name));
  };
  // The caller's device extensions and enabled features are not declared,
  // so Skia assumes the Vulkan 1.1 baseline — a deliberate under-promise
  // that costs optional fast paths, never correctness.

  // Single-threaded (kNo): the context and its one recorder live on one
  // thread, matching the Metal setup.
  backendContext.fMemoryAllocator = skgpu::VulkanMemoryAllocators::Make(
      backendContext, skgpu::ThreadSafe::kNo);
  if (!backendContext.fMemoryAllocator) return nullptr;

  std::unique_ptr<skgpu::graphite::Context> context =
      skgpu::graphite::ContextFactory::MakeVulkan(backendContext,
                                                  makeContextOptions());
  if (!context) return nullptr;

  std::unique_ptr<skgpu::graphite::Recorder> recorder =
      context->makeRecorder(makeRecorderOptions());
  if (!recorder) return nullptr;

  return std::unique_ptr<GraphiteContext>(
      new GraphiteContext(std::move(context), std::move(recorder)));
}

}  // namespace sigil::skia

#else  // !SK_VULKAN

namespace sigil::skia {

// A Skia without the Vulkan backend: nothing to stand up.
std::unique_ptr<GraphiteContext> GraphiteContext::createVulkan(
    const VulkanHandles&) {
  return nullptr;
}

}  // namespace sigil::skia

#endif
