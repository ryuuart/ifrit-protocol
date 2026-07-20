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
// Draw benchmarks measure the consumer-side cost of compositing an
// already-published frame (Ultralight's own repaint cadence is capped at
// 60 FPS by the SDK edition and is reported by BM_Page_ChangeLatency,
// which times a scripted DOM change until the new frame is published).

#include "BenchGpu.h"

#include <sigilscry/WebEngine.h>
#include <sigilscry/WebImage.h>
#include <sigilscry/WebView.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>

#ifdef __APPLE__
#include "SkiaGraphiteContext.h"

#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#endif

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using namespace sigil::scry;

namespace {

bool g_useGpu = false;

constexpr int kViewWidth = 1280;
constexpr int kViewHeight = 720;

WebEngine &engine() {
  static std::shared_ptr<WebEngine> instance = [] {
    WebEngineConfig config;
    if (g_useGpu) {
      config.metalDevice = bench::gpuDevice();
      config.metalCommandQueue = bench::gpuQueue();
    }
    return WebEngine::create(config);
  }();
  return *instance;
}

/** A shared 1280x720 view showing a card layout, first frame published. */
WebView &benchView() {
  static std::shared_ptr<WebView> view = [] {
    auto v = engine().createView(kViewWidth, kViewHeight,
                                 {.transparent = false});
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
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (v->frameVersion() == 0 &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return v;
  }();
  return *view;
}

#ifdef __APPLE__
SkiaGraphiteContext &graphite() {
  static std::unique_ptr<SkiaGraphiteContext> context =
      SkiaGraphiteContext::createMetal(bench::gpuDevice(),
                                       bench::gpuQueue());
  return *context;
}

void submitGraphite() {
  auto recording = graphite().recorder()->snap();
  if (!recording)
    return;
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  graphite().context()->insertRecording(info);
  graphite().context()->submit();
}
#endif

} // namespace

/** Baseline: acquiring the latest published frame (mutex + ref bump). */
static void BM_Frame_Acquire(benchmark::State &state) {
  WebView &view = benchView();
  for (auto _ : state)
    benchmark::DoNotOptimize(view.frame());
}
BENCHMARK(BM_Frame_Acquire);

/** CPU mode: compositing the published raster frame onto a raster
 *  SkCanvas at full 1280x720 (memcpy-ish blit through Skia). */
static void BM_Draw_RasterCanvas(benchmark::State &state) {
  if (g_useGpu) {
    state.SkipWithMessage("CPU mode only");
    return;
  }
  WebView &view = benchView();
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(kViewWidth, kViewHeight));
  for (auto _ : state)
    view.draw(*surface->getCanvas(),
              SkRect::MakeWH(kViewWidth, kViewHeight));
}
BENCHMARK(BM_Draw_RasterCanvas);

#ifdef __APPLE__

/** GPU mode: acquiring the published frame with its texture wrapped for
 *  a recorder — the per-version cache makes repeats free. */
static void BM_Frame_WrapCached(benchmark::State &state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  WebView &view = benchView();
  for (auto _ : state)
    benchmark::DoNotOptimize(view.frame(graphite().recorder()));
}
BENCHMARK(BM_Frame_WrapCached);

/** GPU mode: the uncached wrap cost, forced by alternating recorders
 *  (what the first acquisition after each publish pays). */
static void BM_Frame_WrapMiss(benchmark::State &state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  WebView &view = benchView();
  static std::unique_ptr<SkiaGraphiteContext> other =
      SkiaGraphiteContext::createMetal(bench::gpuDevice(),
                                       bench::gpuQueue());
  int toggle = 0;
  for (auto _ : state)
    benchmark::DoNotOptimize(view.frame(
        (toggle++ & 1) ? other->recorder() : graphite().recorder()));
}
BENCHMARK(BM_Frame_WrapMiss);

/** GPU mode: recording a full-view draw of the published frame into a
 *  Graphite canvas (no submission). */
