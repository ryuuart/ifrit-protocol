/** @file
 * What a scene texture costs beside the composer it is made of: the
 * frame that paints, the frame that does not, and the same tree drawn
 * through a plain composer into a raster surface — which is the floor
 * every arm here is read against.
 *
 * Release only; a Debug timing says nothing about the library.
 */

#include <sigilcompose/core/Factories.h>
#include <sigilcompose/texture/Texture.h>

#include "BenchSupport.h"

namespace sigil::compose::bench {

namespace {

constexpr int kSide = 256;

/** A tree with real work in it: a grid of filled cells over a plate, so
 *  a paint is a paint rather than one rectangle. */
Element card(int cells, int phase) {
  Element root = box().width(pct(100)).height(pct(100)).row().wrapLines();
  for (int i = 0; i < cells; ++i)
    root.child(box().width(16).height(16).fill(cellFill(i, 0, phase)));
  return root;
}

/** The frame a still tree costs: reconciled, compared, and NOT painted. */
void TextureSceneStill(benchmark::State& state) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({kSide, kSide}, fonts());
  const Element tree = card((int)state.range(0), 0);
  scene->render(tree);
  const uint64_t painted = scene->version();
  double seconds = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    seconds += 1.0 / 60.0;
    scene->render(tree, seconds);
  }
  state.counters["paints"] = (double)(scene->version() - painted);
  reportNodes(state, (int)state.range(0));
}

/** …and the frame a changed tree costs: reconciled AND painted. */
void TextureScenePaints(benchmark::State& state) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({kSide, kSide}, fonts());
  int phase = 0;
  double seconds = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    seconds += 1.0 / 60.0;
    scene->render(card((int)state.range(0), ++phase % 2), seconds);
  }
  reportNodes(state, (int)state.range(0));
}

/** The floor: the same tree through a plain composer onto a raster
 *  canvas, which is every cost above minus the surface the scene owns
 *  and the version it keeps. */
void ComposerDraw(benchmark::State& state) {
  Host host(kSide, kSide);
  int phase = 0;
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.render(card((int)state.range(0), ++phase % 2));
    host.draw();
  }
  reportNodes(state, (int)state.range(0));
}

/** Taking the value a consumer holds, which is what a describe does
 *  every frame whether or not anything moved. */
void TextureValue(benchmark::State& state) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({kSide, kSide}, fonts());
  scene->render(card(64, 0));
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(scene->texture());
}

void cellLadder(::benchmark::Benchmark* b) {
  b->Arg(64)->Arg(256)->Unit(benchmark::kMicrosecond);
}

BENCHMARK(TextureSceneStill)->Apply(cellLadder);
BENCHMARK(TextureScenePaints)->Apply(cellLadder);
BENCHMARK(ComposerDraw)->Apply(cellLadder);
BENCHMARK(TextureValue);

}  // namespace

}  // namespace sigil::compose::bench
