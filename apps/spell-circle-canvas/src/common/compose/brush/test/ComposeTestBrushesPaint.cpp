// Brushes whose subject is a paint value: pattern fits, noise fills, the
// displacement and layer-style looks.

#include <set>

#include "support/PaintTestSupport.h"

TEST(ComposeBrushes, PatternIntegerFitNeverTearsTheLastTile) {
  // 160px run, 25px tile → 6 slots stretched to 26.67px: coverage reaches
  // BOTH ends with no torn tail (the Illustrator fit rule).
  Host host;
  brush::Pattern b;
  b.side = box().width(25).height(8).fill(red());
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(42, 100), SK_ColorRED);   // first slot starts at run 0
  EXPECT_EQ(host.pixel(178, 100), SK_ColorRED);  // last slot ends at run end
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);  // continuous through middle
}

TEST(ComposeBrushes, PatternCornerTileSitsOnTheBend) {
  Host host;
  brush::Pattern b;
  b.side = box().width(20).height(4).fill(red());
  b.corner = brush::CornerArt{box().width(12).height(12).fill(blue()),
                              brush::CornerAlign::Bisector};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(40, 40, 40, 40)
                      .shape([](SkSize s) {  // an L: right then down
                        SkPathBuilder p;
                        p.moveTo(0, 0);
                        p.lineTo(s.width(), 0);
                        p.lineTo(s.width(), s.height());
                        return p.detach();
                      })
                      .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(160, 40), SK_ColorBLUE);  // corner tile at the bend
  EXPECT_EQ(host.pixel(100, 40), SK_ColorRED);   // side tiles on the top leg
  EXPECT_EQ(host.pixel(160, 100), SK_ColorRED);  // and down the right leg
}

TEST(ComposeSeams, PerlinNoiseFillsWithVariation) {
  Host host(100, 100);
  host.composer.render(box().child(box().width(100).height(100).fill(
      material::skia::Paint::recipe(material::field::noise(0.05f, 4, 2.0f)))));
  host.frame();
  std::set<SkColor> distinct;
  for (int y = 10; y < 90; y += 8)
    for (int x = 10; x < 90; x += 8) distinct.insert(host.pixel(x, y));
  EXPECT_GT(distinct.size(), 30u);  // organic variation, not a flat fill
}

TEST(ComposeStyles, RippleDisplacesTheLayer) {
  // A thin horizontal red bar warped by a strong ripple: pixels appear
  // off-axis where the flat version has none.
  auto bar = [](bool warped) {
    Element e =
        box().absolute().inset(20, 96, 20, 96).fill(Fill::color({1, 0, 0, 1}));
    if (warped) e.effect(styles::ripple(10, 60));
    return box().child(std::move(e));
  };
  Host flat, warped;
  flat.composer.render(bar(false));
  warped.composer.render(bar(true));
  flat.frame();
  warped.frame();
  int off = 0, offFlat = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) {
      off += warped.pixel(x, 100 + dy) != SK_ColorBLACK;
      offFlat += flat.pixel(x, 100 + dy) != SK_ColorBLACK;
    }
  EXPECT_GT(off, 15);
  EXPECT_EQ(offFlat, 0);
}

