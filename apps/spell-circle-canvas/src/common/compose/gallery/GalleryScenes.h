#pragma once
// ComposeGallery scene registry + headless FPS runner. Scenes live in
// category headers; the Qt Quick app (ComposeGalleryView + qml/Main.qml)
// and --headless both build on this. Each scene names the
// STRESS_TESTS.md catalog items it exercises.
//
// The registry is two halves behind one index space: the CATALOG scenes
// below, authored against the library's own stress list, and the STUDIES
// (GalleryStudies.h), which are the sketch files compiled in. They differ
// in more than provenance — a study brings its own canvas size and
// background — so everything downstream of makeScene() reads those off the
// stage rather than off a constant.

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
  const char *catalog; // STRESS_TESTS.md items exercised
};

// Folders, not one heap. Eighteen scenes under "Showcase" was already the
// limit of what a flat list can say, and the studies more than doubled the
// registry — so the catalog splits by what a scene actually exercises, and
// mirrors the "Study \xc2\xb7" prefix the sketches use so the two halves read
// as siblings.
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
     "\xc2\xa7""8 kinetic grammar (study port)"},
    {"night network", "Catalog \xc2\xb7 Generative",
     "the brush engine, twelve constructions"},
    {"persona menu", "Catalog \xc2\xb7 Game UI",
     "\xc2\xa7""1 verified P3R grammar"},
    {"aero desktop", "Catalog \xc2\xb7 Chrome",
     "\xc2\xa7""6 glass + colorization"},
    {"y2k chrome", "Catalog \xc2\xb7 Chrome", "\xc2\xa7""2/\xc2\xa7""3 presets A/B"},
    {"passive tree", "Catalog \xc2\xb7 Game UI",
     "\xc2\xa7""5 linework + orbit router"},
    {"daemon console", "Catalog \xc2\xb7 Game UI", "console() LineRing feed"},
    {"motion poster", "Catalog \xc2\xb7 Type & grid",
     "EMBER GATE \xe2\x80\x94 the flagship living poster"},
    {"zellige", "Catalog \xc2\xb7 Tiling",
     "girih Hankin PIC \xe2\x80\x94 regenerating"},
    {"beethoven", "Catalog \xc2\xb7 Type & grid",
     "\xc2\xa7""7 Brockmann arc table, trim reveal"},
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
 *  printing the FPS table and writing a 2x PNG per scene. */
