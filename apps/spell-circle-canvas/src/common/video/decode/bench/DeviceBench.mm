/** @file
 * The hundred-stream hardware contract: independent VideoToolbox decoders,
 * mixed source resolutions, native YUV wrapping, and one Metal composition.
 */

#import <Metal/Metal.h>

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
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr int kCanvasWidth = 1080;
constexpr int kCanvasHeight = 1920;
constexpr double kSourceRate = 30.0;

struct Options {
  int streams = 100;
  int surfaces = 100;
  int frames = 240;
  int warmup = 60;
  double rate = 120.0;
  size_t workers = 8;
  bool asynchronous = false;
};

Options parse(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string_view argument = argv[i];
    if (argument == "--async")
      options.asynchronous = true;
    else if (argument == "--streams" && i + 1 < argc)
      options.streams = std::max(1, std::atoi(argv[++i]));
    else if (argument == "--surfaces" && i + 1 < argc)
      options.surfaces = std::max(1, std::atoi(argv[++i]));
    else if (argument == "--frames" && i + 1 < argc)
      options.frames = std::max(1, std::atoi(argv[++i]));
    else if (argument == "--warmup" && i + 1 < argc)
      options.warmup = std::max(0, std::atoi(argv[++i]));
    else if (argument == "--rate" && i + 1 < argc)
      options.rate = std::max(1.0, std::atof(argv[++i]));
    else if (argument == "--workers" && i + 1 < argc)
      options.workers = std::max<size_t>(1, std::atoi(argv[++i]));
  }
  return options;
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

double percentile(std::vector<double> values, double fraction) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const size_t index =
      std::min(values.size() - 1, static_cast<size_t>(std::ceil(fraction * values.size())) - 1);
  return values[index];
}

bool submit(sigil::skia::GraphiteContext& graphite) {
  std::unique_ptr<skgpu::graphite::Recording> recording = graphite.recorder()->snap();
  if (!recording) return false;
  skgpu::graphite::InsertRecordingInfo insert;
  insert.fRecording = recording.get();
  return graphite.context()->insertRecording(insert) &&
         graphite.context()->submit(skgpu::graphite::SyncToCpu::kYes);
}

}  // namespace

