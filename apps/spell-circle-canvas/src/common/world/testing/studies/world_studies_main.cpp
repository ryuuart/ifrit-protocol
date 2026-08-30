// world_studies — the 3D study harness, headless:
//
//   world_studies --headless <outdir> [--study <name>] [--gpu]
//   world_studies --headless <outdir> --list-studies
//
// Each study is stepped from zero to its declared moment and
// photographed, so a plate is a function of the declaration alone. On
// the CPU that is what scripts/plate_ledger.py --tier world hashes; with
// --gpu it is what --tier world-gpu compares against those plates.
//
// The device is brought up HERE rather than in the harness, so the study
// library links no device and a machine with no Vulkan runtime still
// renders the CPU tier.

#ifndef SIGILWORLD_NO_DEVICE
#include <sigilworld/diligent/Device.h>
#include <sigilworld/diligent/Runtime.h>
#endif

#include <memory>
#include <string>

#include "Studies.h"

namespace {

#ifndef SIGILWORLD_NO_DEVICE
/** Held for the run: the runtime borrows the device, and every texture
 *  and pipeline it made goes when the device does. */
std::unique_ptr<sigil::world::diligent::Device> g_device;

sigil::world::Runtime deviceRuntime(std::string* error) {
  const sigil::world::diligent::DeviceConfig config;
  g_device = sigil::world::diligent::Device::create(config, error);
  if (!g_device) return {};
  return sigil::world::diligent::runtime(*g_device);
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
#ifdef SIGILWORLD_NO_DEVICE
  return sigil::world::testing::runStudies(sigil::world::testing::registry(),
                                           argc, argv);
#else
  const int result = sigil::world::testing::runStudies(
      sigil::world::testing::registry(), argc, argv, deviceRuntime);
  // The device outlives every frame that used it and goes before the
  // process does, so nothing it made is released after its queue.
  g_device.reset();
  return result;
#endif
}
