#pragma once

/** @file
 * One device for 2D and 3D. Diligent creates the Vulkan device — this
 * build of it cannot attach to a device that already exists, and has no
 * Metal backend — so SigilSkia adopts what Diligent made rather than
 * standing a second device up beside it.
 */

#include <memory>
#include <string>

namespace Diligent {
struct IRenderDevice;
struct IDeviceContext;
}  // namespace Diligent

namespace sigil::skia {
class GpuDevice;
}  // namespace sigil::skia

namespace sigil::world {

/**
 * The Vulkan objects behind @p device and its immediate @p context —
 * instance, physical device, device, queue and queue family, the API
 * version, and the `vkGetInstanceProcAddr` the loader already in this
 * process hands out — given to SigilSkia as a device it adopts: it never
 * frees any of them, and Diligent stays their owner for as long as the
 * returned device is used.
 *
 * The one requirement on @p device is timeline semaphores, which is what
 * a SigilSkia fence is; ask Diligent for them by requesting its
 * `NativeFence` feature when creating the device.
 *
 * Null with the reason in @p error when @p device is not a Vulkan one,
 * when its queue cannot be reached, when it has no timeline semaphores,
 * or when the adoption itself fails.
 */
std::unique_ptr<skia::GpuDevice> adoptVulkanDevice(
    Diligent::IRenderDevice* device, Diligent::IDeviceContext* context,
    std::string* error);

}  // namespace sigil::world