namespace {

constexpr int kBoards = 24;

constexpr double kGroupPeriod = 2.0;  // the loop
constexpr double kGroupDone = 1.25;   // every board has finished by here

std::vector<choreograph::Output<float>>& boardFade() {
  static std::vector<choreograph::Output<float>> v(kBoards);
  return v;
}

std::vector<choreograph::Output<float>>& boardPop() {
  static std::vector<choreograph::Output<float>> v(kBoards);
  return v;
}

/** The staggered entrance, driven from the test loop rather than from a
 *  ticker so both hosts read exactly the same numbers on the same frame. */
void setBoardPhase(double t) {
  const double now = std::fmod(t, kGroupPeriod);
  for (int i = 0; i < kBoards; ++i) {
    const double delay = 0.9 * (double)i / (double)kBoards;
    double raw = (now - delay) / 0.35;
    raw = raw < 0.0 ? 0.0 : (raw > 1.0 ? 1.0 : raw);
    const float e = (float)(1.0 - std::pow(1.0 - raw, 3.0));  // easeOutCubic
    boardFade()[i] = 0.30f + 0.70f * e;
    boardPop()[i] = 0.55f + 0.45f * e;
  }
}

sk_sp<SkRuntimeEffect> boardGrain() {
  // Real per-pixel work with real local coordinates. A shader reads its
  // local space by INVERTING the CTM, so a bake that lands at a different
  // device offset than live paint would sample the grain differently — a
  // flat colour would hide that entirely. Kept cheap on purpose: the loop
  // below draws this once per piece per frame on two hosts, for hundreds of
  // frames.
  static sk_sp<SkRuntimeEffect> effect = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("half4 main(float2 p) {"
                 "  float g = 0.5 + 0.5 * sin(p.x * 0.71) * cos(p.y * 1.37);"
                 "  float h = 0.5 + 0.5 * sin(p.x * 0.13 + p.y * 0.09);"
                 "  return half4(half(0.20 + 0.62 * g), half(0.16 + 0.52 * h),"
                 "               half(0.10 + 0.34 * g), 1.0);"
                 "}"));
    if (!e) ADD_FAILURE() << err.c_str();
    return e;
  }();
  return effect;
}

/** One board: a mitred quad, a grain fill, a bevelled arris whose light
 *  angle follows the rotation, a seam keyline, and the two bound scalars. */
Element board(int i) {
  // Boards cycle through the three kumiko jig angles — 22.5°, 45°, 67.5°
  // — so every board is rotated (a device-space bake cannot share any of
  // them) and neighbours always meet at different angles.
  const float ang = 22.5f * (float)(1 + i % 3);
  SkPathBuilder quad;
  quad.moveTo(5, 0);
  quad.lineTo(74, 0);
  quad.lineTo(69, 11);
  quad.lineTo(0, 11);
  quad.close();
  SkPath shape = quad.detach();
  const int row = i / 5;
  return box()
      .absolute()
      .left(14.0f + 38.0f * (float)(i % 5))
      .top(14.0f + 38.0f * (float)row)
      .width(74)
      .height(11)
      .rotate(ang)
      .shape([shape](SkSize) { return shape; })
      .fill(material::skia::Paint::sksl(boardGrain()))
      .foreground(styles::BevelEmboss{0.8f,
                                      1.2f,
                                      120.0f + ang,
                                      {1, 0.96f, 0.86f, 0.45f},
                                      {0.14f, 0.09f, 0.03f, 0.45f}})
      .stroke(stroke(0.6f, Fill::color({0.29f, 0.21f, 0.12f, 0.55f}),
                     PathFormat::Align::Inner))
      .opacity(&boardFade()[i])
      .scale(&boardPop()[i]);
}

Element lattice(Cache mode) {
  Element g = box()
                  .absolute()
                  .left(0)
                  .top(0)
                  .width(240)
                  .height(240)
                  .key("lattice")
                  .cache(mode);
  for (int i = 0; i < kBoards; ++i) g.child(board(i));
  return g;
}

/** How the reference composites, which is the whole subtlety above.
 *  `Black` puts the lattice straight on the host's opaque clear, where
 *  srcOver's destination term vanishes; `Lit` gives it a ground to composite
 *  against; `LitIsolated` gives it that ground AND wraps the subtree in a
 *  no-op image filter, so BOTH sides pay the same requantisation. */
enum class Ground { Black, Lit, LitIsolated };

/** The lattice under a Cache::None wrapper — this IS `profiledUnder()`, with
 *  the geometry pinned so the wrapper cannot move the subject: the pixel
 *  comparison below needs the two trees to differ in exactly one thing, and
 *  the Cache mode is that one thing. */
Element latticeScene(Cache mode, Ground ground) {
  Element g = lattice(mode);
  if (ground == Ground::LitIsolated)
    g.effect(
        material::skia::Effect::filter(SkImageFilters::Offset(0, 0, nullptr)));
  Element wrapped = box()
                        .cache(Cache::None)
                        .absolute()
                        .left(0)
                        .top(0)
                        .width(240)
                        .height(240)
                        .child(std::move(g));
  Element root = stack();
  if (ground != Ground::Black)
    root.child(box().inset(0).fill(Fill::color({0.07f, 0.06f, 0.05f, 1})));
  return root.child(std::move(wrapped));
}

