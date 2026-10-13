// What a live value costs the cache: a quantized material is cacheable
// between its ticks, a continuous one stays live, and a held keyframe
// segment repaints nothing.

#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/ease/Ease.h>

#include "support/BrushTestSupport.h"

namespace {

Element timedLeaf(float quantizeHz, Cache caching = Cache::Auto) {
  material::Paint m = material::skia::sksl(heavyEffect(true));
  if (quantizeHz > 0) m.quantizeTime(quantizeHz);
  return box().children(
      {box().width(400).height(400).key("plasma").cache(caching).fill(
          material::skia::base(std::move(m)))});
}

/** How many recordings @p host made over @p frames frames of a sixtieth. */
size_t recordingsOver(Host& host, int frames) {
  size_t recorded = 0;
  for (int i = 0; i < frames; ++i) {
    host.frame(1.0 / 60.0);
    recorded += host.composer.stats().picturesRecorded;
  }
  return recorded;
}

}  // namespace

TEST(ComposeCache, AQuantizedMaterialIsRecordedAgainOnlyWhenItTicks) {
  // quantizeTime(4) at 60 FPS means the shader's inputs change four times a
  // second and the other 56 frames resolve to the SAME shader. Their pixels
  // are therefore identical to the last recording's — not similar,
  // identical — so the recording stands and nothing records again.
  Host host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(timedLeaf(4.0f));
  host.frame();
  const size_t recorded = recordingsOver(host, 24);
  const Composer::NodeCost* row = requireRow(host.composer, "plasma");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Picture);
  EXPECT_LE(recorded, 2u) << "a material stepping at 4 Hz recorded " << recorded
                          << " times in 24 frames";
}

TEST(ComposeCache, AContinuousMaterialIsRecordedEveryFrame) {
  // The other half of the same rule, and the reason it is OBSERVED rather
  // than read off quantizeTime(): a material whose inputs really do change
  // every frame resolves to a new shader every frame, and the recording
  // that baked the last one is stale.
  Host host(400, 400);
  host.composer.render(timedLeaf(0.0f));
  host.frame();
  EXPECT_GE(recordingsOver(host, 12), 12u);
}

TEST(ComposeCache, AQuantizedMaterialIsPixelIdenticalAcrossATick) {
  // Held to the same standard as the static case, over a window that
  // straddles the quantizer's step: 4 Hz at 60 FPS steps on frames 15, 30
  // and 45, so frames 24..40 cover a full hold, the tick, and the hold
  // after it. Every frame must match the uncached render exactly.
  Host held(400, 400), plain(400, 400);
  held.composer.render(timedLeaf(4.0f));
  plain.composer.render(timedLeaf(4.0f, Cache::None));
  for (int i = 0; i < 41; ++i) {
    held.frame(1.0 / 60.0);
    plain.frame(1.0 / 60.0);
    if (i < 24) continue;
    ASSERT_TRUE(identicalPixels(plain, held, 400, 400))
        << "frame " << i << " differs from the uncached render";
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
      .children({box()
                     .width(120)
                     .height(120)
                     .key("ring")
                     .cache(mode)
                     .shape(geometry::shapes::circle())
                     .stroke(stroke(6.0f, Fill::color({1, 1, 1, 1})))
                     .mask(by::spans(spans::upTo(motion::animate(
                         {.from = 0.0f,
                          .keyframes = {{.to = 0.6f, .duration = 200ms},
                                        {.to = 0.6f, .duration = 400ms},
                                        {.to = 1.0f, .duration = 200ms}},
                          .ease = sigil::motion::ease::linear}))))});
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
  // Every frame identical, not just the held ones. A hold that goes stale by
  // one frame at a waypoint is exactly the bug this could introduce, and it
  // would be invisible in any single still.
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
