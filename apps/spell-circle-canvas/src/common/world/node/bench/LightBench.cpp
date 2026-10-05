/** @file
 * world_light_bench — the falloff a renderer evaluates per light per
 * shaded point, by kind. Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/node/Light.h>

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
  const PlacedLight lights[] = {
      {directionalLight({0, -1, 0}).description(), {0, 100, 0}, ""},
      {pointLight().description(), {0, 100, 0}, ""},
      {spotLight({0, -1, 0}).description(), {0, 100, 0}, ""},
  };
  const PlacedLight& light = lights[state.range(0)];
  const std::vector<glm::vec3> at = points(1024);
  for ([[maybe_unused]] auto iteration : state) {
    float sum = 0;
    for (const glm::vec3& p : at) sum += attenuation(light, p);
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * (int64_t)at.size());
}
BENCHMARK(BM_Attenuation)->Arg(0)->Arg(1)->Arg(2);

}  // namespace
