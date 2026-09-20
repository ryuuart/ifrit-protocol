// What a bound value's volatility does to the cache: a fill that settles
// releases it and the subtree promotes, one that keeps moving never
// does, and a fill or an effect still moving under a held gate repaints
// the gate with it.

#include <utility>

#include "support/BrushTestSupport.h"

namespace {

/** Root → frame → a row of stroked, shaped cells, plus ONE accent whose
 *  fill is bound. The accent's ANCESTORS are what a badly-classified
 *  binding poisons, which is why the accent is buried rather than at the
 *  root. `Cache::Texture` makes the outcome observable deterministically:
 *  an explicitly asked-for bake obeys the same volatility gate automatic
 *  promotion does, without the cost-threshold timing an assertion could
 *  flap on. */
Element settledFillPanel(const choreograph::Output<Fill>* tint) {
  auto row = box().key("row").row().flexWrap().gap(2);
  for (int id = 0; id < 12; ++id)
    row.children({box()
                      .width(26)
                      .height(26)
                      .shape(geometry::shapes::star(5 + id % 3, 0.45f, 0.08f))
                      .fill(blue())
                      .stroke(stroke(1.5f, green()))});
  row.children({box().key("accent").width(26).height(26).fill(
      motion::Animatable<Fill>(tint))});
  return box()
      .key("root")
      .cache(Cache::Texture)
      .column()
      .padding(6)
      .children(
          {box().key("frame").column().padding(4).children({std::move(row)})});
}

}  // namespace

TEST(ComposeSettledFill, ASettledBoundFillReleasesVolatilityAndPromotes) {
  // The release must show against Promotion::Volatile's
  // `contentStable`, because promotion is a SEPARATE consumer of
  // `subtreeVolatile` from the memo — keeping the recording while still
  // refusing the bake would keep all of the 5 ms.
  choreograph::Output<Fill> tint{red()};
  Host host(300, 300);
  host.composer.render(settledFillPanel(&tint));
  host.composer.setProfiling(true);
  host.frame();
  const auto accentRect = host.composer.bounds("accent");
  ASSERT_TRUE(accentRect);
  const int ax = (int)accentRect->centerX(), ay = (int)accentRect->centerY();
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorRED);
  // BEFORE the settle: the bound fill denies contentStable at the root —
  // the asked-for texture is refused and the whole chain paints live.
  {
    const Composer::NodeCost* root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "a fresh bound fill must declare volatility";
    EXPECT_EQ(host.composer.stats().texturesLive, 0u)
        << "the root's asked-for bake must be refused while volatile";
  }
  for (int i = 0; i < 12; ++i)  // past the settle count, the release walk,
    host.frame(0.016);          // and the settling frame's re-record
  // AFTER: released — the node promotes like a plain one.
  {
    const Composer::NodeCost* root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_FALSE(root->refused(Composer::Promotion::Volatile))
        << "a settled bound fill still denies contentStable — no release";
    EXPECT_GE(host.composer.stats().texturesLive, 1u)
        << "the released root never took its bake — promotion still denied";
  }
  // …and the hold costs NOTHING, which is the whole point of releasing.
  unsigned settledRecords = 0, settledPaints = 0;
  for (int i = 0; i < 4; ++i) {
    host.frame(0.016);
    settledRecords += host.composer.stats().picturesRecorded;
    settledPaints += host.composer.stats().nodesPainted;
  }
  EXPECT_EQ(settledRecords, 0u) << "a settled bound fill re-recorded";
  EXPECT_EQ(settledPaints, 0u) << "a settled bound fill painted live";
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorRED);

  // THE STALENESS CONTROL. On the frame the Output moves again, the scan
  // must re-declare volatility and stale every recording AND the root's
  // bake BEFORE anything paints. One frame, and never a stale pixel.
  tint = green();
  host.frame(0.016);
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorGREEN)
      << "the moved fill's frame showed a stale colour";
  {
    const Composer::NodeCost* root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "the moved fill must re-declare volatility the same frame";
  }
  // …and the cycle closes: it settles AGAIN and re-releases.
  for (int i = 0; i < 12; ++i) host.frame(0.016);
  const Composer::NodeCost* root = rowOf(host, "root");
  ASSERT_TRUE(root);
  EXPECT_FALSE(root->refused(Composer::Promotion::Volatile))
      << "a re-settled bound fill did not re-release";
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorGREEN);
}

