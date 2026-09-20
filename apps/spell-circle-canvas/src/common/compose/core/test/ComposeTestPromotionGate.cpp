// When the promoter takes a node and when it lets it go: what an eager
// composer promotes on its first frame, what it refuses because the cost
// rule refuses it, the node inside a bake that is not baked again, and
// the two things that send a promoted node back for a rebake — the clip
// that cut it opening, and its matrix moving inside its own rect.

#include <vector>

#include "support/PromotionTestSupport.h"

namespace {

/** Four leaves under a parent that caches nothing, so every one of them is
 *  visited — and profiled — on every frame. One is promotable and the
 *  other three each carry exactly one of the conditions under which a bake
 *  would paint different pixels, so the refusal a row reports names the
 *  thing the leaf was built to carry. Nothing here is expensive: the point
 *  is a page the COST rule would never promote. */
Element eagerPage() {
  return box()
      .cache(Cache::None)
      .children({box()
                     .key("plain")
                     .absolute()
                     .left(0)
                     .top(0)
                     .width(40)
                     .height(40)
                     .fill(red()),
                 box()
                     .key("faded")
                     .absolute()
                     .left(50)
                     .top(0)
                     .width(40)
                     .height(40)
                     .fill(green())
                     .opacity(0.5f),
                 box()
                     .key("turned")
                     .absolute()
                     .left(100)
                     .top(0)
                     .width(40)
                     .height(40)
                     .fill(blue())
                     .rotate(7),
                 box()
                     .key("recorded")
                     .absolute()
                     .left(0)
                     .top(50)
                     .width(40)
                     .height(40)
                     .fill(red())
                     .cache(Cache::Picture)});
}

}  // namespace

TEST(ComposeCache, AnEagerComposerPromotesTheEligibleNodeOnItsFirstFrame) {
  // The cost rule is a stopwatch, so what it promotes is a fact about the
  // machine: a run that means to TEST promotion asks for the eager policy
  // and gets the whole promotable set, everywhere, from frame one.
  Host host;
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
  host.composer.setProfiling(true);
  host.composer.render(eagerPage());
  host.frame();

  const Composer::NodeCost* plain = requireRow(host.composer, "plain");
  ASSERT_NE(plain, nullptr);
  EXPECT_EQ(plain->promotion, Composer::Promotion::Promoted)
      << "eager promotion waited for a stopwatch it is not supposed to read";
  EXPECT_EQ(plain->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u);
  // The bake is the picture the live paint paints, which is the whole of
  // what promotion may cost — eager or not.
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);
}

TEST(ComposeCache, ANodeInsideABakeIsNotBakedAgain) {
  // A bake of a node standing inside another node's bake is consulted only
  // on the frames that one is remade — the cost rule reads a stopwatch and
  // would never ask for it, and only the eager policy ever does. What it
  // costs is a composite the contract does not allow: the node's coverage
  // into its own image, that image into the layer above, and the layer
  // above onto the canvas, where the live paint composited once and a bake
  // may composite twice.
  Host host;
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
  host.composer.setProfiling(true);
  Element page = box().key("outer").width(100).height(100).fill(red());
  page.children(
      {box().key("inner").absolute().left(20).top(20).width(40).height(40).fill(
          green())});
  host.composer.render(std::move(page));
  host.frame();

  const Composer::NodeCost* outer = requireRow(host.composer, "outer");
  const Composer::NodeCost* inner = requireRow(host.composer, "inner");
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(inner, nullptr);
  ASSERT_EQ(outer->cacheState, Composer::CacheState::Promoted)
      << "the outer node was not baked, so nothing was inside a bake";
  EXPECT_NE(inner->cacheState, Composer::CacheState::Promoted)
      << "a node inside a bake was baked again, so its coverage is "
         "composited three times where the live paint composited once";
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u)
      << "the bake above already holds the node inside it";
  EXPECT_EQ(host.pixel(40, 40), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
}