static void BM_Draw_GraphiteRecord(benchmark::State &state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  WebView &view = benchView();
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphite().recorder(),
      SkImageInfo::MakeN32Premul(kViewWidth, kViewHeight));
  for (auto _ : state)
    view.draw(*surface->getCanvas(),
              SkRect::MakeWH(kViewWidth, kViewHeight));
  // Snap and DROP the accumulated recording: executing hundreds of
  // thousands of full-screen draws would saturate the GPU and poison the
  // benchmarks that follow.
  graphite().recorder()->snap();
}
BENCHMARK(BM_Draw_GraphiteRecord);

/** GPU mode: draw + snap + insert + submit per frame — the realistic
 *  per-frame consumer cost (GPU executes asynchronously). */
static void BM_Draw_GraphiteSubmit(benchmark::State &state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  WebView &view = benchView();
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphite().recorder(),
      SkImageInfo::MakeN32Premul(kViewWidth, kViewHeight));
  for (auto _ : state) {
    view.draw(*surface->getCanvas(),
              SkRect::MakeWH(kViewWidth, kViewHeight));
    submitGraphite();
  }
}
BENCHMARK(BM_Draw_GraphiteSubmit);

/** GPU mode: feeding a slot from an external native texture (blit). */
static void BM_Slot_UpdateTexture(benchmark::State &state) {
  if (!g_useGpu) {
    state.SkipWithMessage("GPU mode only");
    return;
  }
  const int size = (int)state.range(0);
  static auto image = engine().createImage("bench_ext", 1024, 1024);
  void *texture = bench::makeSolidTexture(size, size);
  for (auto _ : state)
    image->updateTexture(texture);
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
}
BENCHMARK(BM_Slot_UpdateTexture)->Arg(256)->Arg(1024);

#endif // __APPLE__

/** Raster upload into a slot (convert + copy + invalidate). */
static void BM_Slot_UpdateRaster(benchmark::State &state) {
  const int size = (int)state.range(0);
  auto image = engine().createImage("bench_raster_" + std::to_string(size),
                                    size, size);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(SK_ColorCYAN);
  for (auto _ : state)
    image->update(bitmap.pixmap());
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
}
BENCHMARK(BM_Slot_UpdateRaster)->Arg(256)->Arg(1024);

/** paint() into a slot: canvas wrap + draw + flush + invalidate. */
static void BM_Slot_Paint(benchmark::State &state) {
  const int size = (int)state.range(0);
  auto image = engine().createImage("bench_paint_" + std::to_string(size),
                                    size, size);
  for (auto _ : state)
    image->paint([size](SkCanvas &canvas) {
      canvas.clear(SK_ColorDKGRAY);
      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setColor(SK_ColorCYAN);
      canvas.drawCircle(size / 2.0f, size / 2.0f, size / 3.0f, paint);
    });
  state.SetBytesProcessed(state.iterations() * (int64_t)size * size * 4);
}
BENCHMARK(BM_Slot_Paint)->Arg(256)->Arg(1024);

/** Full-pipeline latency: a scripted DOM change until the repainted
 *  frame is published (includes Ultralight's 60 FPS pacing). */
static void BM_Page_ChangeLatency(benchmark::State &state) {
  WebView &view = benchView();
  int toggle = 0;
  for (auto _ : state) {
    uint64_t version = view.frameVersion();
    auto start = std::chrono::steady_clock::now();
    view.evaluateScript(
        "document.getElementById('t').textContent='bench " +
        std::to_string(toggle++) + "';");
    while (view.frameVersion() == version)
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    state.SetIterationTime(
        std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                      start)
            .count());
  }
}
BENCHMARK(BM_Page_ChangeLatency)->UseManualTime()->Unit(benchmark::kMillisecond);

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--gpu") {
      g_useGpu = true;
      for (int j = i; j < argc - 1; ++j)
        argv[j] = argv[j + 1];
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
