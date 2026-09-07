/** @file
 * The process's device: null unless a host installed one, and the same
 * pointer to every sketch while it is installed.
 */

#include <gtest/gtest.h>
#include <sigilsketch/core/Device.h>

namespace {

using namespace sigil::sketch;

/** The address stands in for a device: the door is a borrowed pointer
 *  and never dereferences it, so what is asserted here is the borrowing
 *  and not anything a device does. A test binary that could bring a real
 *  one up would still be asserting this. */
sigil::geometry::device::Device* stand(void* address) {
  return static_cast<sigil::geometry::device::Device*>(address);
}

TEST(SketchDevice, NoHostInstalledOneIsTheCpuTier) {
  EXPECT_EQ(device(), nullptr);
}

TEST(SketchDevice, TheInstalledDeviceIsWhatComesBack) {
  int marker = 0;
  sigil::geometry::device::Device* installed = stand(&marker);
  useDevice(installed);
  EXPECT_EQ(device(), installed);
  // A host that lets its device go says so, and the answer is the CPU
  // tier again rather than a pointer to something released.
  useDevice(nullptr);
  EXPECT_EQ(device(), nullptr);
}

}  // namespace
