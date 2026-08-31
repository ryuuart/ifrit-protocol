// The paint binary's share of ComposeTestMask.cpp: the suites whose subjects
// are paint-tier values, cut from that file so each test binary links only the
// target it exercises.

#include "support/PaintTestSupport.h"

namespace {

/** The profile row for the node keyed `key`, from the last draw (labels
 *  are "<key> (<kind> WxH)"). */
const Composer::NodeCost* rowOf(Host& host, const char* key) {
  const std::string prefix = std::string(key) + " (";
  for (const Composer::NodeCost& row : host.composer.profile())
    if (row.label.rfind(prefix, 0) == 0) return &row;
  return nullptr;
}

/** A 16 px tile, left half red / right half green — a half-tile pan flips
 *  which colour sits at any given pixel, so phase is binary-assertable. */
Pattern halfTilePattern() {
  return Pattern::tile({16, 16}, [](SkCanvas& c, SkSize s, uint32_t) {
    SkPaint left;
    left.setColor4f({1, 0, 0, 1}, nullptr);
    c.drawRect(SkRect::MakeWH(s.width() * 0.5f, s.height()), left);
    SkPaint right;
    right.setColor4f({0, 1, 0, 1}, nullptr);
    c.drawRect(
        SkRect::MakeXYWH(s.width() * 0.5f, 0, s.width() * 0.5f, s.height()),
        right);
  });
}

/** The same panel as the bound-fill cases, with the accent's bound FILL
 *  swapped for a bound PAN: root (asked-for bake) → frame → row of stroked
 *  star cells, plus ONE cell filled with `pat.material()`. The Pattern is
 *  passed in by reference because its baked tile IS its identity — the test
 *  holds it like an asset rather than re-minting it. */
Element pannedPanel(Pattern& pat) {
  auto row = box().key("row").row().wrapLines().gap(2);
  for (int id = 0; id < 12; ++id)
    row.child(box()
                  .width(26)
                  .height(26)
                  .shape(shapes::star(5 + id % 3, 0.45f, 0.08f))
                  .fill(blue())
                  .stroke(stroke(1.5f, green())));
  row.child(box().key("accent").width(26).height(26).fill(pat.material()));
  return box()
      .key("root")
      .cache(Cache::Texture)
      .column()
      .padding(6)
      .child(box().key("frame").column().padding(4).child(std::move(row)));
}

}  // namespace

TEST(ComposePatternPan, ABoundPanMovesThePatternWithNoRedescribe) {
  // Pin (a): assign the Output, the repeat moves — per frame, two frames
  // pixel-asserted, and render() is never called again after the first
  // describe (the whole point of the bound form).
  choreograph::Output<float> panX{0.0f};
  Pattern pat = halfTilePattern();
  pat.sampling(SkSamplingOptions(SkFilterMode::kNearest))
      .offset(&panX, nullptr);
  Host host(300, 300);
  host.composer.render(pannedPanel(pat));
  host.frame();
  const auto accent = host.composer.bounds("accent");
  ASSERT_TRUE(accent);
  // Node-local x = 3 samples source x = (3 - pan) mod 16: red at pan 0.
  const int px = (int)accent->left() + 3, py = (int)accent->centerY();
  EXPECT_EQ(host.pixel(px, py), SK_ColorRED);
  panX = 8.0f;  // half a tile
  host.frame(0.016);
  EXPECT_EQ(host.pixel(px, py), SK_ColorGREEN)
      << "frame one: the bound pan did not move the repeat";
  panX = 16.0f;  // a full tile — back to phase 0
  host.frame(0.016);
  EXPECT_EQ(host.pixel(px, py), SK_ColorRED)
      << "frame two: the bound pan did not keep moving";
}

TEST(ComposePatternPan, ASettledBoundPanReleasesVolatilityAndPromotes) {
  // The release has to show against Promotion::Volatile's `contentStable`,
  // not only against the recording: promotion is a SEPARATE consumer of
  // `subtreeVolatile`, so a parked pan that kept its recording but was still
  // denied its bake would keep all of the cost.
  choreograph::Output<float> panX{0.0f};
  Pattern pat = halfTilePattern();
  pat.sampling(SkSamplingOptions(SkFilterMode::kNearest))
      .offset(&panX, nullptr);
  Host host(300, 300);
  host.composer.render(pannedPanel(pat));
  host.composer.setProfiling(true);
  host.frame();
  const auto accent = host.composer.bounds("accent");
  ASSERT_TRUE(accent);
  const int px = (int)accent->left() + 3, py = (int)accent->centerY();
  EXPECT_EQ(host.pixel(px, py), SK_ColorRED);
  // BEFORE the settle: a fresh bound pan denies contentStable at the root.
  {
    const Composer::NodeCost* root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "a fresh bound pan must declare volatility";
    EXPECT_EQ(host.composer.stats().texturesLive, 0u)
        << "the root's asked-for bake must be refused while volatile";
  }
  for (int i = 0; i < 12; ++i)  // kScalarSettleFrames = 8, plus the release
    host.frame(0.016);          // walk and the settling frame's re-record
  // AFTER: released — the node promotes like a static pattern.
  {
    const Composer::NodeCost* root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_FALSE(root->refused(Composer::Promotion::Volatile))
        << "a settled bound pan still denies contentStable — no release";
    EXPECT_GE(host.composer.stats().texturesLive, 1u)
        << "the released root never took its bake — promotion still denied";
  }
  // …and the hold costs NOTHING.
  unsigned settledRecords = 0, settledPaints = 0;
  for (int i = 0; i < 4; ++i) {
    host.frame(0.016);
    settledRecords += host.composer.stats().picturesRecorded;
    settledPaints += host.composer.stats().nodesPainted;
  }
  EXPECT_EQ(settledRecords, 0u) << "a settled bound pan re-recorded";
  EXPECT_EQ(settledPaints, 0u) << "a settled bound pan painted live";
  EXPECT_EQ(host.pixel(px, py), SK_ColorRED);

  // Pin (c), move-after-settle: the frame the pan resumes shows the NEW
  // phase — no stale frame, ever — and volatility re-declares.
  panX = 8.0f;
  host.frame(0.016);
  EXPECT_EQ(host.pixel(px, py), SK_ColorGREEN)
      << "the resumed pan's frame showed the parked phase";
  {
    const Composer::NodeCost* root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "the resumed pan must re-declare volatility the same frame";
  }
  // …and the cycle closes: it settles AGAIN and re-releases.
  for (int i = 0; i < 12; ++i) host.frame(0.016);
  const Composer::NodeCost* root = rowOf(host, "root");
  ASSERT_TRUE(root);
  EXPECT_FALSE(root->refused(Composer::Promotion::Volatile))
      << "a re-settled bound pan did not re-release";
  EXPECT_EQ(host.pixel(px, py), SK_ColorGREEN);
}

