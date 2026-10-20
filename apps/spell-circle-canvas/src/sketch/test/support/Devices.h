#pragma once

/** @file
 * THE RUNTIME A SCENE3D SKETCH IS DRAWN ON IN A TEST: the device runtime over
 * the one Vulkan device the process brings up, installed for the process the
 * way a host installs it, or the reason there is none.
 *
 * A scene3d sketch is drawn on a device and nowhere else, so a case that opens
 * one asks for this first and SKIPS with the reason where the machine has no
 * Vulkan runtime — which is why such a case's suite carries the `gpu`
 * label. The binary that includes this links SigilWorldDiligent and puts
 * SigilGeometryDevice's test support on its include path.
 */

#include <gtest/gtest.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/diligent/Runtime.h>

#include <string>

#include "OnDevice.h"

namespace sigil::sketch::test {

/** The device runtime, made once for the process, or empty with the
 *  reason in @p why. */
inline const world::Runtime& deviceRuntime(std::string* why) {
  struct Made {
    world::Runtime runtime;
    std::string error;
  };
  static const Made made = [] {
    Made out;
    const geometry::device::test::OnDevice on =
        geometry::device::test::onDevice();
    if (on)
      out.runtime = world::diligent::runtime(*on);
    else
      out.error = on.error;
    return out;
  }();
  if (why && !made.runtime) *why = made.error;
  return made.runtime;
}

/** Installs the device runtime as the process's, as a host that brought
 *  a device up does, and answers whether there was one to install. */
inline bool installDeviceRuntime(std::string* why) {
  const world::Runtime& runtime = deviceRuntime(why);
  if (!runtime) return false;
  useRuntime(runtime);
  return true;
}

/** A SUITE WHOSE EVERY CASE STANDS ON THE DEVICE RUNTIME: installed before
 *  each case, or the case skipped with the reason there is none, and the
 *  process's runtime put back to none after it. A suite derives from this
 *  under its own name. */
class OnDeviceRuntime : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string why;
    if (!installDeviceRuntime(&why))
      GTEST_SKIP() << "no Vulkan device: " << why;
  }
  void TearDown() override { useRuntime({}); }
};

}  // namespace sigil::sketch::test
