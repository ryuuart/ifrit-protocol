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

// Registered while the binary loads: without the USD plugins there is
// nothing to time here, and the reason is printed once.
[[maybe_unused]] const int kRegistered = [] {
  std::string why;
  if (sigil::usd::available(&why))
    benchmark::RegisterBenchmark("BM_Available", BM_Available);
  else
    std::fprintf(stderr, "usd runtime benchmarks: nothing to run — %s\n",
                 why.c_str());
  return 0;
}();

}  // namespace
