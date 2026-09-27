/** @file
 * Substance in material_bench: decoding the SDK's sample archive, and
 * a warm re-cook at each size after an input moves — the cost a live
 * material pays per change. Registers nothing without the SDK or its
 * sample, so the run is empty rather than failed.
 */

#include <benchmark/benchmark.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/substance/advanced/Archive.h>
#include <sigilmaterial/substance/advanced/Cook.h>

#include <filesystem>
#include <memory>
#include <string>

using namespace sigil;
using namespace sigil::material;

namespace {

std::string sampleArchive() {
  return (std::filesystem::path(SIGIL_SUBSTANCE_SDK_DIR) / "assets" /
          "Autumn_Leaves.sbsar")
      .string();
}

/** Decoding an archive whose bytes the hub already holds. */
void SubstanceDecode(benchmark::State& state) {
  io::Hub hub;
  const std::shared_ptr<const io::Bytes> bytes = hub.read(sampleArchive());
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(sbsar::Archive::decode(bytes->span()));
}

/** A warm re-cook at range(0) pixels a side after the hue moves, on
 *  @p engine: to host memory on the CPU, to a device texture on the GPU,
 *  each ending where a drawer can take it. Timed by the wall clock,
 *  since the calling thread waits while the engine's own threads and the
 *  device do the work. */
void SubstanceCookOn(benchmark::State& state, sbsar::Engine engine) {
  io::Hub hub;
  sbsar::CookScheduler cook(sbsar::load(hub, sampleArchive()), 0,
                            {.resolution = (int)state.range(0),
                             .engine = engine});
  if (cook.engine() != engine) {
    state.SkipWithError("the engine does not start here");
    return;
  }
  cook.cookNow();
  bool toggle = false;
  for ([[maybe_unused]] auto _ : state) {
    const float hue[] = {toggle ? 0.5f : 0.0f};
    toggle = !toggle;
    cook.set("Hue_Shift", hue);
    benchmark::DoNotOptimize(cook.cookNow());
  }
  state.SetItemsProcessed(state.iterations() * state.range(0) *
                          state.range(0));
}

/** Registered only where there is something to cook. */
const bool registered = [] {
  if (!sbsar::available() || !std::filesystem::exists(sampleArchive()))
    return false;
  benchmark::RegisterBenchmark("SubstanceDecode", SubstanceDecode)
      ->Unit(benchmark::kMillisecond);
  benchmark::RegisterBenchmark("SubstanceCook/Cpu", SubstanceCookOn,
                               sbsar::Engine::Cpu)
      ->Arg(64)
      ->Arg(256)
      ->Arg(1024)
      ->UseRealTime()
      ->Unit(benchmark::kMillisecond);
  if (sbsar::available(sbsar::Engine::Metal))
    benchmark::RegisterBenchmark("SubstanceCook/Metal", SubstanceCookOn,
                                 sbsar::Engine::Metal)
        ->Arg(64)
        ->Arg(256)
        ->Arg(1024)
        ->UseRealTime()
        ->Unit(benchmark::kMillisecond);
  return true;
}();

}  // namespace
