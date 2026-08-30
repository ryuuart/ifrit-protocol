/** @file
 * substance_bench — loading the SDK's sample archive, describing a
 * graph, and cooking it at each resolution: the warm re-render after a
 * parameter change, which is the cost a live material pays. Registers
 * nothing when the sample is not where the SDK was configured from, so
 * the ledger sees an empty run rather than a failure. Run a Release
 * build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilsubstance/Substance.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

using namespace sigil;

namespace {

std::filesystem::path sampleArchive() {
  return std::filesystem::path(SIGIL_SUBSTANCE_SDK_DIR) / "assets" /
         "Autumn_Leaves.sbsar";
}

const std::vector<char>& sampleBytes() {
  static const std::vector<char> bytes = [] {
    std::ifstream in(sampleArchive(), std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
  }();
  return bytes;
}

/** Loading a package from bytes already in memory. */
void BM_Load(benchmark::State& state) {
  const std::vector<char>& bytes = sampleBytes();
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(
        substance::Package::load(bytes.data(), bytes.size()));
}

/** Describing every parameter of the first graph. */
void BM_Parameters(benchmark::State& state) {
  const std::vector<char>& bytes = sampleBytes();
  std::unique_ptr<substance::Package> package =
      substance::Package::load(bytes.data(), bytes.size());
  substance::Graph& graph = package->graph(0);
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(graph.parameters());
}

/** A warm re-render at 2^range(0) pixels a side after the first float
 *  slider is nudged, so the engine recomputes what the change dirtied
 *  rather than returning its cache. */
void BM_Render(benchmark::State& state) {
  const std::vector<char>& bytes = sampleBytes();
  std::unique_ptr<substance::Package> package =
      substance::Package::load(bytes.data(), bytes.size());
  substance::Graph& graph = package->graph(0);
  const int log2 = (int)state.range(0);
  graph.setResolution(log2, log2);
  std::string knob;
  float lo = 0, hi = 1;
  for (const substance::Parameter& p : graph.parameters())
    if (p.kind == substance::Parameter::Kind::Float &&
        p.identifier != "$outputsize" && p.maximum.size() == 1 &&
        p.maximum[0] > p.minimum[0]) {
      knob = p.identifier;
      lo = p.minimum[0];
      hi = p.maximum[0];
      break;
    }
  graph.render();
  bool toggle = false;
  for ([[maybe_unused]] auto _ : state) {
    if (!knob.empty()) graph.set(knob, toggle ? hi : lo);
    toggle = !toggle;
    benchmark::DoNotOptimize(graph.render());
  }
  const int64_t pixels = (int64_t)1 << (2 * log2);
  state.SetItemsProcessed(state.iterations() * pixels);
}

}  // namespace

int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape): an
                                   // uncaught error ends the run
  if (std::filesystem::exists(sampleArchive())) {
    benchmark::RegisterBenchmark("BM_Load", BM_Load)
        ->Unit(benchmark::kMillisecond);
    benchmark::RegisterBenchmark("BM_Parameters", BM_Parameters);
    benchmark::RegisterBenchmark("BM_Render", BM_Render)
        ->Arg(6)
        ->Arg(8)
        ->Unit(benchmark::kMillisecond);
  } else {
    std::fprintf(stderr,
                 "substance_bench: nothing to run — sample %s not found\n",
                 sampleArchive().string().c_str());
  }
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
