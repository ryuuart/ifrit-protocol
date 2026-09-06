// Automatic texture promotion: what the promoter takes, what it refuses --
// a blend against the canvas, a backdrop filter, a clip, a rotation -- and
// that every refusal names its reason. Promotion changes no pixels.

#include "support/BrushTestSupport.h"

// ---------------------------------------------------------------------------
// Cache::Auto texture promotion — the library fixing a slow frame by itself.
TEST(ComposeCache, TheAutoPromotionSwitchChangesNoPixels) {
  // Automatic promotion may never change a pixel. A texture cache that
  // resolved at the wrong scale and softened a hairline would trade a speed
  // problem for a fidelity problem, and hairlines at 1x are most of what
  // this library draws. So promotion bakes in DEVICE space at an
  // integer-snapped rect and blits with the matrix reset: an integer device
  // translation cannot change rasterisation. This asserts that over the
  // whole canvas rather than trusting it.
  //
  // Note the limit of the claim: nothing here establishes that a node was
  // actually promoted. If promotion stopped firing entirely, this would
  // compare two unpromoted renders and pass. It pins the SWITCH, not the
  // mechanism — see the profiling cases for that.
  Host reference;
  reference.composer.setAutoTexturePromotion(false);
  reference.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i) reference.frame();
  const std::vector<SkColor> before = grab(reference);

  Host promoted;
  promoted.composer.setAutoTexturePromotion(true);
  promoted.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i) promoted.frame();
  const std::vector<SkColor> after = grab(promoted);

  ASSERT_EQ(before.size(), after.size());
  size_t differing = 0;
  for (size_t i = 0; i < before.size(); ++i) differing += before[i] != after[i];
  EXPECT_EQ(differing, 0u)
      << differing << " pixels changed when the library promoted a node";
}

TEST(ComposeCache, NoRowReportsPromotedWhilePromotionIsOff) {
  // A silent good outcome and a silent bad outcome look identical without an
  // instrument, so a promotion the library performs must be attributable to
  // the library. Whether this particular node trips the threshold depends on
  // the machine, so the assertion is about REPORTING, not about promoting:
  // no node may ever be labelled Promoted while promotion is switched off.
  Host host;
  host.composer.setAutoTexturePromotion(false);
  host.composer.setProfiling(true);
  host.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i) host.frame();
  bool sawPromoted = false, sawPicture = false;
  for (const auto& row : host.composer.profile()) {
    sawPromoted |= row.cacheState == Composer::CacheState::Promoted;
    sawPicture |= row.cacheState == Composer::CacheState::Picture;
  }
  EXPECT_FALSE(sawPromoted) << "promotion is off, yet a node reported Promoted";
  EXPECT_TRUE(sawPicture) << "nothing cached as a picture at all";
  // And cached() still answers the coarse question.
  for (const auto& row : host.composer.profile())
    EXPECT_EQ(row.cached(), row.cacheState != Composer::CacheState::Live);
}

TEST(ComposeCache, CachePictureOptsOutOfPromotion) {
  // The per-node switch: Cache::Picture means "record, and never promote".
  Host host;
  host.composer.setProfiling(true);
  // Cache::None on the wrapper, or the panel is recorded into its parent
  // once and never profiled again — and the loop below would then assert
  // nothing at all, which is how it read before this line existed.
  host.composer.render(
      profiledUnder(expensivePanel().cache(Cache::Picture).key("optout")));
  for (int i = 0; i < 30; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "optout");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::OptedOut);
  // The panel must actually be over the bar, or "not promoted" is true for
  // the wrong reason and this asserts nothing about the opt-out.
  EXPECT_GT(row->selfMs, 1.0)
      << "expensivePanel is under the promotion threshold, so Cache::Picture "
         "is not what kept it from being promoted";
}

namespace {

/** An expensive panel with one MULTIPLY child, over an opaque ground. The
 *  ground matters: multiply against mid-grey and multiply against
 *  transparent black differ enormously, so a wrongly-baked subtree is
 *  loud rather than subtle. */
Element blendingScene(SkBlendMode mode) {
  return profiledUnder(stack()
                           .child(box().absolute().inset(0).fill(
                               Fill::color({0.55f, 0.55f, 0.6f, 1})))
                           .child(expensivePanel().key("reader").child(
                               box()
                                   .absolute()
                                   .left(20)
                                   .top(20)
                                   .width(90)
                                   .height(90)
                                   .fill(Fill::color({0.9f, 0.5f, 0.2f, 1}))
                                   .blend(mode))));
}

}  // namespace

TEST(ComposeCache, PromotionRefusesASubtreeThatBlendsWithTheCanvas) {
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(blendingScene(SkBlendMode::kMultiply));
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "reader");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted)
      << "baked a subtree containing a kMultiply child — its blend would "
         "have resolved against transparent black";
  // Specifically ReadsBackdrop, not the Filtered bucket. Asserting the
  // bucket would have passed had the panel been refused for its clip, its
  // layer effect, or its own backdrop instead — four different causes, one
  // of which is the one under test. A guard that cannot tell them apart
  // does not guard the thing it is named for.
  EXPECT_EQ(row->promotion, Composer::Promotion::ReadsBackdrop);
}

TEST(ComposeCache, TheBlendingChildIsWhatCausesTheRefusal) {
  // THE POSITIVE CONTROL, and the reason the guards above can be trusted.
  //
  // A refusal test proves nothing on its own: a node refused for some
  // unrelated reason — too cheap, wrong transform, a stray clip — passes
  // "was not promoted" exactly as well as one refused for the reason under
  // test. So render the SAME scene with the child's blend set to kSrcOver,
  // which is the only difference, and require that it IS promoted.
  //
  // Together the two halves say: this scene is promotable, and the ONLY
  // thing standing between it and a bake is the child's blend mode. If a
  // future change made the refusal fire for everything, this fails; if it
  // made it fire for nothing, its sibling fails.
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(blendingScene(SkBlendMode::kSrcOver));
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "reader");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "the same scene with a srcOver child was not promoted either, so "
         "the refusal test above proves nothing about blending";
  EXPECT_EQ(row->promotion, Composer::Promotion::Promoted);
}

