// The split bake: an expensive own paint kept while a moving child repaints
// beside it, what a cached leaf keeps of its bleed and its settled opacity,
// and a refusal that names every reason at once.

#include <utility>

#include "support/BrushTestSupport.h"

TEST(ComposeCache, ConnectorWireSurvivesParentCaching) {
  // A connector's routed path is NOT bounded by the connector node's layout
  // rect — it reaches across to the endpoints it joins. The parent's
  // recording cull has to account for that, or the wire is drawn on the
  // first frame and clipped away by every cached replay after it.
  Host host;
  host.composer.render(
      box()
          .child(box().absolute().inset(20, 90, 160, 90).fill(red()).key("a"))
          .child(box().absolute().inset(160, 90, 20, 90).fill(red()).key("b"))
          .child(connector("a", "b").stroke(stroke(4, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN);  // the wire, mid-span
  host.frame();                                    // cached replay
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN);
}

TEST(ComposeCache, TextureBakeKeepsBleedAndOverflow) {
  // A texture bake must cover the node's PAINT bounds, not its box: a
  // decoration that bleeds outside the box (here a shadow offset well past
  // it) is silently cropped away by a bake sized to the node.
  Host host;
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(70, 70, 70, 70)
                      .cache(Cache::Texture)
                      .background(Shadow{{0, 1, 0, 1}, {30, 0}, 0})
                      .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(140, 100), SK_ColorGREEN);  // shadow past the box
}

TEST(ComposeCache, SettledOpacityRebakesTheLeaf) {
  // When an opacity transition settles, the leaf's recording must be re-baked:
  // the recording made while opacity was 1 folded the fill straight into the
  // draw, and replaying it under the settled 0.4 would show the leaf at full
  // strength. The second frame() draws only from caches, which is where a
  // missed re-bake shows up.
  Host host;
  auto tree = [](motion::Animatable<float> op) {
    return box().child(box()
                           .width(80)
                           .height(80)
                           .fill(Fill::color({1, 0, 0, 1}))
                           .opacity(std::move(op)));
  };
  host.composer.render(tree(1.0f));
  host.frame();
  host.composer.render(
      tree(animate(sigil::motion::to(0.4f),
                   {std::chrono::milliseconds(100), &choreograph::easeNone})));
  host.frame(0.5);  // settled at 0.4
  host.frame();     // draw again from caches
  const SkColor c = host.pixel(40, 40);
  EXPECT_NEAR(SkColorGetR(c), 102, 12);  // 0.4 · 255 on black
}

namespace {

/** THE SPLIT-BAKE FIXTURE, and every part of it is load-bearing.
 *
 *  The own paint deliberately OVERLAPS ITSELF — a background stroke under a
 *  runtime shader under an overlay stroke, plus a foreground stroke — so
 *  that compositing genuinely happens INSIDE the baked layer. That is the
 *  one real risk in the pixel argument: srcOver is associative in exact
 *  arithmetic, and a layer quantizes its intermediate to 8 bits where a
 *  direct draw quantizes a different intermediate. A fixture whose own
 *  paint is a single draw could not have found a discrepancy if there were
 *  one.
 *
 *  The foreground matters for a second reason: foregrounds paint AFTER the
 *  children, so a bake that swallowed them would draw them UNDER the child
 *  and this test would see it.
 *
 *  The child rides a bound Output, which is what makes the node volatile and
 *  is the ordinary shape of the problem: a small element driven by a loop
 *  over a large static backdrop. `clipped` and `childBlend` are
 *  parameterised because both are conditions whole-subtree promotion refuses
 *  outright, and the split must accept them. */
choreograph::Output<float>& splitSweep() {
  static choreograph::Output<float> sweep{0.0f};
  return sweep;
}

Element splitPlane(bool clipped, SkBlendMode childBlend) {
  Element plane =
      box()
          .key("plane")
          .absolute()
          .left(0)
          .top(0)
          .width(200)
          .height(200)
          .background(stroke(6.0f, Fill::color({0.2f, 0.4f, 0.9f, 0.6f})))
          .fill(material::skia::Paint::sksl(sharedHeavyEffect()))
          .overlay(stroke(3.0f, Fill::color({1.0f, 0.9f, 0.2f, 0.45f})))
          .foreground(stroke(1.5f, Fill::color({1, 1, 1, 0.5f})));
  if (clipped) plane.clip(true);
  plane.child(box()
                  .absolute()
                  .left(10)
                  .top(80)
                  .width(50)
                  .height(50)
                  .fill(Fill::color({1.0f, 0.35f, 0.1f, 0.85f}))
                  .blend(childBlend)
                  .translateX(motion::bind(&splitSweep()).scale(130.0f)));
  return profiledUnder(std::move(plane));
}

/** The worst frame-to-frame divergence over the child's WHOLE traverse.
 *
 *  Not one still: a split that is exact where the child happens to sit and
 *  wrong where it crosses the bake's own overlay would pass a single
 *  capture. The sweep is what makes the claim about the mechanism rather
 *  than about one position. */
size_t worstSplitDivergence(bool clipped, SkBlendMode childBlend) {
  Host on(200, 200), off(200, 200);
  off.composer.setAutoTexturePromotion(false);
  size_t worst = 0;
  for (int i = 0; i < 32; ++i) {
    splitSweep() = (float)i / 32.0f;
    on.composer.render(splitPlane(clipped, childBlend));
    off.composer.render(splitPlane(clipped, childBlend));
    on.frame();
    off.frame();
    if (!identicalPixels(off, on, 200, 200)) {
      SkBitmap a, b;
      a.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
      b.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
      off.surface->readPixels(a.pixmap(), 0, 0);
      on.surface->readPixels(b.pixmap(), 0, 0);
      size_t differing = 0;
      for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 200; ++x)
          differing += a.getColor(x, y) != b.getColor(x, y);
      worst = std::max(worst, differing);
    }
  }
  return worst;
}

}  // namespace

