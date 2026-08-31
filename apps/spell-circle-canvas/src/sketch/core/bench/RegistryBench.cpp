/** @file
 * Resolving a name to a registry entry — what every host does before it
 * can show anything, and what a sweep does once per sketch.
 */

#include <benchmark/benchmark.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <string>

namespace {

using namespace sigil::sketch;

struct StillKind final : KindOps {
  [[nodiscard]] std::string_view runtime() const override { return "still"; }
  [[nodiscard]] std::unique_ptr<Session> open(sigil::weave::FontContext&,
                                              Assets&) const override {
    return nullptr;
  }
  bool operator==(const StillKind&) const { return true; }
};

Kind stillKind() { return StillKind{}; }

/** A registry the size of a real one, so lookup is measured against the
 *  list a host actually walks. Filled on first use rather than from a
 *  namespace-scope initializer: the names it keeps are allocated, and an
 *  allocation before main() has no handler to throw to. */
bool filled() {
  static const bool once = [] {
    static std::vector<std::string> names;
    names.reserve(128);
    for (int i = 0; i < 128; ++i) {
      names.push_back("sketch_" + std::to_string(i) + "_study");
      add(names.back().c_str(), nullptr, "Bench", "", &stillKind);
    }
    return true;
  }();
  return once;
}

void FindExact(benchmark::State& state) {
  (void)filled();
  for (auto&& _ : state) benchmark::DoNotOptimize(find("sketch_120_study"));
}
BENCHMARK(FindExact);

void FindSubstring(benchmark::State& state) {
  (void)filled();
  for (auto&& _ : state) benchmark::DoNotOptimize(find("_120_"));
}
BENCHMARK(FindSubstring);

void FindMissing(benchmark::State& state) {
  (void)filled();
  for (auto&& _ : state) benchmark::DoNotOptimize(find("no such sketch"));
}
BENCHMARK(FindMissing);

/** Constructing the kind: one erased value per activation, which is what
 *  a host pays to open a sketch. */
void MakeKind(benchmark::State& state) {
  (void)filled();
  for (auto&& _ : state) benchmark::DoNotOptimize(registry()[0].kind());
}
BENCHMARK(MakeKind);

}  // namespace

BENCHMARK_MAIN();
