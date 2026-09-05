/** @file
 * video_device_bench — many independent video clocks through the device
 * path: platform decoders over mixed source resolutions, native YUV planes
 * wrapped as Metal textures, and one Graphite composition per presented
 * frame.
 *
 * Two arms measure the two ways a host drives that path. BM_RenderThread
 * pulls every stream with `Video::frameAt` on the thread that presents, so
 * each decode lands inside the tick that shows it; its sustained cost is
 * what a serial path can promise, since a source-frame boundary arrives as
 * a burst it cannot hide. BM_WorkerPool drives the same streams through
 * `Playback`, where the render thread only asks for a time and reads the
 * last complete frame; that arm is paced at the presentation rate, times
 * the render thread's own work alone, and is judged on its worst tick,
 * because every tick of a presentation path has to fit.
 *
 * A device grants a limited number of simultaneous hardware decompression
 * sessions, and it grants them on the first decode rather than when a
 * decoder opens. A stream that is refused one decodes in software and is
 * counted; the VIDEO_DEVICE line each arm prints states how many sessions
 * the device granted and what fraction of the presented frames came off
 * the device. The exit status says whether the measurement could be taken
 * at all — the budget verdict is on the line, and a regression is the bench
 * ledger's to call.
 *
 * Run a Release build; Debug numbers say nothing.
 */

#import <Metal/Metal.h>

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/decode/Playback.h>
#include <sigilvideo/encode/Encode.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr int kCanvasWidth = 1080;
constexpr int kCanvasHeight = 1920;
constexpr double kSourceRate = 30.0;
constexpr double kClipSeconds = 4.0;

// One thread decoding every stream serially spends its whole presentation
// tick inside frameAt, so the render-thread arm carries the few streams one
// thread can pull while the worker pool carries many. Both counts stay
// inside the simultaneous hardware sessions a device grants, so the arms
// measure the device path rather than the point where the device stops
// granting sessions; `--streams` pushes past that point deliberately, and
// the native percentage then reports where it lies on this machine.
constexpr int kRenderThreadStreams = 4;
constexpr int kWorkerPoolStreams = 16;

struct Settings {
  int streams = 0;   // Zero: each arm's own count.
  int surfaces = 0;  // Zero: one destination per stream.
  double rate = 120.0;
  size_t workers = 8;
};

Settings& settings() {
  static Settings instance;
  return instance;
}

void parse(int argc, char** argv) {
  Settings& options = settings();
  for (int i = 1; i < argc; ++i) {
    const std::string_view argument = argv[i];
    if (argument == "--streams" && i + 1 < argc)
      options.streams = std::max(1, std::atoi(argv[++i]));
    else if (argument == "--surfaces" && i + 1 < argc)
      options.surfaces = std::max(1, std::atoi(argv[++i]));
    else if (argument == "--rate" && i + 1 < argc)
      options.rate = std::max(1.0, std::atof(argv[++i]));
    else if (argument == "--workers" && i + 1 < argc)
      options.workers = std::max<size_t>(1, std::atoi(argv[++i]));
  }
}

id<MTLDevice> device() {
  static id<MTLDevice> instance = MTLCreateSystemDefaultDevice();
  return instance;
}

sigil::skia::GraphiteContext* graphite() {
  static std::unique_ptr<sigil::skia::GraphiteContext> instance = [] {
    if (!device()) return std::unique_ptr<sigil::skia::GraphiteContext>();
    id<MTLCommandQueue> queue = [device() newCommandQueue];
    return sigil::skia::GraphiteContext::createMetal((__bridge void*)device(),
                                                     (__bridge void*)queue);
  }();
  return instance.get();
}

