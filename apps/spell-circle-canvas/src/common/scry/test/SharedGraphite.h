#pragma once

/** @file
 * The one device and Graphite context a process may own, for every GPU
 * case in this library: the subject and the case draw on the same device
 * and the same queue, which is what makes a colour read back proof of
 * what the subject rendered. The read itself is SigilSkia's, beside the
 * context it turns — "GraphiteReadback.h".
 */

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <memory>

namespace sigil::scry::test {

/** The device this process owns, shared by whatever is under test and
 *  every case in the binary. */
inline core::hardware::GpuDevice* sharedDevice() {
  static std::unique_ptr<core::hardware::GpuDevice> device =
      core::hardware::GpuDevice::createOwned();
  return device.get();
}

/** The Graphite context the cases draw with and the subject shares: the
 *  web thread records on its own recorder over it, so every context call
 *  holds lockContext(). */
inline skia::GraphiteContext* sharedGraphite() {
  static std::unique_ptr<skia::GraphiteContext> graphite =
      sharedDevice() ? skia::GraphiteContext::create(*sharedDevice()) : nullptr;
  return graphite.get();
}

}  // namespace sigil::scry::test
