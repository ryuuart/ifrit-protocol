// The instanced leaf: the pool's lanes, the atlas bake and its variants,
// the stamp in both modes, the per-sprite blend and the pick. The kit's
// placers that fill a pool are the shape binary's, beside the other
// shelves.

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

TEST(ComposeInstances, ThePerSpriteBlendAccumulatesWhereALayerCannot) {
  // Nothing in the chain from instances() to drawSpriteAtlas carried a
  // blend mode, so every pool composited kSrcOver. Element::blend() looks
  // like the fix and is not: it flattens the field into a layer and
  // composites it ONCE, so overlapping sprites never accumulate — which
  // is the entire colour model of an additive particle system (Reeves'
  // 1982 wall of fire has no palette, only an overlap count).
  auto build = [](SkBlendMode blend) {
    auto atlas = std::make_shared<instancing::Atlas>(1.0f);
    atlas->cell(
        box().width(40).height(40).fill(Fill::color({0.25f, 0.25f, 0.25f, 1})),
        {40, 40});
    auto pool = std::make_shared<instancing::Pool>();
    for (int i = 0; i < 3; ++i)  // three sprites stacked on one spot
      pool->add({100, 100});
    return box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data, blend));
  };

  Host over(200, 200);
  over.composer.render(build(SkBlendMode::kSrcOver));
  over.frame();
  const int overR = SkColorGetR(over.pixel(100, 100));

  Host plus(200, 200);
  plus.composer.render(build(SkBlendMode::kPlus));
  plus.frame();
  const int plusR = SkColorGetR(plus.pixel(100, 100));

  EXPECT_GT(overR, 40);          // one opaque sprite's worth
  EXPECT_LT(overR, 90);          // …and three of them are no brighter
  EXPECT_GT(plusR, overR + 60);  // additive stacks all three
}

TEST(ComposeInstances, ThePerInstanceSizeLaneCarriesNonUniformScale) {
  // The most-cited gap in the program's hard half: SkRSXform carries
  // (scos, ssin) and ONE scale by construction, so Reeves' 1982
  // `streaked spherical` particle — a quad 0.5·|v| long by `size` wide,
  // aspect swinging ~2.4:1 to under 1:1 across its life — could not be
  // instanced at all. One study hand-built the vertex buffer in 69 lines
  // and lost every decoration slot and all picture caching with it.
  //
  // The lane is opt-in: a pool that never asks for it keeps the pure
  // RSXform path and costs nothing.
  auto build = [](bool stretch) {
    auto atlas = std::make_shared<instancing::Atlas>(1.0f);
    atlas->cell(box().width(20).height(20).fill(Fill::color({1, 0, 0, 1})),
                {20, 20});
    auto pool = std::make_shared<instancing::Pool>();
    pool->add({100, 100});
    if (stretch) {
      pool->sizes()[0] = {4.0f, 0.25f};  // 80 x 5 — a streak
      pool->commit();
    }
    return box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data));
  };
  auto redSpan = [](Host& host, bool horizontal) {
    int n = 0;
    for (int i = 0; i < 200; ++i) {
      const SkColor c = horizontal ? host.pixel(i, 100) : host.pixel(100, i);
      n += SkColorGetR(c) > 180;
    }
    return n;
  };

  Host square(200, 200);
  square.composer.render(build(false));
  square.frame();
  EXPECT_NEAR(redSpan(square, true), 20, 2);
  EXPECT_NEAR(redSpan(square, false), 20, 2);

  Host streak(200, 200);
  streak.composer.render(build(true));
  streak.frame();
  EXPECT_NEAR(redSpan(streak, true), 80, 3);  // four times as wide…
  EXPECT_NEAR(redSpan(streak, false), 5, 2);  // …and a quarter as tall
}

TEST(ComposeInstances, ANonUniformInstanceStillRotatesAboutItsCentre) {
  // The quad is built by hand on this path, so the anchor has to come out
  // where RSXform would have put it — a 90-degree turn must swap the
  // extents in place, not orbit the sprite away from its position.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->cell(box().width(20).height(20).fill(Fill::color({1, 0, 0, 1})),
              {20, 20});
  auto pool = std::make_shared<instancing::Pool>();
  pool->add({100, 100}, 0, (float)M_PI_2);
  pool->sizes()[0] = {4.0f, 0.25f};
  pool->commit();

  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  int across = 0, down = 0;
  for (int i = 0; i < 200; ++i) {
    across += SkColorGetR(host.pixel(i, 100)) > 180;
    down += SkColorGetR(host.pixel(100, i)) > 180;
  }
  EXPECT_NEAR(across, 5, 2);  // rotated: the extents swapped…
  EXPECT_NEAR(down, 80, 3);
  EXPECT_TRUE(SkColorGetR(host.pixel(100, 100)) > 180);  // …in place
}