/** Both hosts, same frame, same numbers, promotion off on BOTH so the only
 *  difference between them is the Cache mode on the lattice. */
struct GroupPair {
  Host on{240, 240}, off{240, 240};
  explicit GroupPair(Ground ground = Ground::Black) {
    on.composer.setAutoTexturePromotion(false);
    off.composer.setAutoTexturePromotion(false);
    on.composer.setProfiling(true);
    setBoardPhase(0.0);
    // Rendered ONCE. A node carrying a shape callable compares unequal on
    // every re-describe — an incomparable callable never prunes — so a
    // per-frame render would mark every board dirty and no cache in the
    // library could hold, including the one under test.
    on.composer.render(latticeScene(Cache::Group, ground));
    off.composer.render(latticeScene(Cache::Auto, ground));
  }
  void at(double t) {
    setBoardPhase(t);
    on.frame();
    off.frame();
  }
  bool blitting() {
    const Composer::NodeCost* row = requireRow(on.composer, "lattice");
    return row && row->cacheState == Composer::CacheState::Group;
  }
  size_t bakesThisFrame() const {
    return on.composer.stats().texturesBaked;  // zeroed by every draw()
  }
  /** Differing pixels and the peak channel delta — the count is the claim,
   *  the peak is what makes a failure legible (one requantised edge reads
   *  very differently from a frozen or misplaced blit). */
  struct Divergence {
    size_t pixels = 0;
    int peak = 0;
  };
  Divergence divergence() {
    SkBitmap a, b;
    a.allocPixels(SkImageInfo::MakeN32Premul(240, 240));
    b.allocPixels(SkImageInfo::MakeN32Premul(240, 240));
    off.surface->readPixels(a.pixmap(), 0, 0);
    on.surface->readPixels(b.pixmap(), 0, 0);
    Divergence d;
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 240; ++x) {
        const SkColor ca = a.getColor(x, y), cb = b.getColor(x, y);
        if (ca == cb) continue;
        ++d.pixels;
        d.peak = std::max(
            {d.peak, std::abs((int)SkColorGetR(ca) - (int)SkColorGetR(cb)),
             std::abs((int)SkColorGetG(ca) - (int)SkColorGetG(cb)),
             std::abs((int)SkColorGetB(ca) - (int)SkColorGetB(cb)),
             std::abs((int)SkColorGetA(ca) - (int)SkColorGetA(cb))});
      }
    return d;
  }
};

/** Walk the whole loop — entrance, settle, hold and the WRAP back to zero,
 *  at 1/60 s, twice around — and report the worst frame plus how the frames
 *  divided between blitting and painting live. */
struct LoopResult {
  int blitFrames = 0, liveFrames = 0, differingFrames = 0, frames = 0;
  double firstBadT = -1;
  GroupPair::Divergence worst;
};

LoopResult walkTheLoop(GroupPair& pair) {
  LoopResult r;
  const int steps = (int)std::lround(2.0 * kGroupPeriod * 60.0);
  for (int i = 0; i <= steps; ++i) {
    const double t = (double)i / 60.0;
    pair.at(t);
    ++r.frames;
    (pair.blitting() ? r.blitFrames : r.liveFrames)++;
    const GroupPair::Divergence d = pair.divergence();
    if (d.pixels) {
      ++r.differingFrames;
      if (r.firstBadT < 0) r.firstBadT = t;
      if (d.pixels > r.worst.pixels) r.worst = d;
    }
  }
  return r;
}

}  // namespace

TEST(ComposeCache, GroupBakesASubtreeItsChildrensBindingsMadeUncacheable) {
  // The premise first: this lattice is refused by every other cache. Its
  // children never stop being volatile — the bindings stay connected for the
  // whole loop, whether or not their values are moving — so `Cache::Texture`
  // on the container bakes nothing (a fill-less container has no own paint
  // to bake) and the picture path is blocked along with it. Cache::Group is
  // the only route by which such a subtree holds pixels at all.
  GroupPair pair;
  for (int i = 0; i < 12; ++i)
    pair.at(kGroupDone + 0.4);  // settled: the same numbers every frame
  EXPECT_TRUE(pair.blitting())
      << "the settled lattice was not baked, so every pixel test below is "
         "comparing two identical live paints and proving nothing";
  EXPECT_GE(pair.on.composer.stats().texturesLive, 1u);
  EXPECT_EQ(pair.bakesThisFrame(), 0u)
      << "a settled group re-baked instead of blitting what it already had";

  // …and the control for the premise: the SAME tree without Cache::Group
  // holds no bake at all. If this ever starts passing, something else began
  // caching the lattice and the assertion above stopped being about Group.
  EXPECT_EQ(pair.off.composer.stats().texturesLive, 0u)
      << "the Cache::Auto lattice cached pixels by some other route, so the "
         "comparison is not Group-against-nothing";
}

