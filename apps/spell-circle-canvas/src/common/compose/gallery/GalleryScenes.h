#pragma once
// ComposeGallery scene registry + headless FPS runner. Scenes live in
// category headers; the Qt Quick app (ComposeGalleryView + qml/Main.qml)
// and --headless both build on this. Each scene names the
// STRESS_TESTS.md catalog items it exercises.

#include "GalleryCore.h"
#include "ScenesChrome.h"
#include "ScenesData.h"
#include "ScenesGame.h"
#include "ScenesMotion.h"
#include "ScenesOrnament.h"
#include "ScenesScale.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

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
  default: return std::make_unique<DeriveScene>();
  }
}

inline int runHeadless(const std::string &outDir) {
  std::filesystem::create_directories(outDir);
  std::printf("%-14s %12s %10s %14s\n", "scene", "work ms", "p99 ms",
              "headroom fps");
  for (int i = 0; i < kGallerySceneCount; ++i) {
    GalleryStage stage;
    stage.activate(makeScene(i));
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)kSceneSize.width(), (int)kSceneSize.height()));
    for (int f = 0; f < 120; ++f) {
      surface->getCanvas()->clear(SK_ColorBLACK);
      stage.frame(*surface->getCanvas(), 1.0 / 60.0);
    }
    std::printf("%-14s %12.2f %10.2f %14.0f\n", stage.scene->name(),
                stage.stats.average(), stage.stats.percentile(0.99),
                stage.stats.fps());
    SkBitmap bm;
    bm.allocPixels(surface->imageInfo());
    surface->readPixels(bm.pixmap(), 0, 0);
    const std::string path =
        outDir + "/gallery_" + stage.scene->name() + ".png";
    SkFILEWStream stream(path.c_str());
    if (!stream.isValid() || !SkPngEncoder::Encode(&stream, bm.pixmap(), {}))
      return 1;
  }
  std::printf("wrote %d gallery scenes to %s\n", kGallerySceneCount,
              outDir.c_str());
  return 0;
}

} // namespace compose_gallery