TEST(ComposeInstances, TheAtlasChoosesItsOwnFilter) {
  // The last of the five hardcoded-kLinear paths. Instancing's biggest
  // real use is tilemaps and sprite sheets — pixel grids — where linear
  // filtering is exactly wrong.
  auto blend = [](SkFilterMode mode) {
    auto atlas = std::make_shared<instancing::Atlas>(1.0f);
    atlas->filter(mode);
    // A cell that is half red, half green: magnified, linear invents a
    // blend band across the seam and nearest does not.
    atlas->cell(
        box()
            .width(16)
            .height(16)
            .row()
            .child(box().width(8).height(16).fill(Fill::color({1, 0, 0, 1})))
            .child(box().width(8).height(16).fill(Fill::color({0, 1, 0, 1}))),
        {16, 16});
    auto pool = std::make_shared<instancing::Pool>();
    pool->add({100, 100}, 0, 0.0f, 8.0f);  // 8x magnification
    Host host(200, 200);
    host.composer.render(box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data)));
    host.frame();
    int mixed = 0;
    for (int x = 85; x < 115; ++x) {
      const SkColor c = host.pixel(x, 100);
      const bool pureRed = SkColorGetR(c) > 200 && SkColorGetG(c) < 40;
      const bool pureGreen = SkColorGetG(c) > 200 && SkColorGetR(c) < 40;
      mixed += !pureRed && !pureGreen;
    }
    return mixed;
  };
  EXPECT_GT(blend(SkFilterMode::kLinear), 2);
  EXPECT_LE(blend(SkFilterMode::kNearest), 1);
}

TEST(ComposeInstances, VariantsAreConsecutiveBakesOfOneRecipe) {
  // A variant is a separate BAKE of one recipe, addressed as first + v.
  // That is what tints() cannot do: a variant may differ by a whole
  // re-render — a per-channel ramp, a different shade table — rather than by
  // a multiply.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  const int first = atlas->variants(3, {20, 20}, [](int v) {
    const float g = 0.2f + 0.3f * (float)v;  // three distinct shades
    return box().fill(Fill::color({g, g, g, 1}));
  });
  EXPECT_EQ(atlas->frameCount(), 3);
  auto pool = std::make_shared<instancing::Pool>();
  for (int v = 0; v < 3; ++v) pool->add({30.0f + 60.0f * (float)v, 30.0f});
  auto frames = pool->frames();
  for (int v = 0; v < 3; ++v) frames[v] = first + v;
  pool->commit();
  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  const unsigned r0 = SkColorGetR(host.pixel(30, 30));
  const unsigned r1 = SkColorGetR(host.pixel(90, 30));
  const unsigned r2 = SkColorGetR(host.pixel(150, 30));
  EXPECT_LT(r0 + 20, r1);  // strictly brighter per variant
  EXPECT_LT(r1 + 20, r2);
}

TEST(ComposeInstances, AddAfterAlphasKeepsEveryFade) {
  // hasAlphas() is a length comparison against the position lane, so every
  // mutator has to keep the alpha lane in step. The sharp case is add()
  // AFTER the lane exists: a lane left one short would fail that length
  // test and silently drop every fade in the pool.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->cell(box().fill(Fill::color({1, 0, 0, 1})), {40, 40});
  auto pool = std::make_shared<instancing::Pool>();
  pool->add({40, 40});
  pool->alphas()[0] = 0.5f;
  pool->add({120, 40});  // the append that must not orphan the lane
  pool->commit();
  ASSERT_TRUE(pool->hasAlphas());
  EXPECT_FLOAT_EQ(pool->alphas()[0], 0.5f);  // the fade survives the append
  EXPECT_FLOAT_EQ(pool->alphas()[1], 1.0f);  // the new instance is opaque
  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  const unsigned faded = SkColorGetR(host.pixel(40, 40));
  const unsigned opaque = SkColorGetR(host.pixel(120, 40));
  EXPECT_GT(opaque, 240u);  // the appended sprite stamps at full
  EXPECT_LT(faded, 170u);   // half-alpha red over black ≈ 128
  EXPECT_GT(faded, 80u);
}

TEST(ComposeInstances, ClearDropsTheAlphaLaneWithItsGeneration) {
  // The worse half of the same desync: clear() then re-add the SAME count.
  // A lane that survived the clear would line up with the position lane
  // again and apply the previous generation's fades to entirely different
  // sprites.
  instancing::Pool pool;
  pool.add({10, 10});
  pool.add({30, 10});
  pool.alphas()[0] = 0.25f;
  pool.alphas()[1] = 0.5f;
  pool.clear();
  pool.add({50, 50});
  pool.add({70, 50});
  EXPECT_FALSE(pool.hasAlphas());  // nothing carried forward: stamps opaque
  auto fresh = pool.alphas();      // re-opting in starts from opaque
  EXPECT_FLOAT_EQ(fresh[0], 1.0f);
  EXPECT_FLOAT_EQ(fresh[1], 1.0f);
}