TEST(ComposeCache, SplitsAnExpensiveOwnPaintFromItsMovingChild) {
  Host host(200, 200);
  host.composer.setProfiling(true);
  for (int i = 0; i < 24; ++i) {
    splitSweep() = (float)i / 24.0f;
    host.composer.render(splitPlane(false, SkBlendMode::kSrcOver));
    host.frame();
  }
  const Composer::NodeCost* row = requireRow(host.composer, "plane");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::SplitOwn)
      << "a static own paint costing " << row->selfMs
      << " ms per frame was re-rasterized to redraw a moving child over it";
  EXPECT_EQ(row->promotion, Composer::Promotion::SplitBaked);
  // And the node really is refused by the ordinary promoter, or "split" is
  // true for the wrong reason: a node the promoter would have baked whole
  // proves nothing about splitting.
  EXPECT_TRUE(row->refused(Composer::Promotion::Volatile))
      << "this node is not volatile, so nothing above tested the split";
}

TEST(ComposeCache, SplitBakeIsPixelIdenticalAcrossTheChildsMotion) {
  // The exactness constraint, which is strictly stronger than whole-subtree
  // promotion's. Promotion only has to argue that an integer device
  // translation cannot change rasterisation. The split replaces PART of what
  // a node paints and then draws the children over the blit, so it has to
  // argue about compositing:
  //
  //   painting the own layer into a transparent device-aligned surface,
  //   blitting it, then painting the children over the result must produce
  //   the same pixels as painting own-then-children directly.
  //
  // srcOver is associative, so that holds in exact arithmetic. What is NOT
  // free is the 8-bit rounding of the intermediate, and this fixture's own
  // paint overlaps itself specifically so that the intermediate exists.
  EXPECT_EQ(worstSplitDivergence(false, SkBlendMode::kSrcOver), 0u)
      << "splitting the own paint from the children changed pixels";
}

TEST(ComposeCache, ABlendingChildIsFineUnderTheSplitAndFatalUnderPromotion) {
  // The one place the split is SAFER than whole-subtree promotion, and it
  // is worth an assertion rather than an argument because it inverts a rule
  // three other tests in this file enforce.
  //
  // Under promotion a non-srcOver child is fatal: it sits INSIDE the bake
  // and resolves against transparent black. Under the split the blit lands
  // BEFORE the children, so the child resolves against exactly the
  // destination bytes it would have found anyway. Same for a child with a
  // backdrop filter.
  for (SkBlendMode mode :
       {SkBlendMode::kMultiply, SkBlendMode::kScreen, SkBlendMode::kPlus}) {
    EXPECT_EQ(worstSplitDivergence(false, mode), 0u)
        << "a " << (int)mode << "-blended child diverged under the split";
  }
}

TEST(ComposeCache, TheSplitSurvivesTheClipThatMadeTheChildAChild) {
  // clipContent looks like it belongs beside layer effects in the split's
  // exclusion list — both appear to "wrap both halves" — but only the layer
  // effect actually does. A filter applies to the UNION of own paint and
  // children, so filtering the own half alone is a different picture. A clip
  // is opened and closed INSIDE each phase (the phase flag skips only the
  // content), so both halves get the identical clip in identical device
  // geometry.
  //
  // Getting this wrong is not a minor over-refusal. A clip is very often
  // exactly WHY a moving element is a child of the plane rather than its
  // sibling — the plane clips it to a silhouette — so excluding clips would
  // refuse the split precisely where it is most wanted, and every other case
  // in this section would still pass.
  Host host(200, 200);
  host.composer.setProfiling(true);
  for (int i = 0; i < 24; ++i) {
    splitSweep() = (float)i / 24.0f;
    host.composer.render(splitPlane(true, SkBlendMode::kSrcOver));
    host.frame();
  }
  const Composer::NodeCost* row = requireRow(host.composer, "plane");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::SplitOwn)
      << "a clipped node was refused the split bake";
  EXPECT_EQ(worstSplitDivergence(true, SkBlendMode::kMultiply), 0u)
      << "the clip was not reproduced identically in both phases";
}

