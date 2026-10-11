/** @file
 * What a scene texture costs beside the composer it is made of: the
 * frame that paints, the frame that does not, and the same tree drawn
 * through a plain composer into a raster surface — which is the floor
 * every arm here is read against.
 *
 * A page painted as the maps of a surface is measured twice over: on the
 * CPU and on the device, each with a change the size of a clock's numerals
 * and a change across the whole page, so the cost of keeping what did not
 * change is read beside the cost of repainting it.
 *
 * Release only; a Debug timing says nothing about the library.
 */

#include <sigilcompose/core/Factories.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>

#include <vector>

#include "BenchSupport.h"

namespace sigil::compose::bench {

namespace {

constexpr int kSide = 256;

/** A tree with real work in it: a grid of filled cells over a plate, so
 *  a paint is a paint rather than one rectangle. */
Element card(int cells, int phase) {
  Element root = box().width(pct(100)).height(pct(100)).row().flexWrap();
  for (int i = 0; i < cells; ++i)
    root.children({box().width(16).height(16).fill(cellFill(i, 0, phase))});
  return root;
}

/** The frame a still tree costs: reconciled, compared, and NOT painted. */
void TextureSceneStill(benchmark::State& state) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({kSide, kSide}, fonts());
  const Element tree = card((int)state.range(0), 0);
  scene->render(tree);
  const uint64_t painted = scene->revision();
  double seconds = 0.0;
  for ([[maybe_unused]] auto iteration : state) {
    seconds += 1.0 / 60.0;
    scene->render(tree, seconds);
  }
  state.counters["paints"] = (double)(scene->revision() - painted);
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

/** A specimen page: a lit ground of noise, eight lit swatches each with a
 *  noise of its own, a square whose colour follows @p frame and a passage
 *  of a stated size whose words do; with @p wholePage the ground's colour
 *  follows the frame too, so every pixel changes. */
Element specimenPage(int frame, bool wholePage, int cells = 0) {
  static const char8_t* const kWords[] = {u8"10.25", u8"10.50", u8"11.75",
                                          u8"12.00"};
  const float shade = (float)(frame % 8) / 8.0f;
  const material::Material ground =
      material::from(wholePage ? material::Color{.2f + .1f * shade, .2f, .2f, 1}
                               : material::Color{.2f, .2f, .2f, 1})
          .layer(material::noise(.09f),
                 {.blend = material::BlendMode::Multiply, .opacity = .3f})
          .surface({.roughness = .8f});
  std::vector<Element> children;
  for (int index = 0; index < 8; ++index) {
    const material::Material swatch =
        material::from(material::Color{.2f + .08f * (float)index, .3f, .5f, 1})
            .layer(material::noise(.03f + .01f * (float)index),
                   {.blend = material::BlendMode::Multiply, .opacity = .4f})
            .surface({.metallic = (float)(index % 2), .roughness = .3f});
    children.push_back(box()
                           .absolute()
                           .rect(40.0f + 243.0f * (float)(index % 4),
                                 146.0f + 236.0f * (float)(index / 4), 215, 160)
                           .fill(swatch));
  }
  // Still cells, so the cost of finding what changed is read against the
  // size of the tree it is found in.
  for (int cell = 0; cell < cells; ++cell)
    children.push_back(box()
                           .absolute()
                           .rect(4.0f + 10.0f * (float)(cell % 100),
                                 4.0f + 10.0f * (float)(cell / 100), 6, 6)
                           .fill(material::Color{.5f, .5f, .5f, 1}));
  children.push_back(box()
                         .absolute()
                         .rect(900, 20, 60, 60)
                         .fill(material::Color{shade, .4f, 1 - shade, 1}));
  children.push_back(text(kWords[frame % 4])
                         .font({.size = 17})
                         .absolute()
                         .rect(900, 90, 100, 22));
  return box().width(1024).height(576).fill(ground).children(
      std::move(children));
}

/** One frame of a page whose description changes every frame, on the CPU
 *  or on the device: range(0) is the backend, zero the CPU and one the
 *  device; range(1) is one where the whole page changes and zero where a
 *  corner of it does; range(2) is how many still cells stand on the page
 *  beside what changes. A device frame is waited for, so the time is the
 *  device's work and not only its submission. */
void PageRepaint(benchmark::State& state) {
  const bool onDevice = state.range(0) == 1;
  const bool wholePage = state.range(1) == 1;
  const int cells = (int)state.range(2);
  const std::shared_ptr<SurfaceScene> page =
      SurfaceScene::make({1024, 576}, fonts());
#ifdef SIGIL_BENCH_GPU
  sigil::skia::GraphiteContext* context = onDevice ? graphite() : nullptr;
  if (onDevice && (!context || !page->useDevice(*gpuDevice(), *context))) {
    state.SkipWithError("no device");
    return;
  }
#else
  if (onDevice) {
    state.SkipWithError("no device");
    return;
  }
#endif
  int frame = 0;
  page->render(specimenPage(frame, wholePage, cells), 0);
  for ([[maybe_unused]] auto iteration : state) {
    ++frame;
    page->render(specimenPage(frame, wholePage, cells), frame / 60.0);
#ifdef SIGIL_BENCH_GPU
    if (onDevice) {
      skgpu::graphite::SubmitInfo submitInfo;
      submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
      const auto lock = context->lockContext();
      context->context()->submit(submitInfo);
    }
#endif
  }
  state.counters["paints"] = (double)page->revision();
}

void cellLadder(::benchmark::Benchmark* b) {
  b->Arg(64)->Arg(256)->Unit(benchmark::kMicrosecond);
}

BENCHMARK(TextureSceneStill)->Apply(cellLadder);
BENCHMARK(TextureScenePaints)->Apply(cellLadder);
BENCHMARK(ComposerDraw)->Apply(cellLadder);
BENCHMARK(TextureValue);
BENCHMARK(PageRepaint)
    ->ArgsProduct({{0, 1}, {0, 1}, {0, 2000}})
    ->ArgNames({"device", "wholePage", "cells"})
    ->Unit(benchmark::kMillisecond);

}  // namespace

}  // namespace sigil::compose::bench