inline int runHeadless(const std::string &outDir, bool gpu = false,
                       int only = -1, bool noPromotion = false) {
#ifdef SIGILCOMPOSE_GALLERY_HEADLESS_GPU
  std::unique_ptr<SkiaGraphiteContext> graphite;
  if (gpu) {
    graphite = makeHeadlessGraphite();
    if (!graphite) {
      std::fprintf(stderr, "Graphite context creation failed; no GPU sweep\n");
      return 1;
    }
    std::printf("backend: Graphite GPU (work ms = CPU + synced GPU)\n");
    // The profiler is blind here, and it must say so: Composer::profile()'s
    // selfMs measures op-RECORDING time under Graphite, not GPU execution
    // (which is async). So every per-node cost, and every caching decision
    // built on one — the promotion threshold, the stability EMA, the
    // temporal gate — describes the raster machine, not this one. A node can
    // read 0.1 ms here and cost 20 ms of GPU. Read the work-ms column for
    // real cost; treat per-node selfMs on GPU as recording weight only.
    std::printf("NOTE: per-node profile times are RECORDING time on GPU, "
                "not GPU execution — trust the work-ms column.\n");
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
    // Warm past the entrance choreography (mount transitions settle
    // within ~2s across the catalog) so the table reports STEADY STATE —
    // the number a running gallery feels. Entrances are one-shots; their
    // cost is real but belongs to a different budget than the loop. On GPU
    // the warmup also absorbs pipeline compilation.
    //
    // The counts are a TIME budget, not a constant. 240 + 120 was free when
    // every scene cost under 50 ms; the studies reach 200 ms on raster, where
    // the same counts are 70 seconds for one row. So: probe, then spend at
    // most a few seconds per scene. A shortened run is marked in the table,
    // because a scene warmed for 0.4 s of scene time is still inside its
    // entrance and its average says something different from the others'.
    // Asking for ONE scene means you want that scene's real number, so the
    // budget only applies to a sweep.
    const double warmBudgetMs = only >= 0 ? 1e9 : 4000;
    const double sampleBudgetMs = only >= 0 ? 1e9 : 2500;
    // The caps are named because the capture frame is DERIVED from them
    // below; if either moves, the capture moves with it rather than
    // silently drifting.
    constexpr int kProbeFrames = 8, kMinSampleFrames = 24;
    constexpr int kMaxWarmFrames = 240 - kProbeFrames;
    constexpr int kMaxSampleFrames = 120;
    for (int f = 0; f < kProbeFrames; ++f) {
      surface->getCanvas()->clear(clearColor);
      stage.frame(*surface->getCanvas(), 1.0 / 60.0);
    }
    const double probeMs = std::max(0.01, stage.stats.average());
    const int warmFrames = std::max(
        0, std::min(kMaxWarmFrames, (int)(warmBudgetMs / probeMs)));
    const int sampleFrames =
        std::max(kMinSampleFrames,
                 std::min(kMaxSampleFrames, (int)(sampleBudgetMs / probeMs)));
    const bool shortened = warmFrames < kMaxWarmFrames;
    anyShortened = anyShortened || shortened;
    for (int f = 0; f < warmFrames; ++f) {
      surface->getCanvas()->clear(clearColor);
      stage.frame(*surface->getCanvas(), 1.0 / 60.0);
    }
    stage.stats = {};
    double reconcileMs = 0, layoutMs = 0, volatileMs = 0, paintMs = 0;
    for (int f = 0; f < sampleFrames; ++f) {
      surface->getCanvas()->clear(clearColor);
      stage.frame(*surface->getCanvas(), 1.0 / 60.0);
      const Composer::Stats &cs = stage.composer->stats();
      reconcileMs += cs.reconcileMs;
      layoutMs += cs.layoutMs;
      volatileMs += cs.volatileMs;
      paintMs += cs.paintMs;
    }
    // ---- capture determinism -------------------------------------------
    // Everything above is a TIME budget, so `warmFrames` and `sampleFrames`
    // both depend on how fast this machine happened to be. That is fine for
    // a timing table and fatal for a PNG: it means the frame we capture is
    // whatever frame the budget happened to reach, so the same binary on the
    // same sources renders a different image every run.
    //
    // It was not a theory. Two consecutive sweeps of this harness differed
    // on 15 of 45 scenes — `daemon console` by 71% of the frame, `vertigo
    // titles` by 40%, `motion poster` by 46% — which means no gallery scene
    // has ever been verifiable frame to frame, and every "it still looks
    // right" check on one of those was reading noise.
    //
    // So the capture always lands at the SAME scene time. kCaptureFrame is
    // the maximum the budgeted path can already reach (probe + warm +
    // sample) — DERIVED from those two caps rather than restated, so that
    // raising either moves the capture with it instead of letting the
    // top-up silently stop reaching. A fast scene tops up by zero; a slow one
    // pays the difference and is worth it, because an image nobody can
    // reproduce cannot be reviewed. The timing table above is untouched —
    // it keeps its budget and its `*` mark.
    constexpr int kCaptureFrame =
        kProbeFrames + kMaxWarmFrames + kMaxSampleFrames;
    const int stepped = kProbeFrames + warmFrames + sampleFrames;
    for (int f = stepped; f < kCaptureFrame; ++f) {
      surface->getCanvas()->clear(clearColor);
      stage.frame(*surface->getCanvas(), 1.0 / 60.0);
    }

    char canvasLabel[24];
    std::snprintf(canvasLabel, sizeof(canvasLabel), "%dx%d",
                  (int)sceneSize.width(), (int)sceneSize.height());
    char nameLabel[40];
    std::snprintf(nameLabel, sizeof(nameLabel), "%s%s", stage.scene->name(),
                  shortened ? " *" : "");
    const double n = (double)sampleFrames;
    std::printf(
        "%-20s %10s %8.2f %8.2f %9.0f %6.2f %6.2f %6.2f %6.2f   rec %zu "
        "painted %zu\n",
        nameLabel, canvasLabel, stage.stats.average(),
        stage.stats.percentile(0.99), stage.stats.fps(), reconcileMs / n,
        layoutMs / n, volatileMs / n, paintMs / n,
        stage.composer->stats().picturesRecorded,
        stage.composer->stats().nodesPainted);
    // Capture the PNG at 2x: the stats above ran at 1x, but the saved
    // frame re-renders through a scaled canvas so review images are
    // sharp (Cache::Texture re-bakes at the capture scale). The studies
    // brought canvases up to 1940 px wide with them, so the factor gives way
    // rather than the ceiling: a 2x twoadvanced_v4 is a 3880x3120 PNG, and
    // nobody reviews one of those.
    const float captureScale =
        std::max(1.0f, std::min(2.0f, 2400.0f / sceneSize.width()));
#ifdef SIGILCOMPOSE_GALLERY_HEADLESS_GPU
    if (gpu) {
      // Same rule as the raster path below, and it has to be said twice
      // because this branch has its own frame() and its own encode and
      // `continue`s before reaching it. Measured: without this, the same
      // scene differs run-to-run by ~1400 px, every one of them inside the
      // overlay band, on every scene tested.
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
              outDir + "/gallery_" + stage.scene->name() + ".png";
          SkFILEWStream stream(path.c_str());
          if (stream.isValid())
            SkPngEncoder::Encode(&stream, bm.pixmap(), {});
        }
      }
      continue;
    }
#endif
    // Clean captures: the FPS overlay bakes live wall-clock digits into the
    // pixels, which makes every capture differ run-to-run — with it off the
    // scene content is deterministic (fixed dt, seeded rngs) and captures
    // diff meaningfully across builds. The GPU branch above sets this too;
    // for a long time it did not, and `continue`d past this line eight lines
    // earlier — so this comment was a correct and complete diagnosis of a
    // bug that was live on the sibling path the whole time it sat here.
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
        outDir + "/gallery_" + stage.scene->name() + ".png";
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
  return 0;
}

} // namespace compose_gallery