TEST(ComposeCache, GroupDropsTheBakeOnTheFrameABindingTicks) {
  // THE MECHANISM, asserted rather than argued. A group that keeps its bake
  // while its bindings move FREEZES the animation — and a frozen entrance
  // looks perfectly correct in any still, which is exactly why this needs a
  // mechanism test and not only a pixel test.
  GroupPair pair;
  for (int i = 0; i < 12; ++i) pair.at(kGroupDone + 0.4);
  ASSERT_TRUE(pair.blitting()) << "never reached the baked state";

  // One tick of the entrance: every board's opacity and scale move.
  pair.at(0.5);
  EXPECT_FALSE(pair.blitting())
      << "the bake survived a frame on which every bound opacity and scale "
         "below the node changed — the group is holding pixels that are no "
         "longer this frame's pixels";
  EXPECT_EQ(pair.bakesThisFrame(), 0u)
      << "the group re-baked on a moving frame: a bake per frame costs "
         "strictly more than the paint it replaces";

  // Hold that same phase and it comes back — the memo says "not changing",
  // not "finished".
  pair.at(0.5);
  EXPECT_TRUE(pair.blitting())
      << "the group did not re-bake once its scalars held still again";
  EXPECT_EQ(pair.bakesThisFrame(), 1u);
}

TEST(ComposeCache, GroupIsPixelIdenticalAcrossTheWholeLoop) {
  // Not one still, and not only the settled window: the entrance, the
  // settle, the hold, and the WRAP back to zero, at 1/60 s, twice around. A
  // group that is exact where it happens to be baked and wrong on the frames
  // it takes or drops the bake would pass any single capture.
  //
  // Over the host's opaque clear, where the destination term of srcOver
  // vanishes and the bake's one extra requantisation cannot show, the
  // standard is BYTE IDENTITY and nothing less.
  GroupPair pair(Ground::Black);
  const LoopResult r = walkTheLoop(pair);
  EXPECT_EQ(r.differingFrames, 0)
      << r.differingFrames << " of " << r.frames
      << " frames differ, first at t=" << r.firstBadT << ", worst frame "
      << r.worst.pixels << " pixels at peak " << r.worst.peak;
  // Both halves, or the loop proved nothing. A run that never baked is two
  // live paints compared with each other; a run that never dropped is a
  // frozen lattice, which is also "identical" to itself.
  EXPECT_GT(r.blitFrames, 0) << "the group never baked over the whole loop";
  EXPECT_GT(r.liveFrames, 0)
      << "the group never dropped its bake over an entrance that moves every "
         "board — the drop is not being exercised, so this test cannot see "
         "the bug it exists for";
}

