// world_bench — the GPU executor under load: what a chain of a given
// count costs per cooked frame on the device (dispatch and readback are
// separate rows), how the mask and the deformers change that, and what
// a point-set seed's re-upload costs against a parameter edit. Needs a
// Vulkan runtime; every benchmark skips cleanly without one. Run a
// Release build; Debug numbers say nothing.

#include <benchmark/benchmark.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Pop.h>

#include <cmath>
#include <memory>
#include <vector>

#include "sigilworld/World.h"

using namespace sigil;

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

std::unique_ptr<world::World> makeWorld() {
  world::WorldConfig config;
  config.width = 64;
  config.height = 64;
  return world::World::create(config);
}

geometry::mesh::pop::Builder plain(int count) {
  return geometry::mesh::pop::on(ring(12))
      .count(count)
      .spread(30)
      .jitter(6)
      .noise(14, 0.012f)
      .fade({1, 0.3f, 0.2f, 1}, {0.2f, 0.6f, 1, 1})
      .vary(0.4f);
}

/** One cooked frame per iteration: the window slides, so the whole
 *  chain re-dispatches, and the frame draws the instanced surface. */
void cookFrames(benchmark::State& state,
                const geometry::mesh::pop::Chain& chain) {
  std::unique_ptr<world::World> w = makeWorld();
  if (!w) {
    state.SkipWithMessage("no 3D backend");
    return;
  }
  const uint32_t id =
      w->placeChain(geometry::mesh::quad(4, 4), chain, world::Material{});
  if (id == 0) {
    state.SkipWithMessage("chain declined");
    return;
  }
  w->render();
  float head = 1;
  for (auto _ : state) {
    head = head > 0.0f ? head - 0.01f : 1.0f;
    w->setChainWindow(id, head, 0.7f);
    w->render();
  }
  // A readback at the end waits for the device, so the timings above
  // include the frames actually finishing.
  benchmark::DoNotOptimize(w->readChain(id));
  state.SetItemsProcessed(state.iterations() * (int64_t)chain.size());
}

void BM_GpuCook_Plain(benchmark::State& state) {
  cookFrames(state, plain((int)state.range(0)));
}
BENCHMARK(BM_GpuCook_Plain)->Arg(10000)->Arg(100000)->Arg(1000000);

void BM_GpuCook_MaskedAndDeformed(benchmark::State& state) {
  cookFrames(state, plain((int)state.range(0))
                        .select("band", {0, 0, 0}, 160, 0.4f)
                        .twist(90, {0, 1, 0}, -60, 60)
                        .masked("band")
                        .bend(40, {0, 1, 0}, {1, 0, 0}, -60, 60)
                        .peak(8)
                        .masked("band")
                        .mixBy("Color", "Color", "Color", "band"));
}
BENCHMARK(BM_GpuCook_MaskedAndDeformed)->Arg(10000)->Arg(100000)->Arg(1000000);

void BM_GpuCook_Relax(benchmark::State& state) {
  cookFrames(state, plain((int)state.range(0)).smooth(0.5f, 4));
}
BENCHMARK(BM_GpuCook_Relax)->Arg(10000)->Arg(100000)->Arg(1000000);

void BM_GpuReadPoints(benchmark::State& state) {
  std::unique_ptr<world::World> w = makeWorld();
  if (!w) {
    state.SkipWithMessage("no 3D backend");
    return;
  }
  const uint32_t id =
      w->placeChain(geometry::mesh::quad(4, 4), plain((int)state.range(0)),
                    world::Material{});
  if (id == 0) {
    state.SkipWithMessage("chain declined");
    return;
  }
  w->render();
  for (auto _ : state) benchmark::DoNotOptimize(w->readChain(id));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_GpuReadPoints)->Arg(10000)->Arg(100000)->Arg(1000000);

void BM_GpuPointSet_Reupload(benchmark::State& state) {
  // A point-set-led chain re-described every frame: the structural path
  // (arena rebuilt and re-uploaded), the cost of "the cloud is the
  // data".
  std::unique_ptr<world::World> w = makeWorld();
  if (!w) {
    state.SkipWithMessage("no 3D backend");
    return;
  }
  const geometry::mesh::Cloud seed = plain((int)state.range(0)).cloud();
  geometry::mesh::pop::Chain chain = geometry::mesh::pop::on(seed)
                                         .move({0, 10, 0})
                                         .vary(0.3f)
                                         .lookAt({0, 0, 900});
  const uint32_t id =
      w->placeChain(geometry::mesh::quad(4, 4), chain, world::Material{});
  if (id == 0) {
    state.SkipWithMessage("chain declined");
    return;
  }
  w->render();
  float y = 0;
  for (auto _ : state) {
    y += 1;
    std::get<geometry::mesh::pop::PointSet>(chain.front())
        .cloud.positions[0]
        .y = y;
    w->setChain(id, chain);
    w->render();
  }
  benchmark::DoNotOptimize(w->readChain(id));
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_GpuPointSet_Reupload)->Arg(10000)->Arg(100000);

}  // namespace

BENCHMARK_MAIN();
