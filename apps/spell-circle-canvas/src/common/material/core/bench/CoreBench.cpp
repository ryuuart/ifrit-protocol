/** @file
 * material_core_bench — resolve per call as the uniform count grows,
 * with and without a memo hit, and the declarations a schema emits. Run a
 * Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/Material.h>

#include <array>
#include <memory>

using namespace sigil::material;

namespace {

struct P0 {
  float a;
};
struct P4 {
  float a, b, c, d;
};
struct P16 {
  float a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p;
};

std::shared_ptr<Program> nullCompiler(std::shared_ptr<const Recipe> r,
                                      Variant v, std::string&) {
  return std::make_shared<Program>(std::move(r), Target::Slang, v);
}

void registerOnce() {
  static bool done = [] {
    registerCompiler(Target::Slang, nullCompiler);
    return true;
  }();
  (void)done;
}

/** Resolve with the clock advancing every call, so every call misses the
 *  memo and rebuilds the upload bytes. */
template <class P>
void BM_Resolve_Live(benchmark::State& state) {
  registerOnce();
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<P>("bench").frame(FrameInput::Time).body(Target::Slang, "x"));
  Material m(recipe);
  FrameData frame;
  for ([[maybe_unused]] auto iteration : state) {
    frame.seconds += 1.0 / 60.0;
    Material::Resolved r = m.resolve(Target::Slang, frame);
    benchmark::DoNotOptimize(r.bytes.data());
  }
}

/** Resolve with nothing changing, so every call after the first is a
 *  memo hit: the cost of proving the inputs equal. */
template <class P>
void BM_Resolve_Memo(benchmark::State& state) {
  registerOnce();
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<P>("bench").body(Target::Slang, "x"));
  Material m(recipe);
  const FrameData frame;
  for ([[maybe_unused]] auto iteration : state) {
    Material::Resolved r = m.resolve(Target::Slang, frame);
    benchmark::DoNotOptimize(r.bytes.data());
  }
}

template <class P>
void BM_Declare(benchmark::State& state) {
  for ([[maybe_unused]] auto iteration : state) {
    std::string s = declare<P>(Target::SkSL);
    benchmark::DoNotOptimize(s.data());
  }
}

}  // namespace

BENCHMARK(BM_Resolve_Live<P0>)->Name("BM_Resolve_Live/0");
BENCHMARK(BM_Resolve_Live<P4>)->Name("BM_Resolve_Live/4");
BENCHMARK(BM_Resolve_Live<P16>)->Name("BM_Resolve_Live/16");
BENCHMARK(BM_Resolve_Memo<P0>)->Name("BM_Resolve_Memo/0");
BENCHMARK(BM_Resolve_Memo<P4>)->Name("BM_Resolve_Memo/4");
BENCHMARK(BM_Resolve_Memo<P16>)->Name("BM_Resolve_Memo/16");
BENCHMARK(BM_Declare<P4>)->Name("BM_Declare/4");
BENCHMARK(BM_Declare<P16>)->Name("BM_Declare/16");

BENCHMARK_MAIN();