TEST(ComposeCache, AGroupBakeIsExactlyALayerAndNothingMore) {
  // The same loop over a LIT ground, which is where an isolating bake and
  // live paint genuinely part company.
  //
  // The control is the point: wrap the reference subtree in a no-op image
  // filter, so the reference is isolated into a layer exactly as the bake is,
  // and the difference goes to zero. That is a stronger statement than a
  // tolerance, because a tolerance is fitted and this is not: it says the
  // bake's ONLY deviation from live paint is the layer, which the author can
  // already ask for by hand.
  {
    GroupPair isolated(Ground::LitIsolated);
    const LoopResult r = walkTheLoop(isolated);
    EXPECT_EQ(r.differingFrames, 0)
        << "a group bake differs from compositing the same subtree through a "
           "layer: "
        << r.worst.pixels << " pixels at peak " << r.worst.peak
        << ", first at t=" << r.firstBadT;
    EXPECT_GT(r.blitFrames, 0);
    EXPECT_GT(r.liveFrames, 0);
  }
  // …and the unisolated comparison, kept as a CEILING rather than an
  // equality, because it cannot be an equality: antialiased coverage rounds
  // to 8 bits once more on the way through a bake, so edge pixels land a
  // level or two off. The ceiling is loose on purpose — what it is built to
  // catch is a frozen bake or a misplaced blit, which move whole boards and
  // blow past any edge-sized budget.
  {
    GroupPair lit(Ground::Lit);
    const LoopResult r = walkTheLoop(lit);
    EXPECT_LE(r.worst.peak, 4)
        << "the divergence over a lit ground is not edge requantisation alone "
           "— "
        << r.worst.pixels << " pixels at peak " << r.worst.peak;
    EXPECT_LT(r.worst.pixels, (size_t)(240 * 240 / 10))
        << "more than a tenth of the canvas moved, which is not an edge "
           "effect";
    EXPECT_GT(r.blitFrames, 0);
  }
}

TEST(ComposeCache, AGroupsOwnFadeDoesNotDropItsBake) {
  // A deliberate exclusion, tested because it is a decision and not a
  // consequence. The root's own opacity and transform are applied by
  // paint()'s saveLayer and matrix OUTSIDE the bake — so a group fading in
  // as a whole must keep baking through the fade, and the fade must still
  // composite exactly. Including them in the memo would have cost the bake
  // on every frame of every entrance, for a change the bake does not
  // contain.
  static choreograph::Output<float> groupFade{1.0f};
  const auto scene = [](Cache mode) {
    Element g = lattice(mode).opacity(&groupFade);
    return stack().child(box()
                             .cache(Cache::None)
                             .absolute()
                             .left(0)
                             .top(0)
                             .width(240)
                             .height(240)
                             .child(std::move(g)));
  };
  Host on(240, 240), off(240, 240);
  on.composer.setAutoTexturePromotion(false);
  off.composer.setAutoTexturePromotion(false);
  on.composer.setProfiling(true);
  setBoardPhase(kGroupDone + 0.4);  // the boards hold still…
  groupFade = 1.0f;
  on.composer.render(scene(Cache::Group));
  off.composer.render(scene(Cache::Auto));
  for (int i = 0; i < 8; ++i) {
    on.frame();
    off.frame();
  }
  const Composer::NodeCost* row = requireRow(on.composer, "lattice");
  ASSERT_NE(row, nullptr);
  ASSERT_EQ(row->cacheState, Composer::CacheState::Group);

  int fadeFrames = 0, blits = 0;
  size_t bakes = 0;
  for (int i = 1; i <= 20; ++i) {  // …while the GROUP fades
    groupFade = 1.0f - 0.045f * (float)i;
    on.frame();
    off.frame();
    ++fadeFrames;
    bakes += on.composer.stats().texturesBaked;
    const Composer::NodeCost* r = requireRow(on.composer, "lattice");
    if (r && r->cacheState == Composer::CacheState::Group) ++blits;
    ASSERT_TRUE(identicalPixels(off, on, 240, 240))
        << "the group's own opacity composited differently through the blit "
           "than through the live paint, at frame "
        << i;
  }
  EXPECT_EQ(blits, fadeFrames)
      << "the group dropped its bake for its OWN fade, which the bake does "
         "not contain — the root's paint slots are in the memo and must not "
         "be";
  EXPECT_EQ(bakes, 0u) << "the group re-baked during its own fade";
}

namespace {

/** Did a lattice carrying `extra` as an extra child ever reach the baked
 *  state over ten settled frames? */
bool groupBakesWith(Element extra) {
  Host host(240, 240);
  host.composer.setAutoTexturePromotion(false);
  host.composer.setProfiling(true);
  setBoardPhase(kGroupDone + 0.4);
  Element g = lattice(Cache::Group).child(std::move(extra));
  host.composer.render(stack().child(box()
                                         .cache(Cache::None)
                                         .absolute()
                                         .left(0)
                                         .top(0)
                                         .width(240)
                                         .height(240)
                                         .child(std::move(g))));
  bool sawBlit = false;
  for (int i = 0; i < 10; ++i) {
    host.frame();
    const Composer::NodeCost* row = requireRow(host.composer, "lattice");
    sawBlit |= row && row->cacheState == Composer::CacheState::Group;
  }
  return sawBlit;
}

Element plainExtra() {
  return box().absolute().left(100).top(100).width(30).height(30).fill(red());
}

}  // namespace

