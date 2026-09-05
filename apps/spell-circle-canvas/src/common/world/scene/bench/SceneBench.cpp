/** @file
 * What a frame costs: describing and reconciling a tree at several node
 * counts, the steady frame that follows, the draw, and a whole frame of
 * passes ordered and executed.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <sigilmotion/clock/Ticker.h>
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
Element describeTree(int nodes, const geometry::mesh::Mesh& body, float phase) {
  Element root;
  root.key("root");
  for (int i = 0; i < nodes; ++i) {
    const int column = i % 32;
    const int row = i / 32;
    root.child(Element()
                   .key("n" + std::to_string(i))
                   .mesh(body)
                   .at({(float)column * 12.0f - 180.0f,
                        (float)row * 12.0f - 90.0f, 0.0f})
                   .rotateY(phase + (float)i));
  }
  return root;
}

void MountTree(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  const geometry::mesh::Mesh body = triangle();
  motion::Ticker ticker;
  for ([[maybe_unused]] auto iteration : state) {
    Scene scene(ticker);
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
  motion::Ticker ticker;
  Scene scene(ticker);
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
  motion::Ticker ticker;
  Scene scene(ticker);
  const Element still = describeTree(nodes, body, 0.0f);
  for (int warm = 0; warm < 8; ++warm) scene.render(still);
  for ([[maybe_unused]] auto iteration : state) {
    scene.render(still);
    int64_t replayed = scene.stats().replayed;
    benchmark::DoNotOptimize(replayed);
  }
  state.SetItemsProcessed(state.iterations() * nodes);
}
BENCHMARK(PrunedFrame)->Arg(64)->Arg(512)->Arg(2048);

/** A whole frame: describe, extract, order the passes and execute them.
 *  The chain is deliberately one every pass depends on, so the ordering
 *  has work to do and the transients take turns on their surfaces. */
void FramePasses(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  const geometry::mesh::Mesh body = triangle();
  motion::Ticker ticker;
  Scene scene(ticker);
  float phase = 0.0f;
  geometry::mesh::camera::Camera lens;
  lens.eye = {0, 0, 620};
  const auto describe = [&](float at) {
    Frame frame(describeTree(nodes, body, at));
    frame.extent({256, 256}).camera(lens);
    frame.pass(geometryPass("main").writes("colour"))
        .pass(postPass("half").reads("colour").writes("half").blur(4.0f))
        .pass(postPass("hot").reads("half").writes("hot").levels(
            1.4f, 0.02f, {1.0f, 0.9f, 0.8f, 1.0f}))
        .pass(postPass("picture")
                  .reads("colour", "hot")
                  .writes("picture")
                  .composite(SkBlendMode::kPlus, 0.7f));
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
BENCHMARK(FramePasses)->Arg(64)->Arg(512);

void DrawFrame(benchmark::State& state) {
  const int nodes = (int)state.range(0);
  const geometry::mesh::Mesh body = triangle();
  motion::Ticker ticker;
  Scene scene(ticker);
  scene.render(describeTree(nodes, body, 0.0f));
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(320, 240));
  SkCanvas canvas(bitmap);
  geometry::mesh::camera::Camera camera;
  camera.eye = {0, 0, 480};
  for ([[maybe_unused]] auto iteration : state) scene.draw(canvas, camera);
  state.SetItemsProcessed(state.iterations() * nodes);
}
BENCHMARK(DrawFrame)->Arg(64)->Arg(512);

}  // namespace