sk_sp<SkData> makeClip(int width, int height, int frames) {
  std::unique_ptr<sigil::video::Encoder> encoder = sigil::video::Encoder::make(
      sigil::video::Format::Mp4, {.width = width,
                                  .height = height,
                                  .framesPerSecond = static_cast<int>(kSourceRate),
                                  .bitRate = std::max(180'000, width * height * 5),
                                  .hardware = sigil::video::HardwarePreference::Disabled});
  if (!encoder) return nullptr;
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  for (int frame = 0; frame < frames; ++frame) {
    const unsigned phase = static_cast<unsigned>(frame * 17);
    pixels.eraseColor(
        SkColorSetARGB(255, 24 + phase % 208, 24 + (phase * 3) % 208, 24 + (phase * 7) % 208));
    if (!encoder->append(pixels.pixmap())) return nullptr;
  }
  return encoder->finish();
}

/** The fixture clips, encoded once: five source resolutions, each long
 *  enough that a presentation clock loops over it rather than running off
 *  its end. Empty when the encoder cannot produce them. */
const std::vector<sk_sp<SkData>>& clips() {
  static std::vector<sk_sp<SkData>> encoded = [] {
    constexpr int widths[] = {160, 240, 320, 480, 640};
    constexpr int heights[] = {90, 136, 180, 270, 360};
    const int frames = static_cast<int>(kClipSeconds * kSourceRate);
    std::vector<sk_sp<SkData>> made;
    for (size_t i = 0; i < std::size(widths); ++i) {
      sk_sp<SkData> clip = makeClip(widths[i], heights[i], frames);
      if (!clip) return std::vector<sk_sp<SkData>>();
      made.push_back(std::move(clip));
    }
    return made;
  }();
  return encoded;
}

double percentile(std::vector<double> values, double fraction) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const size_t index =
      std::min(values.size() - 1, static_cast<size_t>(std::ceil(fraction * values.size())) - 1);
  return values[index];
}

/** What one arm presented, printed once the run is over so the numbers do
 *  not interleave with the benchmark library's own report. It covers every
 *  frame the arm presented after its warm-up, across all of its
 *  repetitions, which is what gives the tail of the distribution enough
 *  samples to mean something. */
struct Report {
  std::string arm;
  int streams = 0;
  int surfaces = 0;
  int nativeStreams = 0;
  size_t frames = 0;
  size_t native = 0;
  size_t missing = 0;
  size_t fresh = 0;
  double budgetMilliseconds = 0.0;
  bool judgeWorstTick = false;
  std::vector<double> samples;
};

std::vector<Report>& reports() {
  static std::vector<Report> instance;
  return instance;
}

void print(const Report& report) {
  if (report.samples.empty()) return;
  const double mean =
      std::accumulate(report.samples.begin(), report.samples.end(), 0.0) / report.samples.size();
  const double p50 = percentile(report.samples, 0.50);
  const double p95 = percentile(report.samples, 0.95);
  const double p99 = percentile(report.samples, 0.99);
  const double maximum = *std::max_element(report.samples.begin(), report.samples.end());
  const double frames = static_cast<double>(report.frames);
  const double nativePercent = frames ? 100.0 * report.native / frames : 0.0;
  const double readyPercent = frames ? 100.0 * (frames - report.missing) / frames : 0.0;
  const double freshPercent = frames ? 100.0 * report.fresh / frames : 0.0;
  const double judged = report.judgeWorstTick ? p99 : mean;
  const bool passed =
      judged <= report.budgetMilliseconds && readyPercent >= 99.0 && freshPercent >= 99.0;
  std::printf("VIDEO_DEVICE arm=%s streams=%d surfaces=%d sessions=%d/%d sources=%zu "
              "canvas=%dx%d rate=%.1f native=%.2f%% ready=%.2f%% fresh=%.2f%% "
              "mean=%.3fms p50=%.3fms p95=%.3fms p99=%.3fms max=%.3fms "
              "budget=%.3fms judged=%s result=%s\n",
              report.arm.c_str(), report.streams, report.surfaces, report.nativeStreams,
              report.streams, clips().size(), kCanvasWidth, kCanvasHeight, settings().rate,
              nativePercent, readyPercent, freshPercent, mean, p50, p95, p99, maximum,
              report.budgetMilliseconds, report.judgeWorstTick ? "p99" : "mean",
              passed ? "PASS" : "FAIL");
  std::fflush(stdout);
}

/** The streams, the destination grid and the surface one arm presents
 *  through. One harness exists at a time: a hardware decompression session
 *  lives as long as its decoder, and an arm that is done with its streams
 *  has to give the device its sessions back before the next arm asks. */
