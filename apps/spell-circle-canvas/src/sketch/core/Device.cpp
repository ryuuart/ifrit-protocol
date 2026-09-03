/** @file
 * The process's device, held as a borrowed pointer.
 */

#include <sigilsketch/core/Device.h>

namespace sigil::sketch {

namespace {

/** Null until a host installs one, which is the CPU tier — the same
 *  starting answer the two runtimes give, so nothing has to be installed
 *  for a sketch to run. */
geometry::device::Device*& processDevice() {
  static geometry::device::Device* held = nullptr;
  return held;
}

}  // namespace

void useDevice(geometry::device::Device* device) { processDevice() = device; }

geometry::device::Device* device() { return processDevice(); }

}  // namespace sigil::sketch