TEST(ComposeSettledFill, AMovingBoundFillNeverReleases) {
  // The release must NOT fire for a fill that IS moving — a
  // binding driven every frame keeps full volatility, keeps the refusal,
  // and keeps painting the true colour.
  choreograph::Output<Fill> tint{red()};
  Host host(300, 300);
  host.composer.render(settledFillPanel(&tint));
  host.composer.setProfiling(true);
  host.frame();
  const auto accentRect = host.composer.bounds("accent");
  ASSERT_TRUE(accentRect);
  const int ax = (int)accentRect->centerX(), ay = (int)accentRect->centerY();
  for (int i = 0; i < 20; ++i) {
    const float t = (float)(i % 10) / 10.0f;
    tint = Fill::color({1.0f - t, 0.0f, t, 1.0f});
    host.frame(0.016);
    const Composer::NodeCost* root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "a driven bound fill released its volatility at frame " << i;
    EXPECT_EQ(host.composer.stats().texturesLive, 0u)
        << "a bake was held across a driven bound fill at frame " << i;
  }
  // The last written colour is on screen, not a settled ancestor's bake.
  tint = green();
  host.frame(0.016);
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorGREEN);
}

// ---- the memo carve-outs and the lanes they must not forget ---------------
//
// The content-volatility terms are enumerated in several places, and every
// copy has to name every term: a carve-out that omits a bound fill or a
// live effect lets a node carrying one of those AND an animated gate take a
// memo it has no right to.

TEST(ComposeCache, ABoundFillMovingUnderAHeldGateRepaints) {
  // A node carrying BOTH a bound fill() and an animated mask gate. The
  // scalar memo holds the recording while the GATE's floats hold still — and
  // that recording baked the fill colour into it. If the bound fill is not
  // part of the memo's comparison, moving it while the gate holds replays
  // the old colour.
  choreograph::Output<float> reveal{1.0f};
  choreograph::Output<Fill> tint{Fill::color({1, 0, 0, 1})};  // red
  Host host(200, 200);
  host.composer.render(box().children(
      {revealBox().fill(&tint).mask(by::spans(spans::upTo(&reveal)))}));
  host.frame();
  for (int i = 0; i < 4; ++i) host.frame(0.016);  // let the memo bake and hold
  EXPECT_GT(redInk(host, 25, 25, 115, 115), 4000) << "red to begin with";
  // The counts below describe the memo actually working: the recording,
  // which baked the red, REPLAYS while both the fill and the gate provably
  // hold, so only the parent paints live. They are the cheap half of the
  // claim. The pixel assertion after them is the one that matters — the
  // memo's comparison must see the fill MOVE, or the old colour replays.
  unsigned records = 0, live = 0;
  for (int i = 0; i < 4; ++i) {
    host.frame(0.016);
    records += host.composer.stats().picturesRecorded;
    live += host.composer.stats().nodesPainted;
  }
  EXPECT_EQ(records, 0u) << "the memoized recording holds while both hold";
  EXPECT_EQ(live, 4u) << "the parent paints live; the node replays its memo";
  tint = Fill::color({0, 0, 1, 1});  // …now turn it blue, gate unmoved
  host.frame(0.016);
  EXPECT_LT(redInk(host, 25, 25, 115, 115), 100)
      << "the bound fill moved and the node replayed a stale recording";
}

TEST(ComposeCache, ALiveEffectMovingUnderAHeldGateRepaints) {
  // The same hazard, a different lane: a LIVE effect driven by a bound
  // uniform is captured by the recording, so the scalar memo's refusal list
  // has to mention it or a held gate replays the effect's old output.
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader content; uniform float amt;"
                 "half4 main(float2 p) { half4 c = content.eval(p);"
                 "  return half4(c.r * half(amt), c.g, c.b, c.a); }"));
    if (!e) ADD_FAILURE() << err.c_str();
    return e;
  }();
  choreograph::Output<float> reveal{1.0f};
  choreograph::Output<float> amt{1.0f};
  Host host(200, 200);
  host.composer.render(box().children(
      {revealBox()
           .fill(Fill::color({1, 0, 0, 1}))
           .filter(material::skia::Effect::shader(fx, {{"amt", 1.0f}})
                       .uniform("amt", &amt))
           .mask(by::spans(spans::upTo(&reveal)))}));
  host.frame();
  for (int i = 0; i < 4; ++i) host.frame(0.016);
  EXPECT_GT(redInk(host, 25, 25, 115, 115), 4000) << "red to begin with";
  amt = 0.0f;  // the effect must now kill the red channel
  host.frame(0.016);
  EXPECT_LT(redInk(host, 25, 25, 115, 115), 100)
      << "the live effect moved and the node replayed a stale recording";
}
