#pragma once
// ComposeGallery scene registry + headless FPS runner. Scenes live in
// category headers; the Qt Quick app (ComposeGalleryView + qml/Main.qml)
// and --headless both build on this. Each scene names the
// STRESS_TESTS.md catalog items it exercises.

#include "GalleryCore.h"
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

inline constexpr SceneInfo kScenes[] = {
    {"world hud", "Showcase",
     "voxygen dimensions \xe2\x80\x94 bars, hotbar, minimap"},
    {"manuscript", "Showcase", "ornament"},
    {"nine slice", "Showcase", "#9 texture-gen"},
    {"botanical", "Showcase", "generative"},
    {"ui particles", "Showcase", "SoA scale \xc2\xb7 instances()"},
    {"load", "Scale", "#21 sustained load"},
    {"tile map", "Tiling", "#15"},
    {"organic", "Showcase", "#5 #9 #10 #12 shapes/layouts"},
    {"flourish", "Showcase", "the whole surface, at once"},
    {"kinetic card", "Showcase", "\xc2\xa7""8 kinetic grammar (study port)"},
    {"night network", "Showcase", "the brush engine, twelve constructions"},
    {"persona menu", "Showcase", "\xc2\xa7""1 verified P3R grammar"},
    {"aero desktop", "Showcase", "\xc2\xa7""6 glass + colorization"},
    {"y2k chrome", "Showcase", "\xc2\xa7""2/\xc2\xa7""3 presets A/B"},
    {"passive tree", "Showcase", "\xc2\xa7""5 linework + orbit router"},
    {"daemon console", "Showcase", "console() LineRing feed"},
    {"motion poster", "Showcase",
     "EMBER GATE \xe2\x80\x94 the flagship living poster"},
    {"zellige", "Showcase", "girih Hankin PIC \xe2\x80\x94 regenerating"},
    {"beethoven", "Showcase", "\xc2\xa7""7 Brockmann arc table, trim reveal"},
    {"loot grid", "Showcase",
     "D2 hoard \xe2\x80\x94 generated materials, instances()"},
    {"gerstner grid", "Showcase",
     "Capital 1962 \xe2\x80\x94 the mobile grid, run"},
    {"cosmati", "Tiling",
     "opus sectile \xe2\x80\x94 quincunx, guilloche, quarried stone"},
};
inline constexpr int kGallerySceneCount =
    (int)(sizeof(kScenes) / sizeof(kScenes[0]));

/** Scene index for a name or a decimal index; -1 when nothing matches.
 *  Names match case-insensitively on any unique substring, so `--scene y2k`
 *  and `--scene "y2k chrome"` both land on the same entry. */
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
  int hit = -1;
  for (int i = 0; i < kGallerySceneCount; ++i) {
    const std::string name = lower(kScenes[i].name);
    if (name == needle)
      return i;
    if (name.find(needle) != std::string::npos && hit < 0)
      hit = i;
  }
  return hit;
}

inline std::unique_ptr<Scene> makeScene(int index) {
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

/** Sweeps the registry (or one scene when `only` is a valid index),
 *  printing the FPS table and writing a 2x PNG per scene. */
inline int runHeadless(const std::string &outDir, bool gpu = false,
                       int only = -1) {
#ifdef SIGILCOMPOSE_GALLERY_HEADLESS_GPU
  std::unique_ptr<SkiaGraphiteContext> graphite;
  if (gpu) {
    graphite = makeHeadlessGraphite();
    if (!graphite) {
      std::fprintf(stderr, "Graphite context creation failed; no GPU sweep\n");
      return 1;
    }
    std::printf("backend: Graphite GPU (work ms = CPU + synced GPU)\n");
  }
#else
  if (gpu) {
    std::fprintf(stderr, "this build has no headless GPU backend\n");
    return 1;
  }
#endif
  std::filesystem::create_directories(outDir);
  std::printf("%-14s %8s %8s %9s %6s %6s %6s %6s\n", "scene", "work ms",
              "p99 ms", "fps", "recon", "layout", "volat", "paint");
  const int first = only >= 0 ? only : 0;
  const int last = only >= 0 ? only + 1 : kGallerySceneCount;
  for (int i = first; i < last; ++i) {
    GalleryStage stage;
    stage.activate(makeScene(i));
    SkDebugf("=== scene %s\n", stage.scene->name());
    const SkImageInfo info = SkImageInfo::MakeN32Premul(
        (int)kSceneSize.width(), (int)kSceneSize.height());
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
    for (int f = 0; f < 240; ++f) {
      surface->getCanvas()->clear(SK_ColorBLACK);
      stage.frame(*surface->getCanvas(), 1.0 / 60.0);
    }
    stage.stats = {};
    double reconcileMs = 0, layoutMs = 0, volatileMs = 0, paintMs = 0;
    for (int f = 0; f < 120; ++f) {
      surface->getCanvas()->clear(SK_ColorBLACK);
      stage.frame(*surface->getCanvas(), 1.0 / 60.0);
      const Composer::Stats &cs = stage.composer->stats();
      reconcileMs += cs.reconcileMs;
      layoutMs += cs.layoutMs;
      volatileMs += cs.volatileMs;
      paintMs += cs.paintMs;
    }
    std::printf(
        "%-14s %8.2f %8.2f %9.0f %6.2f %6.2f %6.2f %6.2f   rec %zu painted %zu\n",
        stage.scene->name(), stage.stats.average(),
        stage.stats.percentile(0.99), stage.stats.fps(), reconcileMs / 120,
        layoutMs / 120, volatileMs / 120, paintMs / 120,
        stage.composer->stats().picturesRecorded,
        stage.composer->stats().nodesPainted);
    // Capture the PNG at 2x: the stats above ran at 1x, but the saved
    // frame re-renders through a scaled canvas so review images are
    // sharp (Cache::Texture re-bakes at the capture scale).
#ifdef SIGILCOMPOSE_GALLERY_HEADLESS_GPU
    if (gpu) {
      // GPU captures read back through the async path (a Graphite surface
      // cannot readPixels synchronously) — these are what the interactive
      // QQuickRhiItem gallery actually shows, so visual QA runs HERE, not
      // on the raster sweep.
      constexpr float kGpuCapture = 2.0f;
      const SkImageInfo shotInfo = SkImageInfo::MakeN32Premul(
          (int)(kSceneSize.width() * kGpuCapture),
          (int)(kSceneSize.height() * kGpuCapture));
      sk_sp<SkSurface> shot =
          SkSurfaces::RenderTarget(graphite->recorder(), shotInfo);
      if (shot) {
        shot->getCanvas()->clear(SK_ColorBLACK);
        shot->getCanvas()->scale(kGpuCapture, kGpuCapture);
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
    constexpr float kCapture = 2.0f;
    // Clean captures: the FPS overlay bakes live wall-clock digits into the
    // pixels, which makes every capture differ run-to-run — with it off the
    // scene content is deterministic (fixed dt, seeded rngs) and captures
    // diff meaningfully across builds.
    stage.showStats = false;
    sk_sp<SkSurface> shot = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)(kSceneSize.width() * kCapture),
        (int)(kSceneSize.height() * kCapture)));
    shot->getCanvas()->clear(SK_ColorBLACK);
    shot->getCanvas()->scale(kCapture, kCapture);
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
  if (!gpu)
    std::printf("wrote %d gallery scene%s to %s\n", last - first,
                last - first == 1 ? "" : "s", outDir.c_str());
  return 0;
}

} // namespace compose_gallery
