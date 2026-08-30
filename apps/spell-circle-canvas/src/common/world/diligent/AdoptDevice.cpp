// Diligent's Vulkan handles, read off the interfaces that expose them
// and handed to SigilSkia.
//
// This is the one translation unit that spells both the Vulkan loader
// and Diligent's Vulkan interfaces, and it exists so that exactly one
// vkGetInstanceProcAddr is in play: the pointer volk resolved for
// Diligent is passed on to SigilSkia rather than letting it open the
// loader a second time. Two dlopens of the same library would work, but
// "one device" is only meaningful if both APIs dispatch through the same
// entry points.

#include "AdoptDevice.h"

// The include ORDER below is load-bearing, so it is fixed by hand:
// volk.h suppresses the Vulkan prototypes and declares every entry point
// as a pointer, and the Diligent interfaces that follow name Vulkan
// types without including a Vulkan header of their own.
// clang-format off
#include "thirdparty/volk/volk.h"

#include <Graphics/GraphicsEngineVulkan/interface/CommandQueueVk.h>
#include <Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <sigilskia/device/GpuDevice.h>
// clang-format on

namespace sigil::world::diligent {

std::unique_ptr<skia::GpuDevice> adoptVulkanDevice(
    Diligent::IRenderDevice* device, Diligent::IDeviceContext* context,
    std::string* error) {
  using namespace Diligent;
  if (!device || !context) {
    if (error) *error = "no render device to adopt";
    return nullptr;
  }

  RefCntAutoPtr<IRenderDeviceVk> deviceVk{device, IID_RenderDeviceVk};
  if (!deviceVk) {
    if (error) *error = "the render device is not a Vulkan one";
    return nullptr;
  }
  // A SigilSkia fence is a timeline semaphore, and a device that was not
  // created with them cannot make one.
  if (device->GetDeviceInfo().Features.NativeFence !=
      DEVICE_FEATURE_STATE_ENABLED) {
    if (error)
      *error = "the Vulkan device has no timeline semaphores, which a fence is";
    return nullptr;
  }

  skia::NativeDevice native;
  native.backend = skia::Backend::Vulkan;
  native.vulkan.instance = deviceVk->GetVkInstance();
  native.vulkan.physicalDevice = deviceVk->GetVkPhysicalDevice();
  native.vulkan.device = deviceVk->GetVkDevice();
  native.vulkan.apiVersion = deviceVk->GetVkVersion();

  // The queue is reached through the lock that guards it, and the lock is
  // released before anything else happens: what is wanted is the handle,
  // not exclusive use of the queue.
  {
    RefCntAutoPtr<ICommandQueueVk> queueVk{context->LockCommandQueue(),
                                           IID_CommandQueueVk};
    if (queueVk) {
      native.vulkan.queue = queueVk->GetVkQueue();
      native.vulkan.queueFamilyIndex = queueVk->GetQueueFamilyIndex();
    }
    queueVk.Release();
    context->UnlockCommandQueue();
  }
  if (!native.vulkan.queue) {
    if (error) *error = "the device context has no Vulkan command queue";
    return nullptr;
  }

  // volk resolved this when Diligent brought the loader up. Leaving it
  // null is legal — SigilSkia finds a loader itself — but then the two
  // APIs would dispatch through separately opened copies.
  if (vkGetInstanceProcAddr)
    native.vulkan.getInstanceProcAddr =
        reinterpret_cast<skia::VulkanHandles::GetInstanceProcAddr>(
            vkGetInstanceProcAddr);

  return skia::GpuDevice::adopt(native, error);
}

}  // namespace sigil::world::diligent
