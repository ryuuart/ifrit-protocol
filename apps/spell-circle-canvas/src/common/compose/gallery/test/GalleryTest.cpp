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
  EXPECT_EQ(kCatalogSceneCount, 22);
  EXPECT_EQ(kGallerySceneCount, kCatalogSceneCount + kStudyCount);
  for (int index = 0; index < kGallerySceneCount; ++index) {
    SCOPED_TRACE(index);
    EXPECT_NE(makeScene(index), nullptr);
    EXPECT_STRNE(sceneInfo(index).name, "");
    EXPECT_STRNE(sceneInfo(index).category, "");
  }
  EXPECT_EQ(makeScene(-1), nullptr);
  EXPECT_EQ(makeScene(kGallerySceneCount), nullptr);
}

// The study table names sketch files by stem; the CMake list decides which
// ones are compiled in. Those two are edited in different files, so the only
// thing keeping them honest is this: a study named here but not linked has
// no factory, and would otherwise show up as a row that renders nothing.
TEST(ComposeGallery, EveryStudyInTheTableIsLinkedIn) {
  for (const StudyInfo &study : kStudies) {
    SCOPED_TRACE(study.key);
    EXPECT_NE(sketch::findStaticSketch(study.key), nullptr);
  }
  EXPECT_EQ(sketch::findStaticSketch("not_a_sketch"), nullptr);
  // And the other direction: a sketch added to SIGIL_SKETCH_STUDIES but not
  // to kStudies compiles, links, and never appears anywhere.
  EXPECT_EQ((int)sketch::staticSketches().size(), kStudyCount);
}

TEST(ComposeGallery, StudiesBringTheirOwnCanvas) {
  // A study declares its canvas from inside setup(), so the size is only
  // available after activate() has run it. The whole point of the exercise:
  // xcom_battlescape is a 320x200 screen at 4x and nothing else is.
  const int xcom = findScene("xcom_battlescape");
  ASSERT_GE(xcom, kCatalogSceneCount);
  GalleryStage stage;
  stage.showStats = false;
  stage.activate(makeScene(xcom));
  EXPECT_EQ(stage.sceneSize, SkSize::Make(1280, 800));
  EXPECT_NE(stage.sceneSize, kSceneSize);

  // A catalog scene declares nothing and keeps the common canvas.
  stage.activate(makeScene(0));
  EXPECT_EQ(stage.sceneSize, kSceneSize);
}

TEST(ComposeGallery, SceneLookupPrefersExactNamesAndAcceptsStudyKeys) {
  EXPECT_EQ(findScene("hello"), findScene("hello"));
  ASSERT_GE(findScene("hello"), 0);
  // The stem and the display name reach the same scene.
  EXPECT_EQ(findScene("slitscan_2001"), findScene("slitscan 2001"));
  EXPECT_EQ(findScene("psx_doom_fire"), findScene("psx doom"));
  // Exact still beats a substring that appears earlier in the registry.
  EXPECT_EQ(findScene("load"), 5);
  EXPECT_LT(findScene("nothing matches this"), 0);
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
