#pragma once

/** @file
 * What every compose benchmark binary shares: the font context, a composer
 * over a raster surface, the node-count ladder the scaling arms walk, and —
 * when the binary is built with SIGIL_BENCH_GPU — one Graphite
 * context over a GPU device of the process's own, with a render target
 * acquired per arm.
 *
 * Every binary is built in Release before its numbers mean anything; a Debug
 * timing says nothing about the library.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Element.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cstdint>
#include <memory>

#ifdef SIGIL_BENCH_GPU
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#endif

namespace sigil::compose::bench {

/** One font context for the process: shaping caches warm once and every
 *  arm measures the library rather than the system font manager. */
inline sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

/** A composer sized to a raster surface. `draw()` is the frame. */
struct Host {
  sigil::motion::Ticker ticker;
  Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface;

  explicit Host(int width = 800, int height = 2400) {
    composer.setSize({(float)width, (float)height});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  }

  SkCanvas& canvas() { return *surface->getCanvas(); }
  void draw() { composer.draw(canvas()); }
};

/** Reports the node count as a counter and as items processed, so a
 *  scaling arm's rows can be read per node as well as per iteration. */
inline void reportNodes(benchmark::State& state, int count) {
  state.counters["nodes"] = (double)count;
  state.SetItemsProcessed(state.iterations() * (int64_t)count);
}

/** The node-count ladder the reconcile and layout arms walk: three points
 *  a decade apart give a curve rather than a number. */
inline void nodeLadder(::benchmark::Benchmark* b) {
  b->Arg(100)->Arg(500)->Arg(2000)->Unit(benchmark::kMicrosecond);
}

/** A cell fill that varies with the id, with one cell singled out when
 *  `changed` names it and `phase` is odd — the one-node-changed arms flip
 *  that cell every iteration. */
inline Fill cellFill(int id, int changed = -1, int phase = 0) {
  if (id == changed && phase != 0)
    return Fill::color({0.95f, 0.35f, 0.18f, 1.0f});
  const float tint = 0.20f + 0.04f * (float)(id % 6);
  return Fill::color({tint, 0.45f, 0.68f, 1.0f});
}

#ifdef SIGIL_BENCH_GPU

/** The process's GPU device, created on first use; null where there is
 *  none. */
inline sigil::core::hardware::GpuDevice* gpuDevice() {
  static std::unique_ptr<sigil::core::hardware::GpuDevice> device =
      sigil::core::hardware::GpuDevice::createOwned();
  return device.get();
}

/** The process's Graphite context over that device, created on first use.
 *  Null when the device is unavailable, which every GPU arm turns into a
 *  skip. */
inline sigil::skia::GraphiteContext* graphite() {
  static std::unique_ptr<sigil::skia::GraphiteContext> ctx = [] {
    sigil::core::hardware::GpuDevice* device = gpuDevice();
    return device ? sigil::skia::GraphiteContext::create(*device) : nullptr;
  }();
  return ctx.get();
}

/** Snap, insert and submit WITHOUT waiting: arms that compare many cheap
 *  iterations against each other use this, where queue back-pressure is
 *  itself part of the signal. */
inline void submitGraphite(sigil::skia::GraphiteContext& graphiteContext) {
  auto recording = graphiteContext.recorder()->snap();
  if (!recording) return;
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  graphiteContext.context()->insertRecording(info);
  graphiteContext.context()->submit();
}

/** Submit and WAIT for the GPU to finish. An arm comparing SHADER cost must
 *  use this: an unsynced submit times only the CPU handing work over, which
 *  can rank the most expensive shader as the cheapest because its queue
 *  never drains inside the timed region. */
inline void submitGraphiteSynced(
    sigil::skia::GraphiteContext& graphiteContext) {
  auto recording = graphiteContext.recorder()->snap();
  if (!recording) return;
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  graphiteContext.context()->insertRecording(info);
  skgpu::graphite::SubmitInfo submitInfo;
  submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
  graphiteContext.context()->submit(submitInfo);
}

/** A Graphite render target for one arm. `ok()` is false — and the state
 *  already carries the skip — when the context or the surface could not be
 *  made, so an arm reads `if (!target.ok()) return;` and nothing else. */
struct GraphiteTarget {
  sigil::skia::GraphiteContext* context = nullptr;
  sk_sp<SkSurface> surface;

  GraphiteTarget(benchmark::State& state, int width, int height) {
    context = graphite();
    if (!context) {
      state.SkipWithError("Graphite Metal context is unavailable");
      return;
    }
    surface = SkSurfaces::RenderTarget(
        context->recorder(), SkImageInfo::MakeN32Premul(width, height));
    if (!surface) {
      state.SkipWithError("Graphite render target creation failed");
      context = nullptr;
    }
  }

  bool ok() const { return context != nullptr && surface != nullptr; }
  SkCanvas& canvas() { return *surface->getCanvas(); }
  void submit() { submitGraphite(*context); }
  void submitSynced() { submitGraphiteSynced(*context); }
};

#endif  // SIGIL_BENCH_GPU

}  // namespace sigil::compose::bench