int main(int argc, char** argv) {
  const Options options = parse(argc, argv);
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) {
    std::fprintf(stderr, "video device bench: no Metal device\n");
    return 2;
  }
  id<MTLCommandQueue> queue = [device newCommandQueue];
  std::unique_ptr<sigil::skia::GraphiteContext> graphite =
      sigil::skia::GraphiteContext::createMetal((__bridge void*)device, (__bridge void*)queue);
  if (!graphite) {
    std::fprintf(stderr, "video device bench: no Graphite Metal context\n");
    return 2;
  }

  constexpr int widths[] = {160, 240, 320, 480, 640};
  constexpr int heights[] = {90, 136, 180, 270, 360};
  const double duration = static_cast<double>(options.warmup + options.frames + 8) / options.rate;
  const int sourceFrames = static_cast<int>(std::ceil(duration * kSourceRate));
  std::vector<sk_sp<SkData>> encoded;
  for (size_t i = 0; i < std::size(widths); ++i) {
    sk_sp<SkData> clip = makeClip(widths[i], heights[i], sourceFrames);
    if (!clip) {
      std::fprintf(stderr, "video device bench: fixture encode failed\n");
      return 2;
    }
    encoded.push_back(std::move(clip));
  }

  const sigil::video::DecodeOptions decodeOptions{
      .hardware = options.asynchronous ? sigil::video::HardwarePreference::Preferred
                                       : sigil::video::HardwarePreference::Required,
      .cachedFrames = 2,
      .metalDevice = (__bridge void*)device};
  std::vector<std::shared_ptr<sigil::video::Video>> players;
  std::unique_ptr<sigil::video::Playback> playback;
  if (options.asynchronous)
    playback = std::make_unique<sigil::video::Playback>(sigil::video::Playback::Options{
        .workerThreads = options.workers, .metalDevice = (__bridge void*)device});
  players.reserve(options.streams);
  for (int i = 0; i < options.streams; ++i) {
    const sk_sp<SkData>& bytes = encoded[static_cast<size_t>(i) % encoded.size()];
    std::shared_ptr<sigil::video::Video> player =
        sigil::video::decodeVideo(static_cast<const std::byte*>(bytes->data()), bytes->size(),
                                  decodeOptions, "device-bench.mp4");
    if (!player || !player->hardwareAccelerated()) {
      std::fprintf(stderr, "video device bench: hardware decoder %d/%d unavailable\n", i + 1,
                   options.streams);
      return 2;
    }
    if (playback) playback->add(player);
    players.push_back(std::move(player));
  }

  const SkImageInfo info = SkImageInfo::MakeN32Premul(kCanvasWidth, kCanvasHeight);
  const sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(graphite->recorder(), info);
  if (!surface) return 2;
  std::vector<SkRect> destinations;
  destinations.reserve(options.surfaces);
  const int columns =
      std::max(1, static_cast<int>(std::ceil(std::sqrt(options.surfaces * 0.5625))));
  const int rows = (options.surfaces + columns - 1) / columns;
  const float cellWidth = static_cast<float>(kCanvasWidth) / columns;
  const float cellHeight = static_cast<float>(kCanvasHeight) / rows;
  for (int i = 0; i < options.surfaces; ++i)
    destinations.push_back(SkRect::MakeXYWH((i % columns) * cellWidth, (i / columns) * cellHeight,
                                            cellWidth + 0.5f, cellHeight + 0.5f));

  size_t nativeFrames = 0;
  size_t missingFrames = 0;
  size_t freshFrames = 0;
  std::vector<sigil::video::VideoFrame> presented(options.streams);
  const auto drawFrame = [&](int frame) {
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorBLACK);
    for (int i = 0; i < options.streams; ++i) {
      // Four phases spread 30 Hz decoder work across a 120 Hz presentation
      // clock instead of forcing every stream to change on the same tick.
      const double phase = (i & 3) / options.rate;
      const double seconds = frame / options.rate + phase;
      sigil::video::VideoFrame& videoFrame = presented[i];
      if (playback) {
        playback->request(i, seconds);
        videoFrame = playback->frame(i, graphite->recorder());
      } else {
        videoFrame = players[i]->frameAt(seconds, graphite->recorder());
      }
      nativeFrames += videoFrame.hardwareDecoded && videoFrame.native ? 1 : 0;
      if (!videoFrame.image) {
        ++missingFrames;
      }
      if (videoFrame && seconds <= videoFrame.presentationSeconds + videoFrame.durationSeconds +
                                       1.0 / kSourceRate)
        ++freshFrames;
    }
    for (int i = 0; i < options.surfaces; ++i) {
      const sigil::video::VideoFrame& videoFrame =
          presented[static_cast<size_t>(i) % presented.size()];
      if (!videoFrame.image) continue;
      SkPaint paint;
      if ((i % 7) == 0) paint.setBlendMode(SkBlendMode::kPlus);
      canvas->drawImageRect(videoFrame.image, destinations[i],
                            SkSamplingOptions(SkFilterMode::kLinear), &paint);
    }
    return submit(*graphite);
  };

  auto deadline = std::chrono::steady_clock::now();
  const auto period = std::chrono::duration<double>(1.0 / options.rate);
  for (int frame = 0; frame < options.warmup; ++frame) {
    if (!drawFrame(frame)) return 2;
    if (playback) {
      deadline += std::chrono::duration_cast<std::chrono::steady_clock::duration>(period);
      std::this_thread::sleep_until(deadline);
    }
  }
  nativeFrames = 0;
  missingFrames = 0;
  freshFrames = 0;
  std::vector<double> samples;
  samples.reserve(options.frames);
  for (int frame = options.warmup; frame < options.warmup + options.frames; ++frame) {
    const auto start = std::chrono::steady_clock::now();
    if (!drawFrame(frame)) return 2;
    const auto end = std::chrono::steady_clock::now();
    samples.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    if (playback) {
      deadline += std::chrono::duration_cast<std::chrono::steady_clock::duration>(period);
      std::this_thread::sleep_until(deadline);
    }
  }

  const double total = std::accumulate(samples.begin(), samples.end(), 0.0);
  const double mean = total / samples.size();
  const double p50 = percentile(samples, 0.50);
  const double p95 = percentile(samples, 0.95);
  const double p99 = percentile(samples, 0.99);
  const double maximum = *std::max_element(samples.begin(), samples.end());
  const size_t expected = static_cast<size_t>(options.streams) * options.frames;
  const double nativePercent =
      expected ? 100.0 * static_cast<double>(nativeFrames) / expected : 0.0;
  const double readyPercent =
      expected ? 100.0 * static_cast<double>(expected - missingFrames) / expected : 0.0;
  const double freshPercent = expected ? 100.0 * static_cast<double>(freshFrames) / expected : 0.0;
  const double budget = 1000.0 / options.rate;
  const bool passed = p99 <= budget && readyPercent >= 99.0 && freshPercent >= 99.0 &&
                      (options.asynchronous || nativeFrames == expected);
  std::printf("VIDEO_DEVICE mode=%s streams=%d surfaces=%d sources=%zu "
              "canvas=%dx%d rate=%.1f "
              "native=%.2f%% ready=%.2f%% fresh=%.2f%% mean=%.3fms "
              "p50=%.3fms p95=%.3fms p99=%.3fms max=%.3fms budget=%.3fms "
              "result=%s\n",
              options.asynchronous ? "async" : "sync", options.streams, options.surfaces,
              encoded.size(), kCanvasWidth, kCanvasHeight, options.rate, nativePercent,
              readyPercent, freshPercent, mean, p50, p95, p99, maximum, budget,
              passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
