#include "GalleryScenes.h"

#include <gtest/gtest.h>

#include <include/core/SkSurface.h>

namespace compose_gallery {
namespace {

struct ClockProbeScene final : Scene {
  double elapsed = -1.0;

  const char *name() const override { return "clock_probe"; }

  void setup(Composer &composer, sigil::motion::Ticker &) override {
    composer.render(box());
  }

  void update(double nextElapsed, Composer &) override {
    elapsed = nextElapsed;
  }
};

TEST(ComposeGallery, EveryRegistryEntryHasAFactory) {
  EXPECT_EQ(kGallerySceneCount, 29);
  for (int index = 0; index < kGallerySceneCount; ++index) {
    SCOPED_TRACE(index);
    EXPECT_NE(makeScene(index), nullptr);
  }
  EXPECT_EQ(makeScene(-1), nullptr);
  EXPECT_EQ(makeScene(kGallerySceneCount), nullptr);
}

TEST(ComposeGallery, FixedFramesAdvanceSyntheticElapsedTime) {
  GalleryStage stage;
  stage.showStats = false;
  auto probe = std::make_unique<ClockProbeScene>();
  ClockProbeScene *probePtr = probe.get();
  stage.activate(std::move(probe));
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(static_cast<int>(kSceneSize.width()),
                                static_cast<int>(kSceneSize.height())));
  ASSERT_NE(surface, nullptr);

  constexpr double dt = 1.0 / 60.0;
  for (int frame = 0; frame < 120; ++frame)
    stage.frame(*surface->getCanvas(), dt);

  EXPECT_NEAR(stage.clock.elapsed(), 2.0, 1e-9);
  EXPECT_NEAR(probePtr->elapsed, 2.0, 1e-9);

  const double beforeWallFrame = stage.clock.elapsed();
  stage.frame(*surface->getCanvas());
  EXPECT_LT(stage.clock.elapsed() - beforeWallFrame, 0.05);
}

TEST(ComposeGallery, PresentationCadenceResetsAcrossPause) {
  GalleryStage stage;
  stage.stats.addPresent(16.0);
  stage.stats.addPresent(1000.0);
  stage.lastPresent = std::chrono::steady_clock::now();

  stage.resetPresentationCadence();

  EXPECT_TRUE(stage.stats.presentMs.empty());
  EXPECT_EQ(stage.lastPresent, std::chrono::steady_clock::time_point{});
}

} // namespace
} // namespace compose_gallery
