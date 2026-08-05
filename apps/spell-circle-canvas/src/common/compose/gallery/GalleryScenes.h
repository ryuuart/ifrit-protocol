#pragma once
// ComposeGallery scene registry and headless runner. Individual scenes live
// in the category headers included below; both the Qt Quick app
// (ComposeGalleryView + qml/Main.qml) and --headless build on this file.
//
// The registry is two halves sharing one index space: the CATALOG scenes
// declared here, each written to exercise a particular part of the library,
// and the STUDIES (GalleryStudies.h), which are the standalone sketch files
// compiled into the binary. The halves are not interchangeable — a study
// brings its own canvas size and background colour — so everything
// downstream of makeScene() reads both off the stage rather than assuming a
// constant.

#include "GalleryCore.h"
#include "GalleryStudies.h"
#include "ScenesChrome.h"
#include "ScenesData.h"
#include "ScenesFlourish.h"
#include "ScenesGerstner.h"
#include "ScenesInventory.h"
#include "ScenesGame.h"
#include "ScenesVeloren.h"
#include "ScenesOrganic.h"
#include "ScenesOrnament.h"
#include "ScenesAero.h"
#include "ScenesConsole.h"
#include "ScenesCosmati.h"
#include "ScenesKinetic.h"
#include "ScenesNetwork.h"
#include "ScenesPersona.h"
#include "ScenesSkillTree.h"
#include "ScenesY2k.h"
#include "ScenesScale.h"
#include "ScenesPoster.h"
#include "ScenesZellige.h"
#include "ScenesBeethoven.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

#ifdef SIGILCOMPOSE_GALLERY_HEADLESS_GPU
#include "GalleryGpu.h"
#include "SkiaGraphiteContext.h"
#include <include/gpu/GpuTypes.h> // skgpu::GpuStatsFlags
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#endif

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string_view>

