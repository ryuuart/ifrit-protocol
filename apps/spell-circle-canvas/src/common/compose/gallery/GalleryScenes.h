#pragma once
// ComposeGallery scene registry + headless FPS runner. Scenes live in
// category headers; the Qt Quick app (ComposeGalleryView + qml/Main.qml)
// and --headless both build on this. Each scene names the
// STRESS_TESTS.md catalog items it exercises.

#include "GalleryCore.h"
#include "ScenesChrome.h"
#include "ScenesData.h"
#include "ScenesFlourish.h"
#include "ScenesGame.h"
#include "ScenesMotion.h"
#include "ScenesOrganic.h"
#include "ScenesOrnament.h"
#include "ScenesAero.h"
#include "ScenesConsole.h"
#include "ScenesKinetic.h"
#include "ScenesNetwork.h"
#include "ScenesPersona.h"
#include "ScenesSkillTree.h"
#include "ScenesY2k.h"
#include "ScenesScale.h"

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

#include <filesystem>

namespace compose_gallery {

struct SceneInfo {
  const char *name;
  const char *category;
  const char *catalog; // STRESS_TESTS.md items exercised
};

inline constexpr SceneInfo kScenes[] = {
    {"rpg hud", "Showcase", "practical UI"},
    {"manuscript", "Showcase", "ornament"},
    {"nine slice", "Showcase", "#9 texture-gen"},
    {"botanical", "Showcase", "generative"},
    {"ui particles", "Showcase", "SoA scale"},
    {"scoreboard", "Composition & data", "#2"},
    {"slots", "Composition & data", "#4"},
    {"grid + query", "Composition & data", "#5 #6"},
    {"transitions", "Composition & data", "#18"},
    {"load", "Composition & data", "#21"},
    {"headline", "Animation", "#17"},
    {"blend", "Animation", "#3"},
    {"chrome", "Chrome & decoration", "#8 #9 #10"},
    {"sksl border", "Chrome & decoration", "#11"},
    {"crt + bloom", "Effects", "#13 #14"},
    {"tile map", "Tiling", "#15"},
    {"derive", "Derive", "#7 #12"},
    {"organic", "Showcase", "#5 #9 #10 #12 shapes/layouts"},
    {"flourish", "Showcase", "the whole surface, at once"},
    {"kinetic card", "Showcase", "\xc2\xa7""8 kinetic grammar (study port)"},
    {"night network", "Showcase", "the brush engine, ten constructions"},
    {"persona menu", "Showcase", "\xc2\xa7""1 verified P3R grammar"},
    {"aero desktop", "Showcase", "\xc2\xa7""6 glass + colorization"},
    {"y2k chrome", "Showcase", "\xc2\xa7""2/\xc2\xa7""3 presets A/B"},
    {"passive tree", "Showcase", "\xc2\xa7""5 linework + orbit router"},
    {"daemon console", "Showcase", "console() LineRing feed"},
};
inline constexpr int kGallerySceneCount =
    (int)(sizeof(kScenes) / sizeof(kScenes[0]));

inline std::unique_ptr<Scene> makeScene(int index) {
  switch (index) {
  case 0: return std::make_unique<RpgHudScene>();
  case 1: return std::make_unique<ManuscriptScene>();
  case 2: return std::make_unique<NineSliceScene>();
  case 3: return std::make_unique<BotanicalScene>();
  case 4: return std::make_unique<UiParticleScene>();
  case 5: return std::make_unique<ScoreboardScene>();
  case 6: return std::make_unique<SlotsScene>();
  case 7: return std::make_unique<GridScene>();
  case 8: return std::make_unique<TransitionScene>();
  case 9: return std::make_unique<LoadScene>();
  case 10: return std::make_unique<HeadlineScene>();
  case 11: return std::make_unique<BlendScene>();
  case 12: return std::make_unique<ChromeScene>();
  case 13: return std::make_unique<SkslBorderScene>();
  case 14: return std::make_unique<CrtScene>();
  case 15: return std::make_unique<TileScene>();
  case 16: return std::make_unique<DeriveScene>();
  case 17: return std::make_unique<OrganicScene>();
  case 18: return std::make_unique<FlourishScene>();
  case 19: return std::make_unique<KineticCardScene>();
  case 20: return std::make_unique<NightNetworkScene>();
  case 21: return std::make_unique<PersonaMenuScene>();
  case 22: return std::make_unique<AeroDesktopScene>();
  case 23: return std::make_unique<Y2kChromeScene>();
  case 24: return std::make_unique<SkillTreeScene>();
  case 25: return std::make_unique<DaemonConsoleScene>();
  default: return nullptr;
  }
}

inline int runHeadless(const std::string &outDir, bool gpu = false) {
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
  for (int i = 0; i < kGallerySceneCount; ++i) {
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
    // sharp (Cache::Texture re-bakes at the capture scale). GPU sweeps
    // skip captures: the stage's caches hold Graphite-backed images that
    // must not be replayed onto a raster canvas, and the raster sweep
    // already writes identical imagery.
    if (gpu)
      continue;
    constexpr float kCapture = 2.0f;
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
    std::printf("wrote %d gallery scenes to %s\n", kGallerySceneCount,
                outDir.c_str());
  return 0;
}

} // namespace compose_gallery
