// Benchmarks for the SigilScry integration paths — what each compositing
// direction costs per frame, so scene-render budgets can be planned.
//
// One Ultralight renderer per process, so CPU and GPU modes are separate
// runs of the same binary:
//
//   scry_bench          # CPU engine (raster SkImage frames)
//   scry_bench --gpu    # GPU engine (Metal driver, texture frames)
//
// Benchmarks that only exist in one mode skip themselves in the other.
//
// The draw arms isolate the CONSUMER side: they composite a frame that is
// already published, so nothing they time depends on how often Ultralight
// chooses to repaint. Producer-side cost sits in one arm of its own,
// BM_Page_ChangeLatency, which times a scripted DOM change until the new
// frame is published and therefore includes the engine's own pacing.

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <sigilscry/WebEngine.h>
#include <sigilscry/WebImage.h>
#include <sigilscry/WebView.h>

#include "BenchGpu.h"

#ifdef __APPLE__
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#endif

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace sigil::scry;

namespace {

bool g_useGpu = false;

constexpr int kViewWidth = 1280;
constexpr int kViewHeight = 720;

#ifdef __APPLE__
sigil::skia::GraphiteContext& graphite();
#endif

WebEngine& engine() {
  static std::shared_ptr<WebEngine> instance = [] {
    WebEngineConfig config;
    if (g_useGpu) {
      config.gpuDevice = bench::gpuDevice();
      config.graphite = &graphite();
    }
    return WebEngine::create(config);
  }();
  return *instance;
}

/** One view showing a card layout, shared by every arm and not returned
 *  until its first frame is published, so no benchmark pays for page load
 *  or waits on a frame that does not exist yet. */
WebView& benchView() {
  static std::shared_ptr<WebView> view = [] {
    auto v =
        engine().createView(kViewWidth, kViewHeight, {.transparent = false});
    v->loadHTML(
        "<html><body style='margin:0;background:#123'>"
        "<div style='display:grid;grid-template-columns:repeat(4,1fr);"
        "gap:12px;padding:12px'>"
        "<div style='background:#fff;border-radius:12px;height:160px;"
        "box-shadow:0 6px 18px rgba(0,0,0,.4)'></div>"
        "<div style='background:#fda;border-radius:12px'></div>"
        "<div style='background:#adf;border-radius:12px'></div>"
        "<div style='background:#dfa;border-radius:12px'></div>"
        "</div><h1 id='t' style='color:#fff;padding:12px'>bench</h1>"
        "</body></html>");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (v->frameVersion() == 0 &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return v;
  }();
  return *view;
}

#ifdef __APPLE__
/** The Graphite context the bench draws with and the engine shares; the
 *  web thread records on its own recorder over it, so every context call
 *  here holds lockContext(). */
sigil::skia::GraphiteContext& graphite() {
  static std::unique_ptr<sigil::skia::GraphiteContext> context =
      sigil::skia::GraphiteContext::create(*bench::gpuDevice());
  return *context;
}

void submitGraphite() {
  auto recording = graphite().recorder()->snap();
  const std::unique_lock<std::mutex> lock = graphite().lockContext();
  if (!recording) return;
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  graphite().context()->insertRecording(info);
  graphite().context()->submit();
}
#endif

}  // namespace

/** Baseline: acquiring the latest published frame (mutex + ref bump). */
static void BM_Frame_Acquire(benchmark::State& state) {
  WebView& view = benchView();
  for ([[maybe_unused]] auto _ : state) benchmark::DoNotOptimize(view.frame());
}
BENCHMARK(BM_Frame_Acquire);

/** CPU mode: compositing the published raster frame onto a raster
 *  SkCanvas at full 1280x720 (memcpy-ish blit through Skia). */
static void BM_Draw_RasterCanvas(benchmark::State& state) {
  if (g_useGpu) {
    state.SkipWithMessage("CPU mode only");
    return;
  }
  WebView& view = benchView();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kViewWidth, kViewHeight));
  for ([[maybe_unused]] auto _ : state)
    view.draw(*surface->getCanvas(), SkRect::MakeWH(kViewWidth, kViewHeight));
}
BENCHMARK(BM_Draw_RasterCanvas);

#ifdef __APPLE__

/** GPU mode: acquiring the published frame with its texture wrapped for a
 *  recorder. The view caches one wrap per (frame version, recorder), and
 *  nothing here publishes a new frame or changes recorder, so every
 *  iteration after the first takes the cached path. */
static void BM_Frame_WrapCached(benchmark::State& state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  WebView& view = benchView();
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(view.frame(graphite().recorder()));
}
BENCHMARK(BM_Frame_WrapCached);

/** GPU mode: the uncached wrap, isolated by alternating between two
 *  recorders so the cache key differs on every iteration. This is the path
 *  a real consumer takes on its first acquisition after each publish. */
static void BM_Frame_WrapMiss(benchmark::State& state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  WebView& view = benchView();
  static std::unique_ptr<sigil::skia::GraphiteContext> other =
      sigil::skia::GraphiteContext::create(*bench::gpuDevice());
  unsigned toggle = 0;
  for ([[maybe_unused]] auto _ : state)
    benchmark::DoNotOptimize(view.frame(
        (toggle++ & 1u) ? other->recorder() : graphite().recorder()));
}
BENCHMARK(BM_Frame_WrapMiss);

/** GPU mode: recording a full-view draw of the published frame into a
 *  Graphite canvas (no submission). */
