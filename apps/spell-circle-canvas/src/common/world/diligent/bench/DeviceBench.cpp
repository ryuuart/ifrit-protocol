/** @file
 * world_diligent_bench — bringing the one device up: the Diligent Vulkan
 * device and queue, and SigilSkia's adoption of them. Run a Release
 * build; Debug numbers say nothing. Needs a Vulkan runtime and reports a
 * skip without one.
 */

#include <benchmark/benchmark.h>
#include <sigilworld/diligent/Device.h>

#include <memory>
#include <string>

using namespace sigil::world;

namespace {

/** A process pays this once, and it is what the first frame ever drawn
 *  waits on. Teardown is untimed: what is measured is the way in. */
void BM_DeviceBringUp(benchmark::State& state) {
  const diligent::DeviceConfig config;
  std::string error;
  if (!diligent::Device::create(config, &error)) {
    state.SkipWithError(error.c_str());
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    std::unique_ptr<diligent::Device> device =
        diligent::Device::create(config, &error);
    benchmark::DoNotOptimize(device);
    state.PauseTiming();
    device.reset();
    state.ResumeTiming();
  }
}
BENCHMARK(BM_DeviceBringUp)->Unit(benchmark::kMillisecond);

}  // namespace

BENCHMARK_MAIN();
