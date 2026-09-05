/** @file
 * The headless sweep: the timing table, and the plate.
 */

#include "sigilsketch/plate/Sweep.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/source/Sink.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/plate/FrameStats.h>
#include <sigilsketch/plate/Graphite.h>

#include <include/gpu/GpuTypes.h>  // skgpu::GpuStatsFlags
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace sigil::sketch {

namespace {

/** The one rate every sweep walks at, and the step it implies. A plate
 *  is a function of the declared moment, so neither may depend on
 *  anything a machine decides — and a frame count is derived by
 *  MULTIPLYING by the rate rather than dividing by the step, because the
 *  step is not exactly representable and the two answers are not
 *  guaranteed to round alike. */
constexpr double kRate = 60.0;
constexpr double kStep = 1.0 / kRate;

/** The caps the capture frame is DERIVED from, so raising one moves the
 *  capture with it rather than leaving the top-up silently short. */
constexpr int kProbeFrames = 8;
constexpr int kMinSampleFrames = 24;
constexpr int kMaxWarmFrames = 240 - kProbeFrames;
constexpr int kMaxSampleFrames = 120;
constexpr int kCaptureFrame = kProbeFrames + kMaxWarmFrames + kMaxSampleFrames;

/** A plate on disk: encoded once, written once. The plate ledger hashes
 *  what lands here, so the encode is the picture's identity and a
 *  half-written file must read as a failure rather than as a plate. */
bool writePlate(const SkPixmap& pixels, const std::filesystem::path& path) {
  const sk_sp<SkData> png = image::encodeImage(pixels, image::Format::Png);
  return png && io::writeBytes(path, png->data(), png->size());
}

}  // namespace

int sweep(const SweepOptions& options, weave::FontContext& fonts,
          Assets& assets) {
  if (!options.timingJson.empty() && options.ledger) {
    std::fprintf(stderr,
                 "--timing-json is refused under --ledger: ledger mode "
                 "skips the benchmark phases, so there is no timing to "
                 "report\n");
    return 1;
  }
  FILE* timingJson = nullptr;
  if (!options.timingJson.empty()) {
    timingJson = std::fopen(options.timingJson.c_str(), "w");
    if (!timingJson) {
      std::fprintf(stderr, "cannot open --timing-json path %s\n",
                   options.timingJson.c_str());
      return 1;
    }
  }

  // THE DEVICE'S OWN CONTEXT, and never one of this sweep's making: a
  // set is rendered by the runtime the host installed on the device it
  // brought up, and a canvas is photographed on a surface allocated
  // here. Two devices would mean the second could not read what the
  // first painted, so both stand on the one the host installed.
  skia::GraphiteContext* graphite = nullptr;
  if (options.gpu) {
    graphite = deviceGraphite();
    if (!graphite) {
      // Named the way a device that could not be created is named, so a
      // caller telling "this machine has no device" from "this is a
      // defect" reads one answer for both: a device whose Graphite
      // adoption failed has no 2D device runtime either.
      std::fprintf(stderr,
                   "no device runtime (the device carries no Graphite "
                   "context)\n");
      if (timingJson) std::fclose(timingJson);
      return 1;
    }
    std::printf("backend: Graphite GPU (work ms = CPU + synced GPU)\n");
    // The profiler is blind here, and it must say so. Under Graphite,
    // per-node self time measures op-RECORDING, not GPU execution, which
    // is asynchronous — so a node that records a handful of cheap ops
    // and then costs the GPU a fortune reads as nearly free. Every
    // per-node cost, and every caching decision derived from one,
    // therefore describes the raster machine rather than this one.
    std::printf(
        "NOTE: per-node profile times are RECORDING time on GPU, "
        "not GPU execution — trust the work-ms column.\n");
    // Graphite does have a per-recording GPU-time API, and only some
    // backends implement it. The mask is printed rather than assumed so
    // that a Skia update enabling it here is visible immediately.
    const skgpu::GpuStatsFlags caps = graphite->context()->supportedGpuStats();
    std::printf(
        "GPU per-recording elapsed-time stats: %s "
        "(supportedGpuStats mask 0x%x)\n",
        (caps & skgpu::GpuStatsFlags::kElapsedTime)
            ? "SUPPORTED — a GPU-time lane is now wireable"
            : "unsupported on this backend",
        (unsigned)caps);
  }

  std::filesystem::create_directories(options.outDir);
  // TWO TIMING COLUMNS, and the difference between them is the point.
  // "frame ms" is end to end, the backend flush included — on the device
  // that is a synchronous drain per frame, so the column is the honest
  // serialized CPU+GPU cost of one frame. "headroom" is 1000 / mean(work
  // ms), the rate the frame's WORK alone would allow with the flush
  // taken out; it is a ceiling and never a frame rate. A headless sweep
  // presents nothing, so it has no frame rate to report at all — a
  // present interval is a property of a swap chain and a vsync, neither
  // of which exists here.
  std::printf("%-22s %10s %8s %8s %9s  %s\n", "sketch", "canvas", "frame ms",
              "p99 ms", "headroom", "lanes");

  const std::vector<int> chosen = selection(options.only, options.kind);
  const std::vector<Entry>& entries = registry();
  bool anyShortened = false;
  size_t skipped = 0;
  // How far the run got, for the crash reporter: a fault at plate 3 and a
  // fault at plate 130 are different problems, and only one of them can
  // be narrowed with --sketch on the next run.
  size_t plates = 0;
  for (int index : chosen) {
    const Entry& entry = entries[index];
    // A SKETCH THIS MACHINE CANNOT DRAW IS SKIPPED RATHER THAN FAILED,
    // and no plate is written for it. What it would draw instead — a
    // card naming what is missing — is not the picture its name stands
    // for, and a baseline that adopted one would hold a promise about
    // this machine's install rather than about the drawing code.
    std::string why;
    if (!entry.available(&why)) {
      std::printf("%-22s %10s  [skipped: %s]\n", entry.name, "-", why.c_str());
      ++skipped;
      continue;
    }
    const Kind kind = entry.kind();
    if (!kind) {
      std::fprintf(stderr, "sketch %s has no kind\n", entry.name);
      if (timingJson) std::fclose(timingJson);
      return 1;
    }
    // EVERY headless run writes a plate, and a plate is a picture that
    // will be diffed, so the session is opened with anything the sketch
    // measured about its own execution PINNED — a build time, a bake
    // cost, a live node count would otherwise differ from ITSELF between
    // two runs of one binary and the sweep would report a mover for a
    // change that moved nothing. It is not conditional on the ledger
    // flag because the two modes must photograph the same picture: the
    // benchmark phases decide how a machine spends its time, never what
    // the capture contains.
    // NAME THE ENTRY BEFORE ANYTHING RUNS IN IT. A sweep opens a hundred
    // sketches in one process, so a fault inside one is a fault inside
    // this process, and the only thing that says which sketch it was is
    // this — the last line on stderr is whatever the sketch BEFORE it
    // printed, and a run of a hundred is where that costs the most.
    noteSketch(entry.name);
    notePlates((int)plates);
    std::unique_ptr<Session> session;
    {
      PhaseMark mark(Phase::Setup);
      session = kind->open(fonts, assets, true);
    }
    if (options.noPromotion) session->setAutoPromotion(false);
    SkDebugf("=== sketch %s\n", entry.name);

    // Every size below comes off the session: a sketch declares its own
    // canvas from inside its own setup, which opening has just run.
    const SkSize size = session->canvas().size;
    const SkColor4f clearColor = session->canvas().background;
    const SkImageInfo info =
        SkImageInfo::MakeN32Premul((int)size.width(), (int)size.height());
    sk_sp<SkSurface> surface;
    std::function<void()> flushHook;
    if (graphite) {
      surface = SkSurfaces::RenderTarget(graphite->recorder(), info);
      // Serialize each frame to completion so a frame's cost cannot hide
      // in queue depth. Real hosts pipeline — this is the honest
      // worst-case bound the frame-time floor is judged on.
      flushHook = [&graphite] {
        if (auto recording = graphite->recorder()->snap()) {
          skgpu::graphite::InsertRecordingInfo insert;
          insert.fRecording = recording.get();
          graphite->context()->insertRecording(insert);
        }
        graphite->context()->submit(skgpu::graphite::SyncToCpu::kYes);
      };
    }
    if (!surface) surface = SkSurfaces::Raster(info);

    FrameStats stats;
    const auto stepOne = [&](SkSurface& target) {
      // One watch, read twice: the two lanes both start at the top of the
      // frame and differ only in whether the backend drain is inside.
      const measure::Stopwatch watch;
      target.getCanvas()->clear(clearColor);
      PhaseMark mark(Phase::Draw);
      session->frame(*target.getCanvas(), kStep);
      stats.addWork(watch.elapsedMs());
      if (flushHook) flushHook();
      stats.add(watch.elapsedMs());
    };

    // Warm past the entrance choreography so the table reports STEADY
    // STATE, which is the number a running host feels. Entrance
    // transitions are one-shots: their cost is real, but it belongs to a
    // different budget than the frame loop. On the device the warmup
    // also absorbs pipeline compilation.
    //
    // The frame counts are a TIME budget rather than a constant, because
    // costs across this registry span more than an order of magnitude
    // and a fixed count would make a sweep of the expensive ones take
    // minutes. A sketch that could not afford the full warmup is marked
    // in the table, because a run cut short is still inside its entrance
    // and its average means something different from the others'.
    const double warmBudgetMs = options.only >= 0 ? 1e9 : 4000;
    const double sampleBudgetMs = options.only >= 0 ? 1e9 : 2500;
    int warmFrames = 0;
    int sampleFrames = kMinSampleFrames;
    bool shortened = false;
    measure::FrameSample sample;
    std::vector<Lane> lanes;
    if (!options.ledger) {
      for (int f = 0; f < kProbeFrames; ++f) stepOne(*surface);
      const double probeMs = std::max(0.01, stats.average());
      warmFrames =
          std::max(0, std::min(kMaxWarmFrames, (int)(warmBudgetMs / probeMs)));
      sampleFrames =
          std::max(kMinSampleFrames,
                   std::min(kMaxSampleFrames, (int)(sampleBudgetMs / probeMs)));
      shortened = warmFrames < kMaxWarmFrames;
      anyShortened = anyShortened || shortened;
      for (int f = 0; f < warmFrames; ++f) stepOne(*surface);
      stats = {};
      std::vector<double> laneTotals;
      for (int f = 0; f < sampleFrames; ++f) {
        stepOne(*surface);
        const std::span<const Lane> frameLanes = session->lanes();
        laneTotals.resize(frameLanes.size(), 0.0);
        lanes.assign(frameLanes.begin(), frameLanes.end());
        for (size_t l = 0; l < frameLanes.size(); ++l)
          laneTotals[l] += frameLanes[l].ms;
      }
      for (size_t l = 0; l < lanes.size(); ++l)
        lanes[l].ms = laneTotals[l] / (double)sampleFrames;
      sample = stats.sample();
    }

    // ---- capture determinism ------------------------------------------
    // Everything above is a TIME budget, so both frame counts depend on
    // how fast this machine happened to be. That is fine for a timing
    // table and fatal for a plate: it would mean the captured frame is
    // whichever frame the budget reached, so the same binary on the same
    // sources would render a different image on every run — and an image
    // that cannot be reproduced cannot be reviewed or diffed.
    //
    // So the capture always lands at the SAME scene time regardless of
    // machine speed. A fast sketch tops up by zero frames; a slow one
    // pays the difference. ...unless the sketch NAMES its moment, which
    // cannot reuse the top-up for two independent reasons: the frames
    // already stepped are machine-dependent, and a declared time may be
    // EARLIER than them, which there is no way to rewind. So the session
    // is reopened and stepped from zero at the same fixed step, and the
    // capture frame is then a function of the DECLARATION alone.
    double declared = options.captureAt > 0 ? options.captureAt
                                            : session->canvas().captureSeconds;
    // A ledger run always takes the exact-stepped path; a sketch with no
    // declared moment gets the derived default, which is the identical
    // frame the benchmarked sweep captures.
    if (options.ledger && declared <= 0) declared = kCaptureFrame / kRate;
    if (declared > 0) {
      session = kind->open(fonts, assets, true);
      if (options.noPromotion) session->setAutoPromotion(false);
      if (session->canvas().size != size) {
        std::fprintf(stderr,
                     "sketch %s declared a different canvas on reopen\n",
                     entry.name);
        if (timingJson) std::fclose(timingJson);
        return 1;
      }
      const int captureFrame = (int)std::lround(declared * kRate);
      for (int f = 0; f < captureFrame; ++f) stepOne(*surface);
    } else {
      const int stepped = kProbeFrames + warmFrames + sampleFrames;
      for (int f = stepped; f < kCaptureFrame; ++f) stepOne(*surface);
    }

    char canvasLabel[24];
    std::snprintf(canvasLabel, sizeof canvasLabel, "%dx%d", (int)size.width(),
                  (int)size.height());
    char nameLabel[48];
    std::snprintf(nameLabel, sizeof nameLabel, "%s%s", entry.name,
                  shortened ? " *" : "");
    if (options.ledger) {
      std::printf("%-22s %10s  [ledger]\n", nameLabel, canvasLabel);
    } else {
      std::printf("%-22s %10s %8.2f %8.2f %9.0f ", nameLabel, canvasLabel,
                  sample.frameMs, sample.p99Ms, sample.headroomFps);
      for (const Lane& lane : lanes)
        std::printf(" %s %.2f", lane.name, lane.ms);
      std::printf("\n");
      if (timingJson) {
        // Both numbers a gate judges, plus the one it derives from.
        std::fprintf(timingJson,
                     "{\"scene\":\"%s\",\"canvas\":\"%dx%d\","
                     "\"frame_ms\":%.3f,\"work_ms\":%.3f,"
                     "\"p99_ms\":%.3f,\"headroom_fps\":%.1f,\"shortened\":%s,"
                     "\"backend\":\"%s\"}\n",
                     entry.name, (int)size.width(), (int)size.height(),
                     sample.frameMs, sample.workMs, sample.p99Ms,
                     sample.headroomFps, shortened ? "true" : "false",
                     options.gpu ? "gpu" : "raster");
        // Flush per line: a sketch that crashes later must not take the
        // lines already written down with it.
        std::fflush(timingJson);
      }
    }

    // WHAT THE SKETCH ASKED FOR, WHOLE, or what the width allows. A
    // declared oversample is the sketch's own grid — one pixel of what
    // it reconstructs covering the same count of device pixels
    // everywhere — and the fraction a width ceiling produces is exactly
    // what that cannot survive, so a declaration outranks the ceiling
    // and every tier honours it alike: two plates of one sketch are
    // comparable only if they were photographed on the same grid.
    const int declaredOversample = session->canvas().oversample;
    const float scale =
        declaredOversample > 0
            ? (float)declaredOversample
            : std::max(1.0f, std::min(session->oversample(),
                                      kPlateWidthCeiling / size.width()));
    const SkImageInfo plateInfo = SkImageInfo::MakeN32Premul(
        (int)(size.width() * scale), (int)(size.height() * scale));
    const std::string path =
        options.outDir + "/" + std::string(kPlatePrefix) + entry.name + ".png";
    SkBitmap bitmap;
    bitmap.allocPixels(plateInfo);

    if (graphite) {
      // A Graphite surface cannot readPixels synchronously, so the still
      // comes back through the async path — and these are the pixels the
      // interactive host actually shows, so visual review runs here.
      sk_sp<SkSurface> plate =
          SkSurfaces::RenderTarget(graphite->recorder(), plateInfo);
      if (!plate) {
        std::fprintf(stderr, "could not allocate a device plate for %s\n",
                     entry.name);
        if (timingJson) std::fclose(timingJson);
        return 1;
      }
      plate->getCanvas()->clear(clearColor);
      plate->getCanvas()->scale(scale, scale);
      session->still(*plate->getCanvas());
      if (auto recording = graphite->recorder()->snap()) {
        skgpu::graphite::InsertRecordingInfo insert;
        insert.fRecording = recording.get();
        graphite->context()->insertRecording(insert);
      }
      struct ReadContext {
        std::unique_ptr<const SkImage::AsyncReadResult> result;
        bool called = false;
      } read;
      graphite->context()->asyncRescaleAndReadPixels(
          plate.get(), plateInfo,
          SkIRect::MakeWH(plateInfo.width(), plateInfo.height()),
          SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
          [](SkImage::ReadPixelsContext context,
             std::unique_ptr<const SkImage::AsyncReadResult> result) {
            auto* r = static_cast<ReadContext*>(context);
            r->result = std::move(result);
            r->called = true;
          },
          &read);
      skgpu::graphite::SubmitInfo submitInfo;
      submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
      graphite->context()->submit(submitInfo);
      for (int spin = 0; spin < 5000 && !read.called; ++spin)
        graphite->context()->checkAsyncWorkCompletion();
      if (!read.result) continue;
      const auto* src = static_cast<const uint8_t*>(read.result->data(0));
      const size_t srcRowBytes = read.result->rowBytes(0);
      for (int y = 0; y < plateInfo.height(); ++y)
        std::memcpy(bitmap.pixmap().writable_addr(0, y),
                    src + (size_t)y * srcRowBytes,
                    std::min(srcRowBytes, bitmap.rowBytes()));
      writePlate(bitmap.pixmap(), path);
      ++plates;
      continue;
    }
    sk_sp<SkSurface> plate = SkSurfaces::Raster(plateInfo);
    plate->getCanvas()->clear(clearColor);
    plate->getCanvas()->scale(scale, scale);
    session->still(*plate->getCanvas());
    plate->readPixels(bitmap.pixmap(), 0, 0);
    if (!writePlate(bitmap.pixmap(), path)) {
      if (timingJson) std::fclose(timingJson);
      return 1;
    }
    ++plates;
  }

  if (anyShortened)
    std::printf(
        "\n* short run: too expensive for the full %d-frame warmup, so "
        "the average still\n  carries some of the entrance. Run it alone "
        "with --sketch for the settled number.\n",
        kProbeFrames + kMaxWarmFrames);
  if (!options.gpu) {
    const size_t written = chosen.size() - skipped;
    std::printf("wrote %zu plate%s to %s\n", written, written == 1 ? "" : "s",
                options.outDir.c_str());
  }
  if (timingJson) std::fclose(timingJson);
  return 0;
}

}  // namespace sigil::sketch
