// The core binary's share of ComposeTestEngineDocs.cpp: the suites whose
// subjects are core-tier values, cut from that file so each test binary links
// only the target it exercises.

#include "support/CoreTestSupport.h"

TEST(ComposeInstances, StampsAtlasCellsAtPoolPositionsWithTint) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {20, 20});
  auto pool = std::make_shared<Pool>();
  pool->add({50, 50});                                // white cell, untinted
  pool->add({150, 50}, 0, 0.0f, 1.0f, {1, 0, 0, 1});  // tinted red
  // The bake itself: sheet exists and the cell's center is opaque white.
  ASSERT_TRUE(atlas->ensureBaked(fonts()));
  ASSERT_TRUE(atlas->image());
  {
    SkBitmap probe;
    probe.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    ASSERT_TRUE(atlas->image()->readPixels(nullptr, probe.pixmap(), 20, 20));
    EXPECT_EQ(probe.getColor(0, 0), SK_ColorWHITE);
  }
  host.composer.render(box().child(instances(atlas, pool)));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(150, 50), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // between stamps: nothing
}

TEST(ComposeInstances, DataModePrunesUntilTouched) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {16, 16});
  auto pool = std::make_shared<Pool>();
  pool->add({40, 40});
  auto describe = [&] { return box().child(instances(atlas, pool)); };
  host.composer.render(describe());
  host.frame();
  // Unchanged pool: the re-describe prunes (memo hit), the cached picture
  // replays, nothing re-records.
  host.composer.render(describe());
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // Mutate + touch + render: repaints exactly once, pixels move.
  pool->positions()[0] = {120, 40};
  pool->commit();
  host.composer.render(describe());
  EXPECT_TRUE(host.composer.dirty());
  host.frame();
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(120, 40), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);
}

TEST(ComposeInstances, LiveModeReadsThePoolEveryFrame) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {16, 16});
  auto pool = std::make_shared<Pool>();
  pool->add({40, 40});
  host.composer.render(box().child(instances(atlas, pool, Mode::Live)));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorWHITE);
  // No touch(), no render() — the Cache::None leaf reads the latest data.
  pool->positions()[0] = {140, 140};
  host.frame();
  EXPECT_EQ(host.pixel(140, 140), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);
}

TEST(ComposeInstances, RepeaterLawExponentialScaleLinearEverythingElse) {
  using namespace sigil::compose::instancing;
  Pool pool;
  place::repeat(pool, 4, {10, 10}, {5, 0}, 0.1f, 0.5f, 1.0f, 0.25f);
  ASSERT_EQ(pool.size(), 4u);
  EXPECT_FLOAT_EQ(pool.positions()[3].fX, 25.0f);  // linear translate
  EXPECT_FLOAT_EQ(pool.rotations()[3], 0.3f);      // linear rotate
  EXPECT_FLOAT_EQ(pool.scales()[3], 0.125f);       // pow(0.5, 3)
  // Opacity and tint are separate lanes: the opacity ramp writes alphas[],
  // and tints[].fA stays exactly what the author put there. Folding one into
  // the other would make a tinted pool silently un-tintable.
  EXPECT_FLOAT_EQ(pool.alphas()[0], 1.0f);  // opacity lerp endpoints
  EXPECT_FLOAT_EQ(pool.alphas()[3], 0.25f);
  EXPECT_FLOAT_EQ(pool.tints()[3].fA, 1.0f);  // untouched
}