TEST(ComposeCache, ABlendingSubtreeKeepsItsPixelsUnderPromotion) {
  // The assertion that would catch a future relaxation of the rule. If the
  // subtree were ever baked, the multiply child would composite against a
  // transparent layer and these two renders would diverge by a lot.
  for (SkBlendMode mode :
       {SkBlendMode::kMultiply, SkBlendMode::kScreen, SkBlendMode::kPlus}) {
    Host on(220, 220), off(220, 220);
    off.composer.setAutoTexturePromotion(false);
    on.composer.render(blendingScene(mode));
    off.composer.render(blendingScene(mode));
    for (int i = 0; i < 24; ++i) {
      on.frame();
      off.frame();
    }
    EXPECT_TRUE(identicalPixels(off, on, 220, 220))
        << "promotion changed the pixels of a subtree that blends with the "
           "canvas (mode "
        << (int)mode << ")";
  }
}

TEST(ComposeCache, PromotionRefusesABackdropFilter) {
  // The other half of subtreeReadsBackdrop: a backdrop filter SAMPLES the
  // destination, so a bake would filter transparent black.
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(profiledUnder(
      stack()
          .child(box().absolute().inset(0).fill(
              Fill::color({0.55f, 0.55f, 0.6f, 1})))
          .child(expensivePanel().key("reader").child(
              box().absolute().left(20).top(20).width(90).height(90).backdrop(
                  material::skia::Effect::filter(
                      SkImageFilters::Blur(3, 3, nullptr)))))));
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "reader");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::ReadsBackdrop);
}

TEST(ComposeCache, AClipIsRefusedSeparatelyFromABlendingSubtree) {
  // The other side of the same split: a clip is the author's OWN node to
  // change, a blend can be three levels down and invisible to them. If
  // these two ever collapse back into one reason, this fails.
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(
      profiledUnder(expensivePanel().key("clipped").clip(true)));
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "clipped");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->promotion, Composer::Promotion::Filtered);
}

TEST(ComposeCache, PromotionRefusesEveryRotation) {
  // Promotion's envelope is upright, unmirrored and unskewed, and ±90° is
  // "axis aligned" in the loose sense that would have let it through. A
  // promoted node that resampled would be a fidelity bug bought with a
  // perf win, which is worse than the perf bug.
  for (float degrees : {90.0f, -90.0f, 180.0f, 45.0f}) {
    Host host(300, 300);
    host.composer.setProfiling(true);
    // Cache::None on the wrapper so the panel is painted every frame; under
    // a cacheable parent it would be recorded once and never profiled, and
    // every assertion below would pass without testing anything.
    host.composer.render(profiledUnder(
        expensivePanel().absolute().left(40).top(40).rotate(degrees).key(
            "turned")));
    for (int i = 0; i < 30; ++i) host.frame();
    const Composer::NodeCost* row = requireRow(host.composer, "turned");
    ASSERT_NE(row, nullptr);
    EXPECT_NE(row->cacheState, Composer::CacheState::Promoted)
        << "promoted a node at rotate(" << degrees << ")";
    EXPECT_EQ(row->promotion, Composer::Promotion::Transformed);
  }
}

namespace {

Element heavyLeaf(const char* key) {
  return profiledUnder(box().width(400).height(400).key(key).fill(
      material::skia::Paint::sksl(heavyEffect(false))));
}

}  // namespace

TEST(ComposeCache, PromotesAnExpensiveLeafAndKeepsEveryPixel) {
  Host promoted(400, 400);
  promoted.composer.setProfiling(true);
  promoted.composer.render(heavyLeaf("field"));
  for (int i = 0; i < 24; ++i) promoted.frame();
  const Composer::NodeCost* row = requireRow(promoted.composer, "field");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "a leaf costing " << row->selfMs
      << " ms per frame was never considered for a bake";
  EXPECT_EQ(row->promotion, Composer::Promotion::Promoted);

  // …and it is the same picture. That is the binding constraint: the output
  // is hairlines at 1x, and a bake resolved anywhere but on the device grid
  // trades a speed problem for a fidelity one.
  Host plain(400, 400);
  plain.composer.setAutoTexturePromotion(false);
  plain.composer.render(heavyLeaf("field"));
  for (int i = 0; i < 24; ++i) plain.frame();
  EXPECT_TRUE(identicalPixels(plain, promoted, 400, 400))
      << "promoting the leaf changed its pixels";
}

TEST(ComposeCache, ARefusalNamesTheReasonItRefused) {
  // Every refusal is individually correct and individually invisible, so an
  // author looking at a node that is painting live and slow has nothing to
  // act on unless the refusal names itself.
  //
  // Opacity is the honest refusal: compositing a bake applies the alpha to
  // an already-rounded 8-bit colour, where a direct draw applies it to the
  // shader's float output. The two agree to within one least-significant
  // bit, which is not agreement. Cache::Texture is how an author says they
  // accept that trade.
  Host host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(
      profiledUnder(box()
                        .width(400)
                        .height(400)
                        .key("wash")
                        .fill(material::skia::Paint::sksl(heavyEffect(false)))
                        .opacity(0.4f)));
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "wash");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::Composited);
  EXPECT_STRNE(Composer::promotionReason(row->promotion), "");
}