TEST(ComposeCache, AnEagerComposerRefusesWhatTheCostRuleRefuses) {
  // Eager skips the cost question and NOTHING else. Every refusal is a
  // condition under which a bake would paint different pixels, so the two
  // policies must report the identical set of them for every node — a
  // policy that promoted its way past one would be promoting a picture
  // that moves.
  const auto refusalsUnder = [](Composer::PromotionPolicy policy) {
    Host host;
    host.composer.setAutoTexturePromotion(policy);
    host.composer.setProfiling(true);
    host.composer.render(eagerPage());
    host.frame();
    std::vector<std::pair<std::string, uint16_t>> out;
    for (const char* key : {"plain", "faded", "turned", "recorded"}) {
      const Composer::NodeCost* row = requireRow(host.composer, key);
      out.emplace_back(key, row ? row->refusals : 0xffffu);
    }
    return out;
  };
  const auto byCost = refusalsUnder(Composer::PromotionPolicy::ByCost);
  const auto eager = refusalsUnder(Composer::PromotionPolicy::Eager);
  ASSERT_EQ(byCost.size(), eager.size());
  for (size_t i = 0; i < byCost.size(); ++i)
    EXPECT_EQ(byCost[i].second, eager[i].second)
        << byCost[i].first << " is refused differently by the two policies";

  // And the refusals are the ones each leaf was built to carry, so the
  // comparison above is not two empty sets agreeing.
  Host host;
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
  host.composer.setProfiling(true);
  host.composer.render(eagerPage());
  host.frame();
  const Composer::NodeCost* faded = requireRow(host.composer, "faded");
  const Composer::NodeCost* turned = requireRow(host.composer, "turned");
  const Composer::NodeCost* recorded = requireRow(host.composer, "recorded");
  ASSERT_NE(faded, nullptr);
  ASSERT_NE(turned, nullptr);
  ASSERT_NE(recorded, nullptr);
  EXPECT_TRUE(faded->refused(Composer::Promotion::Composited));
  EXPECT_TRUE(turned->refused(Composer::Promotion::Transformed));
  EXPECT_TRUE(recorded->refused(Composer::Promotion::OptedOut));
  EXPECT_NE(faded->cacheState, Composer::CacheState::Promoted);
  EXPECT_NE(turned->cacheState, Composer::CacheState::Promoted);
  EXPECT_NE(recorded->cacheState, Composer::CacheState::Promoted);
}

namespace {

/** A PANEL THAT OPENS OVER WHAT IT HOLDS. The window clips its content to
 *  its own box and the box's height is the reveal, so the mark inside is
 *  cut on the frames the reveal is short and whole once it has run. The
 *  mark's OWN description never changes across the two: same content, same
 *  box, same place, same paint bounds — everything a bake's staleness rules
 *  compare stands still while the only thing that moved is the clip. */
Element revealedPage(float reveal) {
  Element page = box().width(240).height(240).fill(Fill::color({0, 0, 0, 1}));
  Element window =
      box().absolute().left(0).top(0).width(240).height(reveal).clip();
  window.children({box()
                       .absolute()
                       .left(20)
                       .top(20)
                       .width(200)
                       .height(200)
                       .fill(material::skia::Paint::sksl(gridEffect()))
                       .key("mark")});
  page.children({window.key("window")});
  return page;
}

/** The reveal, warmed short and then opened: thirty frames with the clip
 *  cutting the mark — long enough for the promoter to have baked whatever
 *  it is going to — and then the same tree with the window open. */
std::vector<SkColor> revealedThenOpen(bool promotion, int w, int h,
                                      bool* promotedOut) {
  Host host(w, h);
  host.composer.setAutoTexturePromotion(promotion
                                            ? Composer::PromotionPolicy::Eager
                                            : Composer::PromotionPolicy::Off);
  host.composer.setProfiling(true);
  host.composer.render(profiledUnder(revealedPage(60).key("page")));
  for (int i = 0; i < 30; ++i) host.frame();
  if (promotedOut)
    for (const Composer::NodeCost& row : host.composer.profile())
      *promotedOut |= row.cacheState == Composer::CacheState::Promoted;
  host.composer.render(profiledUnder(revealedPage((float)h).key("page")));
  for (int i = 0; i < 5; ++i) host.frame();
  return surfaceOf(host, w, h);
}

}  // namespace