namespace compose_gallery {

struct SceneInfo {
  const char *name;
  const char *category;
  const char *catalog; // one line on what this scene exercises, shown in the UI
};

// `category` is a folder path, not a flat label: the sidebar groups on it, so
// a registry this size stays navigable. Catalog entries split by what a scene
// exercises and mirror the "Study \xc2\xb7" prefix the sketch files use, so the
// two halves of the registry read as siblings rather than two systems.
inline constexpr SceneInfo kScenes[] = {
    {"world hud", "Catalog \xc2\xb7 Game UI",
     "voxygen dimensions \xe2\x80\x94 bars, hotbar, minimap"},
    {"manuscript", "Catalog \xc2\xb7 Type & grid", "ornament"},
    {"nine slice", "Catalog \xc2\xb7 Scale", "#9 texture-gen"},
    {"botanical", "Catalog \xc2\xb7 Generative", "generative"},
    {"ui particles", "Catalog \xc2\xb7 Scale", "SoA scale \xc2\xb7 instances()"},
    {"load", "Catalog \xc2\xb7 Scale", "#21 sustained load"},
    {"tile map", "Catalog \xc2\xb7 Tiling", "#15"},
    {"organic", "Catalog \xc2\xb7 Generative", "#5 #9 #10 #12 shapes/layouts"},
    {"flourish", "Catalog \xc2\xb7 Generative", "the whole surface, at once"},
    {"kinetic card", "Catalog \xc2\xb7 Type & grid",
     "kinetic type grammar"},
    {"night network", "Catalog \xc2\xb7 Generative",
     "the brush engine, twelve constructions"},
    {"persona menu", "Catalog \xc2\xb7 Game UI",
     "P3R menu grammar"},
    {"aero desktop", "Catalog \xc2\xb7 Chrome",
     "glass + window colorization"},
    {"y2k chrome", "Catalog \xc2\xb7 Chrome", "chrome presets A/B"},
    {"passive tree", "Catalog \xc2\xb7 Game UI",
     "linework + orbit router"},
    {"daemon console", "Catalog \xc2\xb7 Game UI", "console() LineRing feed"},
    {"motion poster", "Catalog \xc2\xb7 Type & grid",
     "EMBER GATE \xe2\x80\x94 the flagship living poster"},
    {"zellige", "Catalog \xc2\xb7 Tiling",
     "girih Hankin PIC \xe2\x80\x94 regenerating"},
    {"beethoven", "Catalog \xc2\xb7 Type & grid",
     "Brockmann arc table, span reveal"},
    {"loot grid", "Catalog \xc2\xb7 Game UI",
     "D2 hoard \xe2\x80\x94 generated materials, instances()"},
    {"gerstner grid", "Catalog \xc2\xb7 Type & grid",
     "Capital 1962 \xe2\x80\x94 the mobile grid, run"},
    {"cosmati", "Catalog \xc2\xb7 Tiling",
     "opus sectile \xe2\x80\x94 quincunx, guilloche, quarried stone"},
};
inline constexpr int kCatalogSceneCount =
    (int)(sizeof(kScenes) / sizeof(kScenes[0]));
inline constexpr int kGallerySceneCount = kCatalogSceneCount + kStudyCount;

/** The registry entry at `index`: catalog first, then studies. Returned by
 *  value because a study's row is assembled from StudyInfo, but every
 *  member still points at a string literal. */
inline SceneInfo sceneInfo(int index) {
  if (index < 0 || index >= kGallerySceneCount)
    return {"", "", ""};
  if (index < kCatalogSceneCount)
    return kScenes[index];
  const StudyInfo &study = kStudies[index - kCatalogSceneCount];
  return {study.name, study.category, study.tag};
}

/** Scene index for a name or a decimal index; -1 when nothing matches.
 *  Names match case-insensitively on any unique substring, so `--scene y2k`
 *  and `--scene "y2k chrome"` both land on the same entry. A study also
 *  answers to its file stem, because `slitscan_2001` is what you have in
 *  front of you when you want to look at it. */
inline int findScene(std::string_view query) {
  if (query.empty())
    return -1;
  if (query.find_first_not_of("0123456789") == std::string_view::npos) {
    const int index = std::stoi(std::string(query));
    return index >= 0 && index < kGallerySceneCount ? index : -1;
  }
  auto lower = [](std::string_view s) {
    std::string out(s);
    for (char &c : out)
      c = (char)std::tolower((unsigned char)c);
    return out;
  };
  const std::string needle = lower(query);
  // Exact wins over substring, and both win over a later entry: two passes
  // rather than one so `--scene hello` cannot be captured by a study whose
  // tag happens to start the same way.
  for (int i = 0; i < kGallerySceneCount; ++i) {
    if (lower(sceneInfo(i).name) == needle)
      return i;
    if (i >= kCatalogSceneCount &&
        lower(kStudies[i - kCatalogSceneCount].key) == needle)
      return i;
  }
  for (int i = 0; i < kGallerySceneCount; ++i) {
    if (lower(sceneInfo(i).name).find(needle) != std::string::npos)
      return i;
    if (i >= kCatalogSceneCount &&
        lower(kStudies[i - kCatalogSceneCount].key).find(needle) !=
            std::string::npos)
      return i;
  }
  return -1;
}

/** The REGISTRY spelling for the entry at `index`: for a study, its file stem
 *  (`chaucer_astrolabe` — what SIGIL_SKETCH_STATIC registered and what
 *  `--scene` selects by); for a catalog scene, its registry name.
 *
 *  Captures are written under THIS name rather than Scene::name()'s display
 *  spelling, so a script that selects a scene with `--scene $s` can look for
 *  `gallery_$s.png` without keeping a second copy of the mapping. Keep the
 *  two uses in step: anything that names a scene on the command line and
 *  anything that names a scene on disk must agree here. */
inline const char *registryName(int index) {
  if (index >= kCatalogSceneCount && index < kGallerySceneCount)
    return kStudies[index - kCatalogSceneCount].key;
  return sceneInfo(index).name;
}

inline std::unique_ptr<Scene> makeCatalogScene(int index) {
  switch (index) {
  case 0: return std::make_unique<WorldHudScene>();
  case 1: return std::make_unique<ManuscriptScene>();
  case 2: return std::make_unique<NineSliceScene>();
  case 3: return std::make_unique<BotanicalScene>();
  case 4: return std::make_unique<UiParticleScene>();
  case 5: return std::make_unique<LoadScene>();
  case 6: return std::make_unique<TileScene>();
  case 7: return std::make_unique<OrganicScene>();
  case 8: return std::make_unique<FlourishScene>();
  case 9: return std::make_unique<KineticCardScene>();
  case 10: return std::make_unique<NightNetworkScene>();
  case 11: return std::make_unique<PersonaMenuScene>();
  case 12: return std::make_unique<AeroDesktopScene>();
  case 13: return std::make_unique<Y2kChromeScene>();
  case 14: return std::make_unique<SkillTreeScene>();
  case 15: return std::make_unique<DaemonConsoleScene>();
  case 16: return std::make_unique<MotionPosterScene>();
  case 17: return std::make_unique<ZelligeScene>();
  case 18: return std::make_unique<BeethovenScene>();
  case 19: return std::make_unique<LootGridScene>();
  case 20: return std::make_unique<GerstnerGridScene>();
  case 21: return std::make_unique<CosmatiScene>();
  default: return nullptr;
  }
}

inline std::unique_ptr<Scene> makeScene(int index) {
  if (index < 0 || index >= kGallerySceneCount)
    return nullptr;
  if (index < kCatalogSceneCount)
    return makeCatalogScene(index);
  return makeStudy(kStudies[index - kCatalogSceneCount]);
}

/** Sweeps the registry (or one scene when `only` is a valid index),
 *  printing the FPS table and writing a 2x PNG per scene.
 *
 *  @p timingJsonPath — when non-empty, ALSO writes one JSON line per scene
 *  with the steady-state sample numbers (`{"scene":…,"work_ms":…,
 *  "p99_ms":…,"fps":…}`), the machine-readable lane the GPU 60 FPS gate
 *  (`scripts/plate_ledger.py --fps-gate`) parses. The table's stdout is
 *  untouched. The JSON snapshot is taken at the END OF THE SAMPLE WINDOW,
 *  before the capture pass — a scene that declares its capture moment (or
 *  a `--capture-at` run) rebuilds the stage for the capture, which resets
 *  FrameStats and refills it with from-zero stepping frames, so the
 *  numbers left in `stage.stats` by table-print time can be the CAPTURE
 *  pass's, not the sample's. The gate must report the steady frame, so it
 *  reads the snapshot. Refused under --ledger: the ledger runs no
 *  benchmark phases, and a timing file of zeros would be a lie. */
inline int runHeadless(const std::string &outDir, bool gpu = false,
                       int only = -1, bool noPromotion = false,
                       double captureAtOverride = -1.0, bool ledger = false,
                       const std::string &timingJsonPath = std::string()) {
  if (!timingJsonPath.empty() && ledger) {
    std::fprintf(stderr, "--timing-json is refused under --ledger: ledger "
                         "mode skips the benchmark phases, so there is no "
                         "timing to report\n");
    return 1;
  }
  FILE *timingJson = nullptr;
  if (!timingJsonPath.empty()) {
    timingJson = std::fopen(timingJsonPath.c_str(), "w");
    if (!timingJson) {
      std::fprintf(stderr, "cannot open --timing-json path %s\n",
                   timingJsonPath.c_str());
      return 1;
    }
  }
#ifdef SIGILCOMPOSE_GALLERY_HEADLESS_GPU
  std::unique_ptr<SkiaGraphiteContext> graphite;
  if (gpu) {
    graphite = makeHeadlessGraphite();
    if (!graphite) {
      std::fprintf(stderr, "Graphite context creation failed; no GPU sweep\n");
      return 1;
    }
    std::printf("backend: Graphite GPU (work ms = CPU + synced GPU)\n");
    // The profiler is blind here, and it must say so. Under Graphite,
    // Composer::profile()'s selfMs measures op-RECORDING time, not GPU
    // execution, which is asynchronous — so a node that records a handful of
    // cheap ops and then costs the GPU a fortune reads as nearly free. Every
    // per-node cost, and every caching decision derived from one (the
    // promotion threshold, the stability average, the temporal gate),
    // therefore describes the raster machine rather than this one. Read the
    // work-ms column for real cost, and treat per-node selfMs on GPU as
    // recording weight only.
    std::printf("NOTE: per-node profile times are RECORDING time on GPU, "
                "not GPU execution — trust the work-ms column.\n");
    // Graphite does have a per-RECORDING GPU-time API: request
    // GpuStatsFlags::kElapsedTime through InsertRecordingInfo's
    // fFinishedWithStatsProc and the finished callback reports
    // GpuStats::elapsedTime. Only the Vulkan (VkQueryPool timestamps) and
    // Dawn backends implement it; MtlCaps never sets fSupportedGpuStats, so
    // on a Metal context the mask below reads 0 and the QueueManager strips
    // any request for it with a warning. Even where it is supported the
    // bracket is a whole command buffer, and Graphite batches many compose
    // nodes into shared DrawPasses, so per-NODE GPU cost stays out of reach
    // without changes inside Skia. The mask is printed rather than assumed so
    // that a Skia update enabling it on this backend is visible immediately.
    const skgpu::GpuStatsFlags gpuStatCaps =
        graphite->context()->supportedGpuStats();
    const bool hasElapsed =
        gpuStatCaps & skgpu::GpuStatsFlags::kElapsedTime;
    std::printf("GPU per-recording elapsed-time stats: %s "
                "(supportedGpuStats mask 0x%x)\n",
                hasElapsed ? "SUPPORTED — a GPU-time lane is now wireable"
                           : "unsupported on this backend (Vulkan/Dawn "
                             "only at m151)",
                (unsigned)gpuStatCaps);
  }
#else
  if (gpu) {
    std::fprintf(stderr, "this build has no headless GPU backend\n");
    return 1;
  }
#endif
  std::filesystem::create_directories(outDir);
  std::printf("%-20s %10s %8s %8s %9s %6s %6s %6s %6s\n", "scene", "canvas",
              "work ms", "p99 ms", "fps", "recon", "layout", "volat", "paint");
  const int first = only >= 0 ? only : 0;
  const int last = only >= 0 ? only + 1 : kGallerySceneCount;
  bool anyShortened = false;
  for (int i = first; i < last; ++i) {
    GalleryStage stage;
    std::unique_ptr<Scene> next = makeScene(i);
    if (!next) {
      std::fprintf(stderr, "scene %d has no factory\n", i);
      return 1;
    }
    stage.activate(std::move(next));
    // --no-promotion: force automatic texture promotion off explicitly, so
    // the ledger A/B (promotion on vs off, either backend) runs on a real
    // binary. On GPU the backend-aware default already turns it off; this is
    // for a reproducible, attributable comparison rather than a behaviour it
    // adds. Set AFTER activate() (which rebuilds the composer).
    if (noPromotion)
      stage.composer->setAutoTexturePromotion(false);
    SkDebugf("=== scene %s\n", stage.scene->name());
    // Every size below comes off the stage, not off kSceneSize: a study
    // declares its own canvas from inside setup(), which activate() has
    // just run.
    const SkSize sceneSize = stage.sceneSize;
    const SkColor4f clearColor = stage.sceneBackground;
    const SkImageInfo info = SkImageInfo::MakeN32Premul(
        (int)sceneSize.width(), (int)sceneSize.height());
    sk_sp<SkSurface> surface;
#ifdef SIGILCOMPOSE_GALLERY_HEADLESS_GPU
    if (gpu) {
      surface = SkSurfaces::RenderTarget(graphite->recorder(), info);
      // Serialize each frame to completion so a frame's cost cannot hide in
      // queue depth: snap the recording, submit, wait. Real hosts pipeline —
      // this is the honest worst-case bound the 60fps floor is judged on.
      stage.flushHook = [&graphite] {
        if (auto recording = graphite->recorder()->snap()) {
          skgpu::graphite::InsertRecordingInfo insert;
          insert.fRecording = recording.get();
          graphite->context()->insertRecording(insert);
        }
        graphite->context()->submit(skgpu::graphite::SyncToCpu::kYes);
      };
    }
#endif
    if (!surface)
      surface = SkSurfaces::Raster(info);
    // Warm past the entrance choreography so the table reports STEADY STATE,
    // which is the number a running gallery feels. Entrance transitions are
    // one-shots: their cost is real, but it belongs to a different budget
    // than the frame loop. On GPU the warmup also absorbs pipeline
    // compilation.
    //
    // The frame counts are a TIME budget rather than a constant, because
    // scene costs across this registry span more than an order of magnitude
    // and a fixed count would make a sweep of the expensive ones take
    // minutes. So: probe briefly, then spend at most a few seconds per scene.
    // A scene that could not afford the full warmup is marked in the table,
    // because a run cut short is still inside its entrance and its average
    // means something different from the others'. Asking for ONE scene means
    // you want that scene's real number, so the budget applies only to a
    // sweep.
    const double warmBudgetMs = only >= 0 ? 1e9 : 4000;
    const double sampleBudgetMs = only >= 0 ? 1e9 : 2500;
    // The caps are named because the capture frame is DERIVED from them
    // below; if either moves, the capture moves with it rather than
    // silently drifting.
    constexpr int kProbeFrames = 8, kMinSampleFrames = 24;
    constexpr int kMaxWarmFrames = 240 - kProbeFrames;
    constexpr int kMaxSampleFrames = 120;
    // --ledger skips the ENTIRE benchmark (probe, warm, sample) and goes
    // straight to the exact-stepped capture below: a byte-identity sweep
    // wants the image, not the timing table, and the benchmark phases are
    // most of a sweep's wall clock. The image it produces is bit-identical to
    // the full sweep's, because the exact-stepped capture is a function of
    // the declared time alone — stepped from zero at a fixed dt, never
    // touched by how fast this machine ran.
    int warmFrames = 0, sampleFrames = kMinSampleFrames;
    bool shortened = false;
    double reconcileMs = 0, layoutMs = 0, volatileMs = 0, paintMs = 0;
    // The steady-state sample numbers, snapshotted the moment the sample
    // window closes — see the timingJsonPath doc above for why the JSON
    // line cannot read stage.stats at table-print time.
    double sampleWorkMs = 0, sampleP99Ms = 0, sampleFps = 0;
    if (!ledger) {
      for (int f = 0; f < kProbeFrames; ++f) {
        surface->getCanvas()->clear(clearColor);
        stage.frame(*surface->getCanvas(), 1.0 / 60.0);
      }
      const double probeMs = std::max(0.01, stage.stats.average());
      warmFrames = std::max(
          0, std::min(kMaxWarmFrames, (int)(warmBudgetMs / probeMs)));
      sampleFrames =
          std::max(kMinSampleFrames,
                   std::min(kMaxSampleFrames, (int)(sampleBudgetMs / probeMs)));
      shortened = warmFrames < kMaxWarmFrames;
      anyShortened = anyShortened || shortened;
      for (int f = 0; f < warmFrames; ++f) {
        surface->getCanvas()->clear(clearColor);
        stage.frame(*surface->getCanvas(), 1.0 / 60.0);
      }
      stage.stats = {};
      for (int f = 0; f < sampleFrames; ++f) {
        surface->getCanvas()->clear(clearColor);
        stage.frame(*surface->getCanvas(), 1.0 / 60.0);
        const Composer::Stats &cs = stage.composer->stats();
        reconcileMs += cs.reconcileMs;
        layoutMs += cs.layoutMs;
        volatileMs += cs.volatileMs;
        paintMs += cs.paintMs;
      }
      sampleWorkMs = stage.stats.average();
      sampleP99Ms = stage.stats.percentile(0.99);
      sampleFps = stage.stats.fps();
    }
    // ---- capture determinism -------------------------------------------
    // Everything above is a TIME budget, so `warmFrames` and `sampleFrames`
    // both depend on how fast this machine happened to be. That is fine for a
    // timing table and fatal for a PNG: it would mean the captured frame is
    // whichever frame the budget happened to reach, so the same binary on the
    // same sources would render a different image on every run — and an image
    // that cannot be reproduced cannot be reviewed or diffed.
    //
    // So the capture always lands at the SAME scene time regardless of
    // machine speed. kCaptureFrame is the most the budgeted path could
    // possibly have reached (probe + warm + sample), DERIVED from those caps
    // rather than restated, so raising either cap moves the capture with it
    // instead of leaving the top-up below silently short. A fast scene tops
    // up by zero frames; a slow one pays the difference. The timing table
    // above is unaffected: it keeps its budget and its `*` mark.
    constexpr int kCaptureFrame =
        kProbeFrames + kMaxWarmFrames + kMaxSampleFrames;
    // ...unless the scene NAMES its moment (Scene::captureSeconds). Reaching
    // a declared time cannot reuse the top-up above, for two independent
    // reasons: `stepped` is machine-dependent, and a declared time may be
    // EARLIER than the frames already spent benchmarking, which there is no
    // way to rewind. So the scene is rebuilt and stepped from zero at the
    // same fixed step. The capture frame is then a function of the
    // DECLARATION alone — the same property the derived frame has, held the
    // same way: machine speed never reaches the image.
    //
    // The rebuild reuses the surface because it comes from the same factory
    // and therefore declares the same canvas; the sizes are asserted rather
    // than assumed, since a scene that resized on rebuild would otherwise
    // draw into a surface of the wrong shape and merely look odd.
    double declared = captureAtOverride > 0 ? captureAtOverride
                                            : stage.scene->captureSeconds();
    // Ledger mode always takes the exact-stepped path; a scene with no
    // declared moment gets the derived default, which is the identical
    // frame the classic sweep captures (kCaptureFrame at 1/60).
    if (ledger && declared <= 0)
      declared = kCaptureFrame / 60.0;
    if (declared > 0) {
      stage.activate(makeScene(i));
      if (noPromotion)
        stage.composer->setAutoTexturePromotion(false);
      if (stage.sceneSize != sceneSize) {
        std::fprintf(stderr,
                     "scene %s declared a different canvas on rebuild\n",
                     stage.scene->name());
        return 1;
      }
      const int captureFrame = (int)(declared * 60.0 + 0.5);
      for (int f = 0; f < captureFrame; ++f) {
        surface->getCanvas()->clear(clearColor);
        stage.frame(*surface->getCanvas(), 1.0 / 60.0);
      }
    } else {
      const int stepped = kProbeFrames + warmFrames + sampleFrames;
      for (int f = stepped; f < kCaptureFrame; ++f) {
        surface->getCanvas()->clear(clearColor);
        stage.frame(*surface->getCanvas(), 1.0 / 60.0);
      }
    }

    char canvasLabel[24];
    std::snprintf(canvasLabel, sizeof(canvasLabel), "%dx%d",
                  (int)sceneSize.width(), (int)sceneSize.height());
    char nameLabel[40];
    std::snprintf(nameLabel, sizeof(nameLabel), "%s%s", stage.scene->name(),
                  shortened ? " *" : "");
    if (ledger) {
      std::printf("%-20s %10s  [ledger]\n", nameLabel, canvasLabel);
    } else {
      const double n = (double)sampleFrames;
      std::printf(
          "%-20s %10s %8.2f %8.2f %9.0f %6.2f %6.2f %6.2f %6.2f   rec %zu "
          "painted %zu\n",
          nameLabel, canvasLabel, stage.stats.average(),
          stage.stats.percentile(0.99), stage.stats.fps(), reconcileMs / n,
          layoutMs / n, volatileMs / n, paintMs / n,
          stage.composer->stats().picturesRecorded,
          stage.composer->stats().nodesPainted);
      if (timingJson) {
        std::fprintf(
            timingJson,
            "{\"scene\":\"%s\",\"canvas\":\"%dx%d\",\"work_ms\":%.3f,"
            "\"p99_ms\":%.3f,\"fps\":%.1f,\"shortened\":%s,"
            "\"backend\":\"%s\"}\n",
            registryName(i), (int)sceneSize.width(), (int)sceneSize.height(),
            sampleWorkMs, sampleP99Ms, sampleFps,
            shortened ? "true" : "false", gpu ? "gpu" : "raster");
        // Flush per line: a scene that crashes later must not take the lines
        // already written down with it.
        std::fflush(timingJson);
      }
    }
    // Capture the PNG at up to 2x. The timings above ran at 1x, but the saved
    // frame is re-rendered through a scaled canvas so review images are sharp
    // (Cache::Texture re-bakes at the capture scale rather than upsampling).
    // Scenes in this registry differ widely in canvas width, so it is the
    // FACTOR that gives way, not the pixel ceiling: doubling an already-wide
    // canvas produces a PNG too large to actually look at.
    const float captureScale =
        std::max(1.0f, std::min(2.0f, 2400.0f / sceneSize.width()));
#ifdef SIGILCOMPOSE_GALLERY_HEADLESS_GPU
    if (gpu) {
      // Turn the stats overlay off before capturing, the same rule as the
      // raster path below. It has to be set on both paths because this branch
      // has its own frame() and its own encode and `continue`s before
      // reaching the other one. Leaving it on bakes live wall-clock digits
      // into the captured pixels, which makes every capture differ from every
      // other inside the overlay band.
      stage.showStats = false;
      // GPU captures read back through the async path (a Graphite surface
      // cannot readPixels synchronously) — these are what the interactive
      // QQuickRhiItem gallery actually shows, so visual QA runs HERE, not
      // on the raster sweep.
      const SkImageInfo shotInfo = SkImageInfo::MakeN32Premul(
          (int)(sceneSize.width() * captureScale),
          (int)(sceneSize.height() * captureScale));
      sk_sp<SkSurface> shot =
          SkSurfaces::RenderTarget(graphite->recorder(), shotInfo);
      if (shot) {
        shot->getCanvas()->clear(clearColor);
        shot->getCanvas()->scale(captureScale, captureScale);
        stage.frame(*shot->getCanvas(), 1.0 / 60.0);
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
            shot.get(), shotInfo, SkIRect::MakeWH(shotInfo.width(),
                                                  shotInfo.height()),
            SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest,
            [](SkImage::ReadPixelsContext context,
               std::unique_ptr<const SkImage::AsyncReadResult> result) {
              auto *r = static_cast<ReadContext *>(context);
              r->result = std::move(result);
              r->called = true;
            },
            &read);
        skgpu::graphite::SubmitInfo submitInfo;
        submitInfo.fSync = skgpu::graphite::SyncToCpu::kYes;
        graphite->context()->submit(submitInfo);
        for (int spin = 0; spin < 5000 && !read.called; ++spin)
          graphite->context()->checkAsyncWorkCompletion();
        if (read.result) {
          SkBitmap bm;
          bm.allocPixels(shotInfo);
          const auto *src = static_cast<const uint8_t *>(read.result->data(0));
          const size_t srcRB = read.result->rowBytes(0);
          for (int y = 0; y < shotInfo.height(); ++y)
            std::memcpy(bm.pixmap().writable_addr(0, y),
                        src + (size_t)y * srcRB,
                        std::min(srcRB, bm.rowBytes()));
          const std::string path =
              outDir + "/gallery_" + registryName(i) + ".png";
          SkFILEWStream stream(path.c_str());
          if (stream.isValid())
            SkPngEncoder::Encode(&stream, bm.pixmap(), {});
        }
      }
      continue;
    }
#endif
    // Clean captures: the FPS overlay bakes live wall-clock digits into the
    // pixels, which would make every capture differ from every other. With it
    // off, scene content is fully deterministic (fixed dt, seeded random
    // sources), so captures diff meaningfully across builds. The GPU branch
    // above must set this for itself — it returns before reaching here.
    stage.showStats = false;
    sk_sp<SkSurface> shot = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)(sceneSize.width() * captureScale),
        (int)(sceneSize.height() * captureScale)));
    shot->getCanvas()->clear(clearColor);
    shot->getCanvas()->scale(captureScale, captureScale);
    stage.frame(*shot->getCanvas(), 1.0 / 60.0);
    SkBitmap bm;
    bm.allocPixels(shot->imageInfo());
    shot->readPixels(bm.pixmap(), 0, 0);
    const std::string path =
        outDir + "/gallery_" + registryName(i) + ".png";
    SkFILEWStream stream(path.c_str());
    if (!stream.isValid() || !SkPngEncoder::Encode(&stream, bm.pixmap(), {}))
      return 1;
  }
  if (anyShortened)
    std::printf("\n* short run: too expensive for the full 240-frame warmup, "
                "so the average still\n  carries some of the entrance. Run it "
                "alone with --scene for the settled number.\n");
  if (!gpu)
    std::printf("wrote %d gallery scene%s to %s\n", last - first,
                last - first == 1 ? "" : "s", outDir.c_str());
  if (timingJson)
    std::fclose(timingJson);
  return 0;
}

} // namespace compose_gallery