TEST(ComposePatternPan, AMovingBoundPanNeverReleases) {
  // The release must NOT fire for a pan that IS moving — a conveyor driven
  // every frame keeps full volatility, keeps the refusal, and keeps the
  // true phase on screen.
  choreograph::Output<float> panX{0.0f};
  Pattern pat = halfTilePattern();
  pat.sampling(SkSamplingOptions(SkFilterMode::kNearest))
      .offset(&panX, nullptr);
  Host host(300, 300);
  host.composer.render(pannedPanel(pat));
  host.composer.setProfiling(true);
  host.frame();
  const auto accent = host.composer.bounds("accent");
  ASSERT_TRUE(accent);
  const int px = (int)accent->left() + 3, py = (int)accent->centerY();
  for (int i = 0; i < 20; ++i) {
    panX = (float)((i % 4) + 1);  // moves every frame, never twice the same
    host.frame(0.016);
    const Composer::NodeCost* root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "a driven bound pan released its volatility at frame " << i;
    EXPECT_EQ(host.composer.stats().texturesLive, 0u)
        << "a bake was held across a driven bound pan at frame " << i;
  }
  panX = 8.0f;
  host.frame(0.016);
  EXPECT_EQ(host.pixel(px, py), SK_ColorGREEN)
      << "the last written pan is not on screen";
}

TEST(ComposePatternPan, AnUnboundOffsetStaysDescribeTimeAndPrunes) {
  // Pin (d), the control arm: the static offset() is UNTOUCHED by the
  // bound channel — still describe-time, still the static fill path (no
  // live slot, no volatility), and an identical re-describe still prunes.
  Pattern pat = halfTilePattern();
  pat.sampling(SkSamplingOptions(SkFilterMode::kNearest))
      .offset(SkPoint{8.0f, 0.0f});
  EXPECT_FALSE(pat.material().isAnimated())
      << "a static pan must not route to the live material slot";
  choreograph::Output<float> panX{0.0f};
  Pattern bound = pat;
  bound.offset(&panX, nullptr);
  EXPECT_TRUE(bound.material().isAnimated())
      << "the bound form must route live";
  EXPECT_FALSE(pat.material().isAnimated())
      << "binding a COPY must not contaminate the original (value law)";
  // The static pan draws at its phase and prunes across re-describes.
  Host host(300, 300);
  host.composer.render(pannedPanel(pat));
  host.frame();
  const auto accent = host.composer.bounds("accent");
  ASSERT_TRUE(accent);
  const int px = (int)accent->left() + 3, py = (int)accent->centerY();
  EXPECT_EQ(host.pixel(px, py), SK_ColorGREEN);  // 8 px static pan: flipped
  host.composer.render(pannedPanel(pat));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical static-pan re-describe did not prune";
  EXPECT_EQ(host.pixel(px, py), SK_ColorGREEN);
}

TEST(ComposePatternPan, ThePanBindingIsRecipe) {
  // The BINDING participates in the prune signature: the same binding
  // prunes, a different one patches. A pruned swap would leave the OLD
  // Output driving the pixels for as long as the node lives.
  choreograph::Output<float> a{0.0f}, b{0.0f};
  Pattern pat = halfTilePattern();
  Pattern p1 = pat, p2 = pat, p3 = pat;
  p1.offset(&a, nullptr);
  p2.offset(&b, nullptr);
  p3.offset(&a, nullptr);
  EXPECT_TRUE(p1.material() == p3.material()) << "same recipe, same binding";
  EXPECT_FALSE(p1.material() == p2.material()) << "a rebound pan must patch";
  EXPECT_FALSE(p1.material() == pat.material()) << "bound differs from unbound";
  // …and through the reconciler: rebinding patches, an identical
  // re-describe does not.
  Host host(300, 300);
  host.composer.render(pannedPanel(p1));
  host.frame();
  host.composer.render(pannedPanel(p2));
  host.frame();
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "a rebound pan pruned — the old Output would drive forever";
  host.composer.render(pannedPanel(p2));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical bound re-describe did not prune";
}