TEST(ComposeCache, ItIsTheVolatileChildThatSplitsTheBake) {
  // THE POSITIVE CONTROL. "It was split" proves nothing on its own: a node
  // split for some unrelated reason passes exactly as well as one split for
  // the reason under test. So render the SAME scene with the child's
  // binding replaced by the constant it would have held — the only
  // difference — and require that the node is promoted WHOLE instead.
  //
  // Together the two halves say: this node's own paint is bakeable, and the
  // only thing deciding between a whole bake and a split one is whether the
  // child moves. A change that split everything fails this; one that split
  // nothing fails its sibling.
  const auto still = [] {
    return profiledUnder(
        box()
            .key("plane")
            .absolute()
            .left(0)
            .top(0)
            .width(200)
            .height(200)
            .background(stroke(6.0f, Fill::color({0.2f, 0.4f, 0.9f, 0.6f})))
            .fill(material::skia::Paint::sksl(sharedHeavyEffect()))
            .overlay(stroke(3.0f, Fill::color({1.0f, 0.9f, 0.2f, 0.45f})))
            .foreground(stroke(1.5f, Fill::color({1, 1, 1, 0.5f})))
            .child(box()
                       .absolute()
                       .left(10)
                       .top(80)
                       .width(50)
                       .height(50)
                       .fill(Fill::color({1.0f, 0.35f, 0.1f, 0.85f}))
                       .translateX(65.0f)));
  };
  Host host(200, 200);
  host.composer.setProfiling(true);
  host.composer.render(still());
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "plane");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "the same node with a STILL child was not promoted whole either, "
         "so the split tests above prove nothing about the child's motion";
  EXPECT_FALSE(row->refused(Composer::Promotion::Volatile));
}

TEST(ComposeCache, ARefusalNamesEveryReasonAndNotJustTheFirst) {
  // `promotion` is a FIRST-MATCH verdict, so a node that is both volatile
  // and clipped reports only Volatile — and an author who removes the
  // volatility then meets a second refusal nobody mentioned. The `refusals`
  // mask carries all of them at once; `promotion` stays the primary outcome,
  // so every assertion elsewhere that reads it still means what it meant.
  //
  // The tree here is refused three ways at once: a bound child (Volatile),
  // a rotation (Transformed) and clip(true) (Filtered). Composited is
  // deliberately NOT in the set — opacity only reaches that refusal through
  // the childless-leaf fast path, which a clipped node with children can
  // never take, so including it would make this test unfalsifiable.
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(
      profiledUnder(expensivePanel().key("many").clip(true).rotate(30.0f).child(
          box()
              .absolute()
              .left(4)
              .top(4)
              .width(20)
              .height(20)
              .fill(red())
              .translateX(motion::bind(&splitSweep()).scale(40.0f)))));
  for (int i = 0; i < 24; ++i) {
    splitSweep() = (float)i / 24.0f;
    host.frame();
  }
  const Composer::NodeCost* row = requireRow(host.composer, "many");
  ASSERT_NE(row, nullptr);
  EXPECT_TRUE(row->refused(Composer::Promotion::Volatile));
  EXPECT_TRUE(row->refused(Composer::Promotion::Transformed));
  EXPECT_TRUE(row->refused(Composer::Promotion::Filtered));
  // …and the primary verdict is exactly the first of them in the documented
  // order. The two can never disagree because the verdict is DERIVED from
  // the mask rather than computed alongside it.
  EXPECT_EQ(row->promotion, Composer::Promotion::Volatile);
  // A node with nothing wrong with it must report an EMPTY mask, or
  // "refused(X) is true" above is true of everything and tests nothing.
  Host clean(220, 220);
  clean.composer.setProfiling(true);
  clean.composer.render(profiledUnder(expensivePanel().key("clean")));
  for (int i = 0; i < 24; ++i) clean.frame();
  const Composer::NodeCost* ok = requireRow(clean.composer, "clean");
  ASSERT_NE(ok, nullptr);
  EXPECT_EQ(ok->refusals, 0u);
}