static void BM_Draw_GraphiteRecord(benchmark::State& state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  WebView& view = benchView();
  // Taken from the context, which builds every recorder with the caching
  // image provider — not optional: Graphite silently DROPS any draw that
  // samples a raster (non-Graphite) image when the recorder has no
  // provider to promote it, and a recorder made bare from the Skia
  // context would leave this arm timing draws that never happen.
  //
  // A recorder of its own, deliberately, and a FRESH one per invocation.
  // This arm times recording alone and never submits — actually executing
  // one full-screen draw per iteration would saturate the GPU and distort
  // every benchmark that follows — so it snaps and DROPS the accumulated
  // recording. Those same options also require ordered recordings, and a
  // dropped snap burns an ID in the recorder's chain: every later insert
  // from that recorder then fails, silently and permanently. Google
  // Benchmark invokes this function more than once (iteration-count
  // estimation, and again under --benchmark_repetitions), so a recorder
  // that survived invocations would carry that poison into every one after
  // the first; a recorder that lives and dies with one invocation cannot.
  // Both allocations sit outside the timed loop, so the timing is
  // unaffected.
  //
  // Retired, not freed: WebView caches its Graphite wrap keyed on the raw
  // Recorder*, so a freed recorder whose address the next invocation's
  // allocation reuses would cache-hit on a dangling key. Parking each
  // spent recorder in a static list keeps every address distinct for the
  // life of the process, and the handful of invocations the framework
  // makes keeps the list small.
  static std::vector<std::unique_ptr<skgpu::graphite::Recorder>> retired;
  retired.push_back(graphite().makeRecorder());
  skgpu::graphite::Recorder* recorder = retired.back().get();
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      recorder, SkImageInfo::MakeN32Premul(kViewWidth, kViewHeight));
  for ([[maybe_unused]] auto _ : state)
    view.draw(*surface->getCanvas(), SkRect::MakeWH(kViewWidth, kViewHeight));
  recorder->snap();
}
BENCHMARK(BM_Draw_GraphiteRecord);

/** GPU mode: draw + snap + insert + submit per iteration — the shape of a
 *  real consumer frame. submit() returns once the work is queued, so this
 *  is the host-side cost of a frame and not the GPU's execution of it. */
static void BM_Draw_GraphiteSubmit(benchmark::State& state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  WebView& view = benchView();
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphite().recorder(),
      SkImageInfo::MakeN32Premul(kViewWidth, kViewHeight));
  for ([[maybe_unused]] auto _ : state) {
    view.draw(*surface->getCanvas(), SkRect::MakeWH(kViewWidth, kViewHeight));
    submitGraphite();
  }
}
BENCHMARK(BM_Draw_GraphiteSubmit);

/** GPU mode: feeding a slot from an external native texture (blit). */
static void BM_Slot_UpdateTexture(benchmark::State& state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  const int size = (int)state.range(0);
  static auto image = engine().createImage("bench_ext", 1024, 1024);
  const sigil::skia::TextureHandle texture =
      bench::makeSolidTexture(size, size);
  for ([[maybe_unused]] auto _ : state) image->updateTexture(texture);
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
}
BENCHMARK(BM_Slot_UpdateTexture)->Arg(256)->Arg(1024);

#endif  // __APPLE__

/** Raster upload into a slot (convert + copy + invalidate). */
static void BM_Slot_UpdateRaster(benchmark::State& state) {
  const int size = (int)state.range(0);
  auto image =
      engine().createImage("bench_raster_" + std::to_string(size), size, size);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(SK_ColorCYAN);
  for ([[maybe_unused]] auto _ : state) image->update(bitmap.pixmap());
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
}
BENCHMARK(BM_Slot_UpdateRaster)->Arg(256)->Arg(1024);

/** paint() into a slot: canvas wrap + draw + flush + invalidate. */
static void BM_Slot_Paint(benchmark::State& state) {
  const int size = (int)state.range(0);
  auto image =
      engine().createImage("bench_paint_" + std::to_string(size), size, size);
  for ([[maybe_unused]] auto _ : state)
    image->paint([size](SkCanvas& canvas) {
      canvas.clear(SK_ColorDKGRAY);
      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setColor(SK_ColorCYAN);
      canvas.drawCircle(size / 2.0f, size / 2.0f, size / 3.0f, paint);
    });
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
}
BENCHMARK(BM_Slot_Paint)->Arg(256)->Arg(1024);

/** Full-pipeline latency: a scripted DOM change until the repainted frame
 *  is published. The only arm that includes the engine's own repaint
 *  pacing, so it is a latency figure and not a per-frame cost. */
static void BM_Page_ChangeLatency(benchmark::State& state) {
  WebView& view = benchView();
  int toggle = 0;
  for ([[maybe_unused]] auto _ : state) {
    uint64_t version = view.frameVersion();
    auto start = std::chrono::steady_clock::now();
    view.evaluateScript("document.getElementById('t').textContent='bench " +
                        std::to_string(toggle++) + "';");
    while (view.frameVersion() == version)
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    state.SetIterationTime(
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count());
  }
}
BENCHMARK(BM_Page_ChangeLatency)
    ->UseManualTime()
    ->Unit(benchmark::kMillisecond);

int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape): an
                                   // uncaught error ends the run
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--gpu") {
      g_useGpu = true;
      for (int j = i; j < argc - 1; ++j) argv[j] = argv[j + 1];
      --argc;
      break;
    }
  }
  if (g_useGpu && !bench::gpuDevice()) {
    std::fprintf(stderr, "no GPU backend on this platform\n");
    return 1;
  }
  std::printf("engine mode: %s\n", g_useGpu ? "GPU" : "CPU");

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
