/** @file
 * world_light_bench — the falloff a renderer evaluates per light per
 * shaded point, by kind. Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilworld/light/Light.h>

#include <vector>

using namespace sigil::world;

namespace {

std::vector<glm::vec3> points(int n) {
  std::vector<glm::vec3> out;
  out.reserve((size_t)n);
  for (int i = 0; i < n; ++i)
    out.emplace_back((float)(i % 97), (float)(i % 53), (float)(i % 31));
  return out;
}

void BM_Attenuation(benchmark::State& state) {
  const light::Light lights[] = {
      light::sun({0, -1, 0}),
      light::point({0, 100, 0}),
      light::spot({0, 100, 0}, {0, -1, 0}),
  };
  const light::Light& light = lights[state.range(0)];
  const std::vector<glm::vec3> at = points(1024);
  for ([[maybe_unused]] auto iteration : state) {
    float sum = 0;
    for (const glm::vec3& p : at) sum += light::attenuation(light, p);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)at.size());
}
BENCHMARK(BM_Attenuation)->Arg(0)->Arg(1)->Arg(2);

}  // namespace

BENCHMARK_MAIN();