TEST(ComposeCache, GroupRefusesWhatItsMemoCannotSee) {
  // The control first: a plain extra child changes nothing.
  ASSERT_TRUE(groupBakesWith(plainExtra()))
      << "the fixture does not bake even with an innocuous child, so none of "
         "the refusals below is being attributed to the right thing";

  // A LIVE MATERIAL. uTime moves pixels every frame with no float anywhere in
  // the tree to compare, so a group holding a bake across one would blit last
  // second's picture forever.
  EXPECT_FALSE(groupBakesWith(
      plainExtra().fill(material::skia::Paint::sksl(heavyEffect(true)))))
      << "a group baked over a live material";

  // A NON-SRCOVER BLEND below the root: inside the bake it resolves against
  // transparent black instead of against the ground.
  EXPECT_FALSE(groupBakesWith(plainExtra().blend(SkBlendMode::kMultiply)))
      << "a group baked over a kMultiply child, which resolves against "
         "transparent black inside a bake";

  // A Cache::None LEAF — declared per-frame volatility, by definition
  // invisible to a value comparison.
  EXPECT_FALSE(groupBakesWith(plainExtra().cache(Cache::None)))
      << "a group baked over a Cache::None leaf";

  // An ANIMATED DECORATION: the same argument as the live material, one
  // level out from the fill.
  static choreograph::Output<float> dash{0};
  PathFormat marching = stroke(2.0f, Fill::color({1, 1, 1, 1}));
  marching.dashIntervals = {4.0f, 4.0f};
  marching.dashPhaseBinding = &dash;
  EXPECT_FALSE(groupBakesWith(plainExtra().background(marching)))
      << "a group baked over an animated decoration";
}

TEST(ComposeCache, AMovingGroupRefusesTheBakeRatherThanRemakingIt) {
  // The other documented limit, and the one whose failure is a slowdown
  // rather than a wrong picture: a device-pinned bake remade every frame
  // costs strictly more than the paint it replaces, so a moving group must
  // REFUSE the bake instead of remaking it. The group's own transform is the
  // case a declaration can see coming; a resizing host is the case only the
  // device rect can.
  static choreograph::Output<float> slide{0};
  Host host(240, 240);
  host.composer.setAutoTexturePromotion(false);
  host.composer.setProfiling(true);
  setBoardPhase(kGroupDone + 0.4);
  slide = 0.0f;
  Element g = lattice(Cache::Group).translateX(&slide);
  host.composer.render(stack().child(box()
                                         .cache(Cache::None)
                                         .absolute()
                                         .left(0)
                                         .top(0)
                                         .width(240)
                                         .height(240)
                                         .child(std::move(g))));
  for (int i = 0; i < 6; ++i) host.frame();
  size_t bakes = 0;
  for (int i = 1; i <= 20; ++i) {
    slide = 0.7f * (float)i;
    host.frame();
    bakes += host.composer.stats().texturesBaked;
  }
  EXPECT_EQ(bakes, 0u) << "the group re-baked while it was sliding: 20 frames, "
                       << bakes << " bakes";
  const Composer::NodeCost* row = requireRow(host.composer, "lattice");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Group);
}

TEST(ComposeBrushes, PatternCornerTileAtTheClosedSeam) {
  // The seam of a closed contour is a corner like any other, even though no
  // moveTo/lineTo pair announces it: the last segment turns into the first.
  // A corner tile belongs there too.
  Host host;
  brush::Pattern b;
  b.side = box().width(20).height(4).fill(red());
  b.corner = brush::CornerArt{box().width(12).height(12).fill(blue()),
                              brush::CornerAlign::Bisector};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .shape([](SkSize s) {  // closed rect starting at (0,0)
                        SkPathBuilder p;
                        p.moveTo(0, 0);
                        p.lineTo(s.width(), 0);
                        p.lineTo(s.width(), s.height());
                        p.lineTo(0, s.height());
                        p.close();
                        return p.detach();
                      })
                      .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);  // the seam corner tile
}