class Harness {
 public:
  Harness(std::string arm, bool asynchronous, int defaultStreams)
      : m_asynchronous(asynchronous) {
    const Settings& options = settings();
    m_report.arm = std::move(arm);
    m_report.streams = options.streams > 0 ? options.streams : defaultStreams;
    m_report.surfaces = options.surfaces > 0 ? options.surfaces : m_report.streams;
    m_report.budgetMilliseconds = 1000.0 / options.rate;
    m_report.judgeWorstTick = asynchronous;

    const sigil::video::DecodeOptions decodeOptions{
        // Preferred, not Required, in both arms: a stream the device
        // refuses a session decodes in software and is reported as such,
        // which is the fact this bench exists to measure. Rejecting a
        // frame that arrives on the wrong surface is the decode test's
        // contract, not this one's.
        .hardware = sigil::video::HardwarePreference::Preferred,
        .cachedFrames = 2,
        .metalDevice = (__bridge void*)device()};
    if (asynchronous)
      m_playback = std::make_unique<sigil::video::Playback>(sigil::video::Playback::Options{
          .workerThreads = options.workers, .metalDevice = (__bridge void*)device()});
    m_players.reserve(m_report.streams);
    for (int i = 0; i < m_report.streams; ++i) {
      const sk_sp<SkData>& bytes = clips()[static_cast<size_t>(i) % clips().size()];
      std::shared_ptr<sigil::video::Video> player =
          sigil::video::decodeVideo(static_cast<const std::byte*>(bytes->data()), bytes->size(),
                                    decodeOptions, "device-bench.mp4");
      if (!player) {
        m_error = "a fixture clip did not open";
        return;
      }
      // The device is asked for a session by the first decode, not by
      // opening the decoder, so the surface a stream will use is only
      // knowable after one frame has come back.
      player->decodeAt(0.0);
      if (player->hardwareDecoding()) ++m_report.nativeStreams;
      if (m_playback) m_playback->add(player);
      m_players.push_back(std::move(player));
    }
    if (m_report.nativeStreams < m_report.streams)
      std::fprintf(stderr,
                   "video device bench: the device granted %d of %d hardware decompression "
                   "sessions; the remaining streams decode in software\n",
                   m_report.nativeStreams, m_report.streams);

    m_presented.resize(m_report.streams);
    m_surface = SkSurfaces::RenderTarget(graphite()->recorder(),
                                         SkImageInfo::MakeN32Premul(kCanvasWidth, kCanvasHeight));
    if (!m_surface) {
      m_error = "the Graphite render target was refused";
      return;
    }
    const int columns =
        std::max(1, static_cast<int>(std::ceil(std::sqrt(m_report.surfaces * 0.5625))));
    const int rows = (m_report.surfaces + columns - 1) / columns;
    const float cellWidth = static_cast<float>(kCanvasWidth) / columns;
    const float cellHeight = static_cast<float>(kCanvasHeight) / rows;
    m_destinations.reserve(m_report.surfaces);
    for (int i = 0; i < m_report.surfaces; ++i)
      m_destinations.push_back(SkRect::MakeXYWH((i % columns) * cellWidth,
                                                (i / columns) * cellHeight, cellWidth + 0.5f,
                                                cellHeight + 0.5f));
    warmUp();
  }

  ~Harness() {
    if (!m_report.samples.empty()) reports().push_back(m_report);
  }

  bool matches(std::string_view arm) const { return m_report.arm == arm; }
  const char* error() const { return m_error; }
  bool paced() const { return m_asynchronous; }

  /** One presented frame: every stream asked for its time, every
   *  destination drawn, the recording inserted and submitted. Answers the
   *  seconds it took, or a negative number when the device refused the
   *  submission. */
  double present() {
    const auto start = std::chrono::steady_clock::now();
    const bool submitted = draw(true);
    const auto end = std::chrono::steady_clock::now();
    if (!submitted) return -1.0;
    const double seconds = std::chrono::duration<double>(end - start).count();
    m_report.samples.push_back(seconds * 1000.0);
    return seconds;
  }

