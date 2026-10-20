#pragma once

/** @file
 * The Graphite context that stands on a device's own Vulkan queue, for the
 * cases in any binary that ask what 2D painted through Vulkan does.
 */

#include <gpu/graphite/Recorder.h>
#include <sigilgeometry/device/Device.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <memory>

namespace sigil::geometry::device::test {

/** GRAPHITE ON THE DEVICE'S OWN VULKAN QUEUE: the device's `graphite()`
 *  where 2D shares that queue, and where it paints through Metal instead,
 *  a Vulkan context made over the adopted device and kept for the process
 *  — the context a host without Metal stands on. Null where the device
 *  was not adopted or this Skia carries no Vulkan backend. */
inline skia::GraphiteContext* vulkanGraphite(Device& device) {
  skia::GraphiteContext* own = device.graphite();
  if (own && own->recorder() &&
      own->recorder()->backend() == skgpu::BackendApi::kVulkan)
    return own;
  if (!device.gpu()) return nullptr;
  static const std::unique_ptr<skia::GraphiteContext> made =
      skia::GraphiteContext::create(*device.gpu());
  return made.get();
}

}  // namespace sigil::geometry::device::test
