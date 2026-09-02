/** @file
 * usd_runtime_bench — the probe after the registry is warm: the cost a
 * caller pays to ask again. Registers nothing when the runtime is
 * absent, so the ledger sees an empty run rather than a failure.
 * Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilusd/runtime/Runtime.h>

#include <cstdio>

namespace {

void BM_Available(benchmark::State& state) {
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(sigil::usd::available());
}

}  // namespace

int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape): an
                                   // uncaught error ends the run
  std::string why;
  if (sigil::usd::available(&why))
    benchmark::RegisterBenchmark("BM_Available", BM_Available);
  else
    std::fprintf(stderr, "usd_runtime_bench: nothing to run — %s\n",
                 why.c_str());
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
