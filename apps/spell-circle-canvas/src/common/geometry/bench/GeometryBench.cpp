// geometry_bench — the CPU reference executor under load: how the pop cook
// scales with count and with the operator mix, and what a mask, a
// deformer or a point-set seed costs relative to the plain chain. Run a
// Release build; Debug numbers say nothing.

#include <benchmark/benchmark.h>
#include <sigilgeometry/Import.h>
#include <sigilgeometry/Mesh.h>
#include <sigilgeometry/Pop.h>

#include <cmath>
#include <vector>

using namespace sigil::geometry;

namespace {

std::vector<glm::vec3> ring(int knots) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < knots; ++i) {
    const float a = (float)i / (float)knots * 2.0f * (float)M_PI;
    loop.push_back({200.0f * std::cos(a), 40.0f * std::sin(3.0f * a),
                    200.0f * std::sin(a)});
  }
  return loop;
}

/** The plain chain: scatter, jitter, noise, colour, size — the shape a
 *  comet has. */
pop::Builder plain(int count) {
  return pop::on(ring(12))
      .count(count)
      .spread(30)
      .jitter(6)
      .noise(14, 0.012f)
      .fade({1, 0.3f, 0.2f, 1}, {0.2f, 0.6f, 1, 1})
      .vary(0.4f);
}

void BM_PopCook_Plain(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0));
  for (auto _ : state) benchmark::DoNotOptimize(popops::cook(chain));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PopCook_Plain)->Arg(1000)->Arg(10000)->Arg(100000);

void BM_PopCook_MaskedAndDeformed(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0))
                               .select("band", {0, 0, 0}, 160, 0.4f)
                               .twist(90, {0, 1, 0}, -60, 60)
                               .masked("band")
                               .bend(40, {0, 1, 0}, {1, 0, 0}, -60, 60)
                               .peak(8)
                               .masked("band")
                               .mixBy("Color", "Color", "Color", "band");
  for (auto _ : state) benchmark::DoNotOptimize(popops::cook(chain));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PopCook_MaskedAndDeformed)->Arg(1000)->Arg(10000)->Arg(100000);

void BM_PopCook_Relax(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0)).smooth(0.5f, 4);
  for (auto _ : state) benchmark::DoNotOptimize(popops::cook(chain));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PopCook_Relax)->Arg(1000)->Arg(10000)->Arg(100000);

void BM_PopCook_PointSetSeed(benchmark::State& state) {
  // Seeding from an existing cloud with a few lanes: the copy-in cost
  // against the scatter it replaces.
  const Cloud seed = plain((int)state.range(0)).cloud();
  const pop::Chain chain =
      pop::on(seed).move({0, 10, 0}).vary(0.3f).lookAt({0, 0, 900});
  for (auto _ : state) benchmark::DoNotOptimize(popops::cook(chain));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PopCook_PointSetSeed)->Arg(1000)->Arg(10000)->Arg(100000);

void BM_PopSink_Stamps(benchmark::State& state) {
  const pop::Chain chain = plain((int)state.range(0)).lookAt({0, 0, 900});
  const Mesh stamp = mesh::quad(4, 4);
  for (auto _ : state) benchmark::DoNotOptimize(popops::cookMesh(chain, stamp));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_PopSink_Stamps)->Arg(1000)->Arg(10000);

/** A .geo file of @p points particles, written the way Houdini does,
 *  parsed back: the JSON reader and the paged decode. */
void BM_ImportGeo_Points(benchmark::State& state) {
  const int n = (int)state.range(0);
  std::string geo =
      "[\"fileversion\",\"20.5.278\",\"pointcount\"," + std::to_string(n) +
      ",\"vertexcount\",0,\"primitivecount\",0,"
      "\"topology\",[\"pointref\",[\"indices\",[]]],"
      "\"attributes\",[\"pointattributes\",[[[\"scope\",\"public\",\"type\","
      "\"numeric\",\"name\",\"P\",\"options\",{}],[\"size\",3,\"storage\","
      "\"fpreal32\",\"values\",[\"size\",3,\"storage\",\"fpreal32\","
      "\"packing\","
      "[3],\"pagesize\",1024,\"constantpageflags\",[[";
  const int pages = (n + 1023) / 1024;
  for (int p = 0; p < pages; ++p) geo += p ? ",false" : "false";
  geo += "]],\"rawpagedata\",[";
  for (int i = 0; i < n; ++i) {
    if (i) geo += ',';
    geo += std::to_string(i % 100) + ",0.5," + std::to_string(i % 7);
  }
  geo += "]]]]]],\"primitives\",[]]";
  for (auto _ : state)
    benchmark::DoNotOptimize(import::model(geo.data(), geo.size(), "p.geo"));
  state.SetItemsProcessed(state.iterations() * n);
  state.SetBytesProcessed(state.iterations() * (int64_t)geo.size());
}
BENCHMARK(BM_ImportGeo_Points)->Arg(10000)->Arg(100000);

}  // namespace

BENCHMARK_MAIN();