TEST(ComposeInstances, ResizeKeepsTheAlphaLaneInStepBothWays) {
  instancing::Pool pool;
  pool.add({10, 10});
  pool.add({30, 10});
  pool.alphas()[0] = 0.5f;
  pool.resize(4);  // grow: existing fades survive, new slots are opaque
  ASSERT_TRUE(pool.hasAlphas());
  EXPECT_FLOAT_EQ(pool.alphas()[0], 0.5f);
  EXPECT_FLOAT_EQ(pool.alphas()[2], 1.0f);
  EXPECT_FLOAT_EQ(pool.alphas()[3], 1.0f);
  pool.resize(1);  // shrink: the lane truncates with the pool
  ASSERT_TRUE(pool.hasAlphas());
  ASSERT_EQ(pool.alphas().size(), 1u);
  EXPECT_FLOAT_EQ(pool.alphas()[0], 0.5f);  // the kept slot keeps its fade
}

TEST(ComposeInstances, PickInvertsTheStampTopmostFirst) {
  // hitTest cannot see a pool instance at all — the whole field is ONE
  // custom() draw as far as the tree is concerned. pick() is the inverse
  // projection, read against the same lanes the stamp reads: rotation,
  // scale, and topmost-wins where stamps overlap.
  using namespace sigil::compose::instancing;
  Atlas atlas(1.0f);
  atlas.cell(box().fill(Fill::color({1, 0, 0, 1})), {40, 20});
  Pool pool;
  pool.add({100, 100});     // instance 0
  pool.add({120, 100});     // instance 1, overlapping 0's right side
  pool.scales()[1] = 0.5f;  // 20x10 quad at (120,100)
  pool.rotations()[0] = (float)M_PI / 2.0f;  // 0 is rotated 90°: 20x40 now
  pool.commit();

  // Overlap region: (118, 100) is inside both — topmost (1) wins.
  auto hit = pick(pool, atlas, {118, 100});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, 1u);
  // Rotation honoured: (100, 117) is inside 0's rotated quad (tall now),
  // outside its unrotated footprint.
  hit = pick(pool, atlas, {100, 117});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, 0u);
  // …and (117, 100) horizontally would have been inside UNrotated 0 but
  // is outside the rotated quad and outside 1.
  EXPECT_FALSE(pick(pool, atlas, {84, 100}).has_value());
  // Scale honoured: outside 1's shrunken quad.
  EXPECT_FALSE(pick(pool, atlas, {135, 100}).has_value());
}

TEST(ComposeInstances, APerInstanceUVWindowAddressesInsideACell) {
  // The per-sprite texture rect existed all the way down —
  // drawSpriteAtlas reads tex[i] per sprite — and the only narrowing was
  // that a Pool could name a cell INDEX and never a RECT. So a strip of
  // artwork crawling behind a slit, or a sprite scrolling within its own
  // cell, was out of reach for want of a lane, not a draw path.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->filter(SkFilterMode::kNearest);
  // One cell, four vertical quarters: red, green, blue, white.
  atlas->cell(
      box()
          .width(16)
          .height(64)
          .column()
          .child(box().width(16).height(16).fill(Fill::color({1, 0, 0, 1})))
          .child(box().width(16).height(16).fill(Fill::color({0, 1, 0, 1})))
          .child(box().width(16).height(16).fill(Fill::color({0, 0, 1, 1})))
          .child(box().width(16).height(16).fill(Fill::color({1, 1, 1, 1}))),
      {16, 64});

  auto quarterAt = [&](float top) {
    auto pool = std::make_shared<instancing::Pool>();
    pool->add({100, 100});
    pool->texWindows()[0] = SkRect::MakeXYWH(0.0f, top, 1.0f, 0.25f);
    pool->sizes()[0] = {1.0f, 0.25f};  // draw the window at its own aspect
    pool->commit();
    Host host(200, 200);
    host.composer.render(box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data)));
    host.frame();
    return host.pixel(100, 100);
  };

  // Sliding the window down the cell selects each band in turn — one
  // pool, one cell, four different sprites.
  const SkColor a = quarterAt(0.00f), b = quarterAt(0.25f);
  const SkColor c = quarterAt(0.50f), d = quarterAt(0.75f);
  EXPECT_GT(SkColorGetR(a), 180);
  EXPECT_LT(SkColorGetG(a), 80);
  EXPECT_GT(SkColorGetG(b), 180);
  EXPECT_LT(SkColorGetR(b), 80);
  EXPECT_GT(SkColorGetB(c), 180);
  EXPECT_LT(SkColorGetR(c), 80);
  EXPECT_GT(SkColorGetR(d), 180);
  EXPECT_GT(SkColorGetB(d), 180);

  // The lane is opt-in: a pool that never asks keeps the whole cell.
  auto plain = std::make_shared<instancing::Pool>();
  plain->add({100, 100});
  EXPECT_FALSE(plain->hasTexWindows());
}

