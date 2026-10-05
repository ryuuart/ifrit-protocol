/** @file
 * What a frame costs: describing and reconciling a tree at several node
 * counts, the steady frame that follows, and a whole frame of passes
 * ordered and handed to an executor that draws nothing — so what is
 * measured is the retained side and the ordering, and what a device
 * costs is the device bench's.
 */

#include <benchmark/benchmark.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/scene/Scene.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;

namespace {

geometry::mesh::Mesh triangle() {
  geometry::mesh::Mesh m;
  m.positions = {{-4, -4, 0}, {4, -4, 0}, {0, 4, 0}};
  m.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  m.uvs = {{0, 0}, {1, 0}, {0.5f, 1}};
  m.indices = {0, 1, 2};
  return m;
}

/** One geometry for the whole tree, so the store's dedup is what the
 *  ladder measures rather than the cook. */
Group describeTree(int nodes, const geometry::mesh::Mesh& body, float phase) {
  return group(each((size_t)nodes,
                    [&](size_t i) {
                      const int column = (int)i % 32;
                      const int row = (int)i / 32;
                      return mesh(body)
                          .key("n" + std::to_string(i))
                          .at({(float)column * 12.0f - 180.0f,
                               (float)row * 12.0f - 90.0f, 0.0f})
                          .rotateY(phase + (float)i);
                    }))
      .key("root");
}

void MountTree(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  const geometry::mesh::Mesh body = triangle();
  motion::Engine engine;
  for ([[maybe_unused]] auto iteration : state) {
    Scene scene(engine);
    scene.render(describeTree(nodes, body, 0.0f));
    int64_t nodes = scene.stats().nodes;
    benchmark::DoNotOptimize(nodes);
  }
  state.SetItemsProcessed(state.iterations() * nodes);
}
BENCHMARK(MountTree)->Arg(64)->Arg(512)->Arg(2048);

void SteadyFrame(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  const geometry::mesh::Mesh body = triangle();
  motion::Engine engine;
  Scene scene(engine);
  scene.render(describeTree(nodes, body, 0.0f));
  float phase = 0.0f;
  for ([[maybe_unused]] auto iteration : state) {
    phase += 1.0f;
    scene.render(describeTree(nodes, body, phase));
    int64_t extracted = scene.stats().extracted;
    benchmark::DoNotOptimize(extracted);
  }
  state.SetItemsProcessed(state.iterations() * nodes);
}
BENCHMARK(SteadyFrame)->Arg(64)->Arg(512)->Arg(2048);

void PrunedFrame(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  const geometry::mesh::Mesh body = triangle();
  motion::Engine engine;
  Scene scene(engine);
  const Group still = describeTree(nodes, body, 0.0f);
  for (int warm = 0; warm < 8; ++warm) scene.render(still);
  for ([[maybe_unused]] auto iteration : state) {
    scene.render(still);
    int64_t replayed = scene.stats().replayed;
    benchmark::DoNotOptimize(replayed);
  }
  state.SetItemsProcessed(state.iterations() * nodes);
}
BENCHMARK(PrunedFrame)->Arg(64)->Arg(512)->Arg(2048);

/** An executor that is handed every step and draws nothing. */
struct Idle : Executor {
  void execute(const PassWork&, const View&, Targets&) const override {}
  bool operator==(const Idle&) const { return true; }
};

/** A whole frame: describe, extract, order the passes and hand each one
 *  to an executor. The chain is deliberately one every pass depends on,
 *  so the ordering has work to do and the transients take turns on their
 *  surfaces. */
void OrderedFrame(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  const geometry::mesh::Mesh body = triangle();
  motion::Engine engine;
  Scene scene(engine);
  float phase = 0.0f;
  geometry::mesh::camera::Camera lens;
  lens.eye = {0, 0, 620};
  const auto describe = [&](float at) {
    Frame frame(describeTree(nodes, body, at));
    frame.extent({256, 256}).defaultCamera(lens).runtime(Runtime{Idle{}});
    frame.pass(geometryPass("main").writes("colour"))
        .pass(postPass("half").reads("colour").writes("half").blur(4.0f))
        .pass(postPass("hot").reads("half").writes("hot").levels(
            {.gain = 1.4f, .lift = 0.02f, .tint = {1.0f, 0.9f, 0.8f, 1.0f}}))
        .pass(postPass("picture")
                  .reads("colour", "hot")
                  .writes("picture")
                  .composite({.mode = material::BlendMode::PlusLighter,
                              .opacity = 0.7f}));
    return frame;
  };
  for ([[maybe_unused]] auto iteration : state) {
    phase += 1.0f;
    scene.render(describe(phase));
    int64_t passes = scene.stats().passes;
    benchmark::DoNotOptimize(passes);
  }
  state.SetItemsProcessed(state.iterations() * nodes);
}
BENCHMARK(OrderedFrame)->Arg(64)->Arg(512);

}  // namespace
