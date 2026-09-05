#pragma once

/** @file
 * The one Vulkan device a test process brings up, and the reason there is
 * none. SigilGeometryDevice owns `Device::create`, so every binary whose
 * cases need a device reads it here rather than standing one up per file.
 */

#include <sigilgeometry/device/Device.h>

#include <memory>
#include <string>

namespace sigil::geometry::device::test {

/** A device, or the reason there is none. A case that has no device SKIPS
 *  with that reason rather than failing, so a machine with no Vulkan
 *  runtime stays green. */
struct OnDevice {
  Device* device = nullptr;
  std::string error;

  explicit operator bool() const { return device != nullptr; }
  Device& operator*() const { return *device; }
  Device* operator->() const { return device; }
};

/** The one device this process brings up, kept for its lifetime: making a
 *  second Vulkan device costs the driver more the more it has already
 *  made, and every case wants the same one anyway. */
inline OnDevice onDevice() {
  static std::string error;
  static std::unique_ptr<Device> made = [] {
    const DeviceConfig config;
    return Device::create(config, &error);
  }();
  OnDevice out;
  out.device = made.get();
  out.error = error;
  return out;
}

}  // namespace sigil::geometry::device::test

/** Declares `name` as the process's device, or skips the case naming what
 *  is missing. `name` is a declarator, so it cannot be parenthesised;
 *  every caller passes a plain identifier. */
// NOLINTBEGIN(bugprone-macro-parentheses)
#define SIGIL_ON_DEVICE_OR_SKIP(name)                                \
  const ::sigil::geometry::device::test::OnDevice name =             \
      ::sigil::geometry::device::test::onDevice();                   \
  if (!name) GTEST_SKIP() << "no Vulkan device: " << (name).error
// NOLINTEND(bugprone-macro-parentheses)