TEST(ComposeInstances, CommitPublishesABulkEdit) {
  instancing::Pool pool;
  pool.add({10, 10});
  pool.add({20, 20});
  const uint64_t after = pool.revision();
  for (SkPoint& p : pool.positions()) p.fY += 5;
  EXPECT_EQ(pool.revision(), after) << "a span write is staging, not publish";
  pool.commit();
  const uint64_t committed = pool.revision();
  EXPECT_NE(committed, after);
  pool.commit();
  EXPECT_NE(pool.revision(), committed) << "each publish is its own revision";
}

TEST(ComposeInstances, AFlightLaneCarriesTheEntranceThePoolCannot) {
  using namespace sigil::compose::instancing;
  Pool pool;
  pool.add({0, 0});
  pool.add({0, 0});

  // Two instances leaving at different times — the stagger IS the field,
  // which is why the times are per instance rather than one schedule.
  {
    std::span<Pool::Flight> flights = pool.flights();
    flights[0] = {.from = {0, 0},
                  .to = {100, 0},
                  .alphaFrom = 0.0f,
                  .alphaTo = 1.0f,
                  .start = 0.0f,
                  .duration = 1.0f};
    flights[1] = {.from = {0, 0},
                  .to = {100, 0},
                  .rotateFrom = 0.0f,
                  .rotateTo = 2.0f,
                  .start = 1.0f,
                  .duration = 1.0f};
  }
  ASSERT_TRUE(pool.hasFlights());

  pool.fly(0.5f);
  EXPECT_FLOAT_EQ(pool.positions()[0].fX, 50.0f);
  EXPECT_FLOAT_EQ(pool.alphas()[0], 0.5f) << "the opacity lane flies too";
  EXPECT_FLOAT_EQ(pool.positions()[1].fX, 0.0f) << "not left yet";
  EXPECT_FLOAT_EQ(pool.rotations()[1], 0.0f);

  pool.fly(1.5f);
  EXPECT_FLOAT_EQ(pool.positions()[0].fX, 100.0f) << "landed, and holds";
  EXPECT_FLOAT_EQ(pool.positions()[1].fX, 50.0f);
  EXPECT_FLOAT_EQ(pool.rotations()[1], 1.0f);

  // One curve for the whole field: the variation between sprites is in
  // their times, not in their curves.
  pool.fly(0.5f, [](float u) { return u * u; });
  EXPECT_FLOAT_EQ(pool.positions()[0].fX, 25.0f);

  // A step publishes, or a Mode::Data leaf would replay its old picture.
  const uint64_t before = pool.revision();
  pool.fly(0.75f);
  EXPECT_NE(pool.revision(), before);
}

TEST(ComposeInstances, AFlightMaterialisesAtRestAndKeepsStepWithTheLanes) {
  using namespace sigil::compose::instancing;
  Pool pool;
  pool.add({10, 20}, 0, 0.5f, 2.0f);
  // Asking for the lane must not move anything: an instance whose flight
  // was never filled in stays exactly where it was put.
  (void)pool.flights();
  pool.fly(99.0f);
  EXPECT_FLOAT_EQ(pool.positions()[0].fX, 10.0f);
  EXPECT_FLOAT_EQ(pool.rotations()[0], 0.5f);
  EXPECT_FLOAT_EQ(pool.scales()[0], 2.0f);

  // …and the lane keeps step with the pool through add and resize, or a
  // length test would switch it off and a re-fill would apply a previous
  // generation's flights.
  pool.add({30, 40});
  EXPECT_TRUE(pool.hasFlights());
  pool.resize(5);
  EXPECT_TRUE(pool.hasFlights());
  pool.fly(0.0f);
  EXPECT_FLOAT_EQ(pool.positions()[1].fX, 30.0f) << "added at rest";

  // A zero duration is "appears then", not a division by zero.
  std::span<Pool::Flight> flights = pool.flights();
  flights[0] = {.from = {0, 0}, .to = {80, 0}, .start = 2.0f};
  pool.fly(1.9f);
  EXPECT_FLOAT_EQ(pool.positions()[0].fX, 0.0f);
  pool.fly(2.0f);
  EXPECT_FLOAT_EQ(pool.positions()[0].fX, 80.0f);

  // clear() drops the lane with its generation, so the next fill starts
  // without one rather than lining up again against old flights.
  pool.clear();
  pool.add({7, 7});
  EXPECT_FALSE(pool.hasFlights());
}