TEST(ComposeCache, APromotedNodeIsRebakedWhenTheClipThatCutItOpens) {
  // A device bake carries the canvas's own clip into its layer, so what it
  // holds is the node's paint as that clip left it. A clip narrows and
  // widens for reasons the node's own bounds cannot see — a panel opening,
  // a window growing, an ancestor's layer standing over the box while it
  // fades in — and nothing else the staleness rules compare moves with it:
  // the paint bounds are the same bounds whatever the clip did to them. So
  // a bake taken under the narrow clip and held past it blits the CUT, and
  // the marks the clip removed never come back.
  bool promoted = false;
  const std::vector<SkColor> live = revealedThenOpen(false, 240, 240, nullptr);
  const std::vector<SkColor> baked =
      revealedThenOpen(true, 240, 240, &promoted);
  ASSERT_TRUE(promoted)
      << "nothing was promoted while the clip was narrow, so this compared "
         "two live renders";
  ASSERT_EQ(live.size(), baked.size());
  size_t differing = 0;
  const int worst = worstDrift(live, baked, &differing);
  EXPECT_LE(worst, 1) << differing << " pixels moved, worst " << worst
                      << " code values, after the clip that cut the bake "
                         "opened";
}

namespace {

/** A MARK THAT SETTLES A THOUSANDTH SHORT OF WHERE IT STARTED. The scale
 *  lane runs a hair below one for the whole warm and reaches one on the
 *  frame the still is taken, which moves the node's device geometry by far
 *  less than a pixel: its rounded device rect never moves, its clip never
 *  moves, and its content never changes. The mark is a stroked ellipse,
 *  whose top and bottom run nearly tangent to the grid — where a
 *  sub-pixel move costs a whole step of a pixel's coverage. */
Element settlingStamp(const ch::Output<float>* lane) {
  Element page = box().width(200).height(200).fill(Fill::color({0, 0, 0, 1}));
  page.children({box()
                     .absolute()
                     .left(40)
                     .top(70)
                     .width(120)
                     .height(60)
                     .shape(geometry::shapes::circle())
                     .stroke(FlatStroke{1.5f})
                     .scale(lane)
                     .key("stamp")});
  return page;
}

std::vector<SkColor> stampAfterSettling(bool promoted) {
  Host host(200, 200);
  host.composer.setAutoTexturePromotion(promoted
                                            ? Composer::PromotionPolicy::Eager
                                            : Composer::PromotionPolicy::Off);
  ch::Output<float> lane{0.997f};
  host.composer.render(settlingStamp(&lane));
  for (int i = 0; i < 8; ++i) host.frame();
  lane = 1.0f;
  host.composer.render(settlingStamp(&lane));
  host.frame();
  std::vector<SkColor> out;
  out.reserve(200u * 200u);
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 200; ++x) out.push_back(host.pixel(x, y));
  return out;
}

}  // namespace

TEST(ComposeCache, APromotedMarkIsRebakedWhenItsMatrixMovesInsideItsRect) {
  // WHAT DECIDES A BAKE'S PIXELS IS THE MATRIX, NOT THE RECT. The rect a
  // device bake is measured to is whole device pixels, so a matrix that
  // moves below one leaves it exactly where it was — and the picture
  // inside it is another picture: an edge lands on another part of its
  // pixel, a glyph takes another subpixel phase. Nothing else the
  // staleness rules compare moves with it, so a bake held across such a
  // move blits the frame it was taken on for as long as the node's
  // content stands still.
  const std::vector<SkColor> promoted = stampAfterSettling(true);
  const std::vector<SkColor> live = stampAfterSettling(false);
  ASSERT_EQ(promoted.size(), live.size());
  int worst = 0;
  size_t differing = 0;
  for (size_t i = 0; i < live.size(); ++i) {
    if (live[i] == promoted[i]) continue;
    ++differing;
    for (int shift : {0, 8, 16, 24})
      worst = std::max(worst, std::abs((int)((live[i] >> shift) & 0xffu) -
                                       (int)((promoted[i] >> shift) & 0xffu)));
  }
  EXPECT_LE(worst, 2) << differing << " pixels moved, worst " << worst
                      << " code values, when a promoted mark settled inside "
                         "the device rect its bake was measured to";
}