 private:
  void warmUp() {
    // The pool needs frames in flight and every decoder needs its first
    // seek behind it before a number means anything; a caller that runs
    // the binary by hand gets the same settled state the ledger's own
    // warm-up period would have produced.
    const int frames = static_cast<int>(settings().rate / 2.0);
    auto deadline = std::chrono::steady_clock::now();
    const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / settings().rate));
    for (int frame = 0; frame < frames; ++frame) {
      draw(false);
      if (!m_asynchronous) continue;
      deadline += period;
      std::this_thread::sleep_until(deadline);
    }
    m_report.frames = 0;
    m_report.native = 0;
    m_report.missing = 0;
    m_report.fresh = 0;
    m_report.samples.clear();
  }

  bool draw(bool counted) {
    SkCanvas* canvas = m_surface->getCanvas();
    canvas->clear(SK_ColorBLACK);
    const double rate = settings().rate;
    for (int i = 0; i < m_report.streams; ++i) {
      // Four phases spread source-frame boundaries across presentation
      // ticks instead of forcing every stream to decode on the same one.
      const double phase = (i & 3) / rate;
      const double seconds = std::fmod(m_clock / rate + phase, kClipSeconds);
      sigil::video::VideoFrame& videoFrame = m_presented[i];
      if (m_playback) {
        m_playback->request(i, seconds);
        videoFrame = m_playback->frame(i, graphite()->recorder());
      } else {
        videoFrame = m_players[i]->frameAt(seconds, graphite()->recorder());
      }
      if (!counted) continue;
      ++m_report.frames;
      if (videoFrame.hardwareDecoded && videoFrame.native) ++m_report.native;
      if (!videoFrame.image) ++m_report.missing;
      // Fresh is the frame the clock is inside, so the last frame of the
      // previous loop is stale the moment the clock wraps to its start.
      if (videoFrame && videoFrame.presentationSeconds <= seconds &&
          seconds <= videoFrame.presentationSeconds + videoFrame.durationSeconds +
                         1.0 / kSourceRate)
        ++m_report.fresh;
    }
    ++m_clock;
    for (int i = 0; i < m_report.surfaces; ++i) {
      const sigil::video::VideoFrame& videoFrame =
          m_presented[static_cast<size_t>(i) % m_presented.size()];
      if (!videoFrame.image) continue;
      SkPaint paint;
      if ((i % 7) == 0) paint.setBlendMode(SkBlendMode::kPlus);
      canvas->drawImageRect(videoFrame.image, m_destinations[i],
                            SkSamplingOptions(SkFilterMode::kLinear), &paint);
    }
    std::unique_ptr<skgpu::graphite::Recording> recording = graphite()->recorder()->snap();
    if (!recording) return false;
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    return graphite()->context()->insertRecording(insert) &&
           graphite()->context()->submit(skgpu::graphite::SyncToCpu::kYes);
  }

  bool m_asynchronous = false;
  const char* m_error = nullptr;
  Report m_report;
  std::vector<std::shared_ptr<sigil::video::Video>> m_players;
  std::unique_ptr<sigil::video::Playback> m_playback;
  std::vector<sigil::video::VideoFrame> m_presented;
  sk_sp<SkSurface> m_surface;
  std::vector<SkRect> m_destinations;
  int m_clock = 0;
};

std::unique_ptr<Harness>& harness() {
  static std::unique_ptr<Harness> instance;
  return instance;
}

Harness& harnessFor(const char* arm, bool asynchronous, int defaultStreams) {
  if (!harness() || !harness()->matches(arm)) {
    harness().reset();
    harness() = std::make_unique<Harness>(arm, asynchronous, defaultStreams);
  }
  return *harness();
}

void run(benchmark::State& state, const char* arm, bool asynchronous, int defaultStreams) {
  Harness& active = harnessFor(arm, asynchronous, defaultStreams);
  if (active.error()) {
    state.SkipWithError(active.error());
    return;
  }
  auto deadline = std::chrono::steady_clock::now();
  const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(1.0 / settings().rate));
  for ([[maybe_unused]] auto _ : state) {
    const double seconds = active.present();
    if (seconds < 0.0) {
      state.SkipWithError("the device refused a recording");
      return;
    }
    if (!active.paced()) continue;
    state.SetIterationTime(seconds);
    deadline += period;
    std::this_thread::sleep_until(deadline);
  }
}

void BM_RenderThread(benchmark::State& state) {
  run(state, "render-thread", false, kRenderThreadStreams);
}
BENCHMARK(BM_RenderThread);

void BM_WorkerPool(benchmark::State& state) {
  run(state, "worker-pool", true, kWorkerPoolStreams);
}
BENCHMARK(BM_WorkerPool)->UseManualTime();

}  // namespace

int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape): an
                                   // uncaught error ends the run
  parse(argc, argv);
  benchmark::Initialize(&argc, argv);
  if (!device() || !graphite()) {
    std::fprintf(stderr, "video device bench: no Metal device on this machine\n");
    return 1;
  }
  if (clips().empty()) {
    std::fprintf(stderr, "video device bench: the fixture clips did not encode\n");
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  harness().reset();
  for (const Report& report : reports()) print(report);
  return 0;
}
