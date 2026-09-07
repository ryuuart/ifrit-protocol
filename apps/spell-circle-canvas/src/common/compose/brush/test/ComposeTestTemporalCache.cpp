// What a live value costs the cache: a quantized material is cacheable
// between its ticks, a continuous one stays live, and a held keyframe
// segment repaints nothing.

#include "support/BrushTestSupport.h"

namespace {

/** Host with a real FrameClock, so a material's injected uTime advances.
 *  (The shared Host deliberately has none — most tests want elapsed 0.) */
struct ClockedHost {
  sigil::motion::Ticker ticker;
  sigil::motion::FrameClock clock;
  Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface;
  double now = 0;

  ClockedHost(int w, int h) {
    composer.setClock(&clock);
    composer.setSize({(float)w, (float)h});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  }
  void frame(double dt) {
    now += dt;
    clock.tick(now);
    ticker.tick(dt);
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
  }
  SkBitmap grab(int w, int h) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
    surface->readPixels(bm.pixmap(), 0, 0);
    return bm;
  }
};

Element timedLeaf(float quantizeHz) {
  material::skia::Paint m = material::skia::Paint::sksl(heavyEffect(true));
  if (quantizeHz > 0) m.quantizeTime(quantizeHz);
  return box().child(
      box().width(400).height(400).key("plasma").fill(std::move(m)));
}

}  // namespace

TEST(ComposeCache, AQuantizedMaterialIsCacheableBetweenItsTicks) {
  // quantizeTime(4) at 60 FPS means the shader's inputs change four times a
  // second and the other 56 frames resolve to the SAME shader. Their pixels
  // are therefore identical to the last bake's — not similar, identical —
  // so the bake is still valid and the shader need not run.
  ClockedHost host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(timedLeaf(4.0f));
  for (int i = 0; i < 24; ++i) host.frame(1.0 / 60.0);
  const Composer::NodeCost* row = requireRow(host.composer, "plasma");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "a material stepping at 4 Hz still paid its shader 60 times a second";
}

TEST(ComposeCache, AContinuousMaterialStaysLive) {
  // The other half of the same rule, and the reason it is MEASURED rather
  // than read off quantizeTime(): a material whose inputs really do change
  // every frame would re-bake every frame, which costs more than the replay
  // it replaced. Its stability rate never reaches the threshold.
  ClockedHost host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(timedLeaf(0.0f));
  for (int i = 0; i < 24; ++i) host.frame(1.0 / 60.0);
  const Composer::NodeCost* row = requireRow(host.composer, "plasma");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::Volatile);
}

TEST(ComposeCache, TemporalPromotionIsPixelIdenticalAcrossATick) {
  // Held to the same standard as the static case, over a window that
  // straddles the quantizer's step: 4 Hz at 60 FPS steps on frames 15, 30
  // and 45, so frames 24..40 cover a full hold, the tick, and the hold
  // after it. Every frame must match the unpromoted render exactly.
  ClockedHost promoted(400, 400), plain(400, 400);
  plain.composer.setAutoTexturePromotion(false);
  promoted.composer.render(timedLeaf(4.0f));
  plain.composer.render(timedLeaf(4.0f));
  for (int i = 0; i < 41; ++i) {
    promoted.frame(1.0 / 60.0);
    plain.frame(1.0 / 60.0);
    if (i < 24) continue;
    SkBitmap a = plain.grab(400, 400), b = promoted.grab(400, 400);
    size_t differing = 0;
    for (int y = 0; y < 400; ++y)
      for (int x = 0; x < 400; ++x)
        differing += a.getColor(x, y) != b.getColor(x, y);
    EXPECT_EQ(differing, 0u) << "frame " << i << ": " << differing
                             << " pixels changed under temporal promotion";
  }
}

namespace {

/** A stroked ring whose trim end follows a keyframe path with a HOLD:
 *  0 -> 0.6 over 200 ms, then flat until 600 ms, then on to 1. The flat
 *  stretch is the whole point — it is a running motion whose value is not
 *  changing, which is the case the volatility model could not express. */
Element gatedRing(Cache mode) {
  return box()
      .cache(Cache::None)
      .child(box()
                 .width(120)
                 .height(120)
                 .key("ring")
                 .cache(mode)
                 .shape(geometry::shapes::circle())
                 .stroke(stroke(6.0f, Fill::color({1, 1, 1, 1})))
                 .mask(by::spans(spans::upTo(
                     animate(sigil::motion::through(
                                 {{std::chrono::milliseconds(0), 0.0f},
                                  {std::chrono::milliseconds(200), 0.6f},
                                  {std::chrono::milliseconds(600), 0.6f},
                                  {std::chrono::milliseconds(800), 1.0f}}),
                             &choreograph::easeNone)))));
}

}  // namespace

TEST(ComposeCache, AHeldKeyframeSegmentDoesNotRepaint) {
  Host host;
  host.composer.render(gatedRing(Cache::Auto));
  host.frame();
  // Warm past the release: after enough stable paints the volatility flag
  // releases and the tree re-records ONCE, on the settling frame. The steady
  // state after that is the zero this test pins — measuring before the
  // release would count the settling record and prove nothing.
  for (int i = 0; i < 26; ++i)
    host.frame(1.0 / 60.0);  // t ~ 0.43 s: deep in the hold, post-release
  unsigned duringHold = 0;
  for (int i = 0; i < 8; ++i) {  // 0.43 -> 0.57 s, still flat
    host.frame(1.0 / 60.0);
    duringHold += host.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(duringHold, 0u)
      << "re-recorded " << duringHold
      << " times across a keyframe segment whose value never changed";
  unsigned afterHold = 0;
  for (int i = 0; i < 12; ++i) {  // 0.55 -> 0.75 s: moving again
    host.frame(1.0 / 60.0);
    afterHold += host.composer.stats().picturesRecorded;
  }
  EXPECT_GT(afterHold, 0u)
      << "the memo went stale-blind: the trim moved and nothing re-recorded";
}

TEST(ComposeCache, ScalarMemoIsPixelIdenticalAcrossEveryWaypoint) {
  // The constraint carried from the temporal-promotion work: every frame
  // identical, not just the held ones. A hold that goes stale by one frame
  // at a waypoint is exactly the bug this could introduce, and it would be
  // invisible in any single still.
  //
  // Cache::None on the node under test is the ground truth — it re-paints
  // from scratch every frame by construction — so the two hosts differ in
  // nothing but whether the memo is allowed to hold.
  Host memo, truth;
  memo.composer.render(gatedRing(Cache::Auto));
  truth.composer.render(gatedRing(Cache::None));
  memo.frame();
  truth.frame();
  for (int i = 0; i < 60; ++i) {  // a full second, over all three waypoints
    memo.frame(1.0 / 60.0);
    truth.frame(1.0 / 60.0);
    ASSERT_TRUE(identicalPixels(memo, truth, 200, 200))
        << "frame " << i << " (t = " << (double)i / 60.0
        << " s) differs from the uncached render";
  }
}
