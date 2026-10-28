// The group bake: one layer over a subtree whose children's bindings tick --
// what it charges, when it drops, what its memo cannot see, and the area it
// bakes wherever the node sits.

#include <sigilmaterial/skia/Paint.h>

#include <utility>

#include "support/BrushTestSupport.h"
#include "support/Rows.h"

namespace {

/** @p count identical 100x100 Cache::Group leaves at (@p x, @p y), under a
 *  Cache::None wrapper so every one is painted — and its value memo run —
 *  each frame. */
Element groupField(int count, float x, float y, Cache mode = Cache::Group) {
  Element root = box().cache(Cache::None);
  for (int index = 0; index < count; ++index)
    root.children({box()
                       .absolute()
                       .left(x)
                       .top(y)
                       .width(100)
                       .height(100)
                       .cache(mode)
                       .fill(Fill::color({0.2f, 0.5f, 0.8f, 1}))});
  return root;
}

/** Frame past the settle: a group's first frame seeds its value memo, its
 *  second takes the bake, the rest prove the bake holds. */
void settleGroups(Host& host) {
  for (int index = 0; index < 8; ++index) host.frame();
}

}  // namespace

// A group bake is charged and drawn at the node's own area wherever it sits.
TEST(ComposeCache, AGroupBakeTakesOnlyItsOwnArea) {
  sigil::compose::test::checkRows({
      {"AGroupBakeChargesOnlyTheNodesOwnArea",
       [] {
         // 150 settled 100x100 groups charge 150 bakes of their own area — far
         // inside the bake budget, so every one of them holds a texture. A bake
         // rect that reached the canvas origin on either axis would charge that
         // whole span per node instead, exhaust the budget partway through the
         // paint order, and every node after that point would fail
         // affordability and paint live. Three placements, each on a canvas
         // long in exactly the axis it pins, so a rect grown to left = 0 and
         // one grown to top = 0 are each caught alone as well as together.
         struct Placement {
           int width, height;  // the canvas
           float x, y;  // the node, away from the origin toward the far corner
         };
         const Placement placements[] = {
             {800, 600, 700, 500},  // both axes at once
             {4000, 100, 3900, 0},  // the LEFT side alone
             {100, 4000, 0, 3900},  // the TOP side alone
         };
         for (const Placement& placement : placements) {
           Host host(placement.width, placement.height);
           host.composer.render(groupField(150, placement.x, placement.y));
           settleGroups(host);
           EXPECT_EQ(host.composer.stats().texturesLive, 150u)
               << "on a " << placement.width << "x" << placement.height
               << " canvas, 100x100 groups at (" << placement.x << ", "
               << placement.y << ") did not all hold their bakes — a group "
               << "away from the origin is charging more than its own area "
               << "against the bake budget, and the overcharge evicted the "
                  "rest";
           host.frame();
           EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
               << "a settled group re-baked instead of blitting what it "
                  "already "
                  "had — the bake rect is not holding still";
         }
       }},
      {"AGroupBakeAtTheCanvasOriginStillBakes",
       [] {
         // The corner where the node's device bounds and the clip share a
         // corner: the intersection IS the node's rect, and it must keep
         // baking. Pinned so the placement rows can never pass for the wrong
         // reason: a group that stopped baking entirely fails here by name.
         Host host(800, 600);
         host.composer.setProfiling(true);
         Element root =
             box()
                 .cache(Cache::None)
                 .children({box()
                                .absolute()
                                .left(0)
                                .top(0)
                                .width(100)
                                .height(100)
                                .cache(Cache::Group)
                                .key("corner")
                                .fill(Fill::color({0.2f, 0.5f, 0.8f, 1}))});
         host.composer.render(root);
         settleGroups(host);
         EXPECT_EQ(host.composer.stats().texturesLive, 1u);
         const Composer::NodeCost* row = requireRow(host.composer, "corner");
         ASSERT_NE(row, nullptr);
         EXPECT_EQ(row->cacheState, Composer::CacheState::Group)
             << "the group at the canvas origin is not blitting its bake";
       }},
      {"AGroupBakeAwayFromTheOriginChangesNoPixels",
       [] {
         // The bake rect governs allocation and budget accounting, never
         // placement: the blit draws the texture at the rect's own origin. So a
         // group anywhere on the canvas must produce byte-identical output to
         // the same subtree painted live every frame — including on the frame
         // the bake is first taken and on every blit after it.
         const SkPoint positions[] = {{0, 0}, {400, 400}, {700, 500}};
         for (const SkPoint& at : positions) {
           Host baked(800, 600), live(800, 600);
           baked.composer.render(groupField(1, at.x(), at.y(), Cache::Group));
           live.composer.render(groupField(1, at.x(), at.y(), Cache::None));
           for (int index = 0; index < 8; ++index) {
             baked.frame();
             live.frame();
             ASSERT_TRUE(identicalPixels(baked, live, 800, 600))
                 << "frame " << index << ": the group bake at (" << at.x()
                 << ", " << at.y()
                 << ") drew different pixels than the live paint";
           }
         }
       }},
  });
}

namespace {

constexpr int kBoards = 24;

constexpr double kGroupPeriod = 2.0;  // the loop
constexpr double kGroupDone = 1.25;   // every board has finished by here

/** One live value per board, each its own cell. */
std::vector<sigil::motion::Animatable<float>> liveBoards() {
  std::vector<sigil::motion::Animatable<float>> boards;
  for (int index = 0; index < kBoards; ++index)
    boards.push_back(sigil::motion::animatable(0.0f));
  return boards;
}

std::vector<sigil::motion::Animatable<float>>& boardFade() {
  static std::vector<sigil::motion::Animatable<float>> boards = liveBoards();
  return boards;
}

std::vector<sigil::motion::Animatable<float>>& boardPop() {
  static std::vector<sigil::motion::Animatable<float>> boards = liveBoards();
  return boards;
}

/** The staggered entrance, driven from the test loop rather than from a
 *  motion so both hosts read exactly the same numbers on the same frame. */
void setBoardPhase(double time) {
  const double now = std::fmod(time, kGroupPeriod);
  for (int index = 0; index < kBoards; ++index) {
    const double delay = 0.9 * (double)index / (double)kBoards;
    double raw = (now - delay) / 0.35;
    raw = raw < 0.0 ? 0.0 : (raw > 1.0 ? 1.0 : raw);
    const float eased =
        (float)(1.0 - std::pow(1.0 - raw, 3.0));  // easeOutCubic
    boardFade()[index] = 0.30f + 0.70f * eased;
    boardPop()[index] = 0.55f + 0.45f * eased;
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
Element board(int index) {
  // Boards cycle through the three kumiko jig angles — 22.5°, 45°, 67.5°
  // — so every board is rotated (a device-space bake cannot share any of
  // them) and neighbours always meet at different angles.
  const float ang = 22.5f * (float)(1 + index % 3);
  SkPathBuilder quad;
  quad.moveTo(5, 0);
  quad.lineTo(74, 0);
  quad.lineTo(69, 11);
  quad.lineTo(0, 11);
  quad.close();
  SkPath shape = quad.detach();
  const int row = index / 5;
  return box()
      .absolute()
      .left(14.0f + 38.0f * (float)(index % 5))
      .top(14.0f + 38.0f * (float)row)
      .width(74)
      .height(11)
      .rotate(ang)
      .shape(skiaShape([shape] { return shape; }))
      .fill(material::from(
                material::skia::base(material::skia::sksl(boardGrain())))
                .effects(material::Filter::bevel(
                    {.depth = 0.8f,
                     .size = 1.2f,
                     .angleDegrees = 120.0f + ang,
                     .highlight = {1, 0.96f, 0.86f, 0.45f},
                     .shadow = {0.14f, 0.09f, 0.03f, 0.45f}})))
      .stroke(stroke(0.6f, Fill::color({0.29f, 0.21f, 0.12f, 0.55f}),
                     PathFormat::Align::Inner))
      .opacity(boardFade()[index])
      .scale(boardPop()[index]);
}

Element lattice(Cache mode) {
  Element group = box()
                      .absolute()
                      .left(0)
                      .top(0)
                      .width(240)
                      .height(240)
                      .key("lattice")
                      .cache(mode);
  for (int index = 0; index < kBoards; ++index) group.children({board(index)});
  return group;
}

/** How the reference composites, which is the whole subtlety above.
 *  `Black` puts the lattice straight on the host's opaque clear, where
 *  srcOver's destination term vanishes; `LitIsolated` gives it a ground to
 *  composite against AND wraps the subtree in a no-op image filter, so
 *  BOTH sides pay the same requantisation and the comparison stays an
 *  equality rather than a fitted tolerance. */
enum class Ground { Black, LitIsolated };

/** The lattice under a Cache::None wrapper — this IS `profiledUnder()`, with
 *  the geometry pinned so the wrapper cannot move the subject: the pixel
 *  comparison below needs the two trees to differ in exactly one thing, and
 *  the Cache mode is that one thing. */
Element latticeScene(Cache mode, Ground ground) {
  Element group = lattice(mode);
  if (ground == Ground::LitIsolated)
    group.filter(material::skia::filter(SkImageFilters::Offset(0, 0, nullptr)));
  Element wrapped = box()
                        .cache(Cache::None)
                        .absolute()
                        .left(0)
                        .top(0)
                        .width(240)
                        .height(240)
                        .children({std::move(group)});
  Element root = stack();
  if (ground != Ground::Black)
    root.children({box().inset(0).fill(Fill::color({0.07f, 0.06f, 0.05f, 1}))});
  return root.children({std::move(wrapped)});
}

/** Both hosts, same frame, same numbers, so the only difference between
 *  them is the Cache mode on the lattice. */
struct GroupPair {
  Host on{240, 240}, off{240, 240};
  explicit GroupPair(Ground ground = Ground::Black) {
    on.composer.setProfiling(true);
    setBoardPhase(0.0);
    // Rendered ONCE. A node carrying a shape callable compares unequal on
    // every re-describe — an incomparable callable never prunes — so a
    // per-frame render would mark every board dirty and no cache in the
    // library could hold, including the one under test.
    on.composer.render(latticeScene(Cache::Group, ground));
    off.composer.render(latticeScene(Cache::Auto, ground));
  }
  void at(double time) {
    setBoardPhase(time);
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
    SkBitmap offPixels, onPixels;
    offPixels.allocPixels(SkImageInfo::MakeN32Premul(240, 240));
    onPixels.allocPixels(SkImageInfo::MakeN32Premul(240, 240));
    off.surface->readPixels(offPixels.pixmap(), 0, 0);
    on.surface->readPixels(onPixels.pixmap(), 0, 0);
    Divergence measured;
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 240; ++x) {
        const SkColor offColor = offPixels.getColor(x, y),
                      onColor = onPixels.getColor(x, y);
        if (offColor == onColor) continue;
        ++measured.pixels;
        measured.peak = std::max(
            {measured.peak,
             std::abs((int)SkColorGetR(offColor) - (int)SkColorGetR(onColor)),
             std::abs((int)SkColorGetG(offColor) - (int)SkColorGetG(onColor)),
             std::abs((int)SkColorGetB(offColor) - (int)SkColorGetB(onColor)),
             std::abs((int)SkColorGetA(offColor) - (int)SkColorGetA(onColor))});
      }
    return measured;
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
  LoopResult result;
  const int steps = (int)std::lround(2.0 * kGroupPeriod * 60.0);
  for (int index = 0; index <= steps; ++index) {
    const double time = (double)index / 60.0;
    pair.at(time);
    ++result.frames;
    (pair.blitting() ? result.blitFrames : result.liveFrames)++;
    const GroupPair::Divergence frameDivergence = pair.divergence();
    if (frameDivergence.pixels) {
      ++result.differingFrames;
      if (result.firstBadT < 0) result.firstBadT = time;
      if (frameDivergence.pixels > result.worst.pixels)
        result.worst = frameDivergence;
    }
  }
  return result;
}

}  // namespace

// Over the whole loop a group paints the live paint's pixels, and over a lit
// ground the pixels the same subtree composites through a layer.
TEST(ComposeCache, AGroupBakeIsTheLivePaintThroughALayer) {
  sigil::compose::test::checkRows({
      {"GroupIsPixelIdenticalAcrossTheWholeLoop",
       [] {
         // Not one still, and not only the settled window: the entrance, the
         // settle, the hold, and the WRAP back to zero, at 1/60 s, twice
         // around. A group that is exact where it happens to be baked and wrong
         // on the frames it takes or drops the bake would pass any single
         // capture.
         //
         // Over the host's opaque clear, where the destination term of srcOver
         // vanishes and the bake's one extra requantisation cannot show, the
         // standard is BYTE IDENTITY and nothing less.
         GroupPair pair(Ground::Black);
         const LoopResult result = walkTheLoop(pair);
         EXPECT_EQ(result.differingFrames, 0)
             << result.differingFrames << " of " << result.frames
             << " frames differ, first at t=" << result.firstBadT
             << ", worst frame " << result.worst.pixels << " pixels at peak "
             << result.worst.peak;
         // Both halves, or the loop proved nothing. A run that never baked is
         // two live paints compared with each other; a run that never dropped
         // is a frozen lattice, which is also "identical" to itself.
         EXPECT_GT(result.blitFrames, 0)
             << "the group never baked over the whole loop";
         EXPECT_GT(result.liveFrames, 0)
             << "the group never dropped its bake over "
                "an entrance that moves every "
                "board, so the drop is not being "
                "exercised";
       }},
      {"AGroupBakeIsExactlyALayerAndNothingMore",
       [] {
         // The same loop over a LIT ground, which is where an isolating bake
         // and live paint genuinely part company.
         //
         // The control is the point: wrap the reference subtree in a no-op
         // image filter, so the reference is isolated into a layer exactly as
         // the bake is, and the difference goes to zero. That is a stronger
         // statement than a tolerance, because a tolerance is fitted and this
         // is not: it says the bake's ONLY deviation from live paint is the
         // layer, which the author can already ask for by hand.
         {
           GroupPair isolated(Ground::LitIsolated);
           const LoopResult result = walkTheLoop(isolated);
           EXPECT_EQ(result.differingFrames, 0)
               << "a group bake differs from compositing the same subtree "
                  "through a "
                  "layer: "
               << result.worst.pixels << " pixels at peak " << result.worst.peak
               << ", first at t=" << result.firstBadT;
           EXPECT_GT(result.blitFrames, 0);
           EXPECT_GT(result.liveFrames, 0);
         }
       }},
  });
}

namespace {

/** Did a lattice carrying `extra` as an extra child ever reach the baked
 *  state over ten settled frames? */
bool groupBakesWith(Element extra) {
  Host host(240, 240);
  host.composer.setProfiling(true);
  setBoardPhase(kGroupDone + 0.4);
  Element group = lattice(Cache::Group).children({std::move(extra)});
  host.composer.render(stack().children({box()
                                             .cache(Cache::None)
                                             .absolute()
                                             .left(0)
                                             .top(0)
                                             .width(240)
                                             .height(240)
                                             .children({std::move(group)})}));
  bool sawBlit = false;
  for (int index = 0; index < 10; ++index) {
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

namespace {

/** An extra child of the lattice that carries something a group's memo
 *  cannot see. */
struct RefusedChild {
  const char* name;
  Element (*child)();
};

Element liveMaterialChild() {
  return plainExtra().fill(
      material::skia::base(material::skia::sksl(heavyEffect(true))));
}

Element multipliedChild() {
  return plainExtra().blendMode(material::BlendMode::Multiply);
}

Element uncachedChild() { return plainExtra().cache(Cache::None); }

Element marchingChild() {
  static sigil::motion::Animatable<float> dash =
      sigil::motion::animatable(0.0f);
  PathFormat marching = stroke(2.0f, Fill::color({1, 1, 1, 1}));
  marching.dashIntervals = {4.0f, 4.0f};
  marching.dashPhaseBinding = dash;
  return plainExtra().background(marching);
}

}  // namespace

// A group bakes only what its memo can compare, so a subtree holding a live
// material, a blend that reads the ground, a Cache::None leaf or an animated
// decoration is refused the bake.
TEST(ComposeCache, AGroupIsRefusedWhatItsMemoCannotSee) {
  ASSERT_TRUE(groupBakesWith(plainExtra()))
      << "the fixture does not bake even with an innocuous child, so none of "
         "the refusals below is being attributed to the right thing";
  const RefusedChild rows[] = {
      // Time moves its pixels with no float in the tree to compare.
      {"ALiveMaterial", liveMaterialChild},
      // Inside a bake it resolves against transparent black, not the ground.
      {"ANonSourceOverBlend", multipliedChild},
      // Declared per-frame volatility, invisible to a value comparison.
      {"ACacheNoneLeaf", uncachedChild},
      // The live material's argument, one level out from the fill.
      {"AnAnimatedDecoration", marchingChild},
  };
  sigil::compose::test::forEachRow(rows, [](const RefusedChild& row) {
    EXPECT_FALSE(groupBakesWith(row.child()))
        << "a group baked over " << row.name;
  });
}

// A group holds a subtree whose children are bound, drops the bake on a
// frame a child moves, and keeps it through its own fade and its own motion.
TEST(ComposeCache, AGroupHoldsASettledSubtreeAndDropsItWhileAChildMoves) {
  sigil::compose::test::checkRows({
      {"GroupBakesASubtreeItsChildrensBindingsMadeUncacheable",
       [] {
         // The premise first: this lattice is refused by every other cache. Its
         // children never stop being volatile — the bindings stay connected for
         // the whole loop, whether or not their values are moving — so
         // `Cache::Texture` on the container bakes nothing (a fill-less
         // container has no own paint to bake) and the picture path is blocked
         // along with it. Cache::Group is the only route by which such a
         // subtree holds pixels at all.
         GroupPair pair;
         for (int index = 0; index < 12; ++index)
           pair.at(kGroupDone + 0.4);  // settled: the same numbers every frame
         EXPECT_TRUE(pair.blitting())
             << "the settled lattice was not baked, so every pixel test below "
                "is "
                "comparing two identical live paints and proving nothing";
         EXPECT_GE(pair.on.composer.stats().texturesLive, 1u);
         EXPECT_EQ(pair.bakesThisFrame(), 0u)
             << "a settled group re-baked instead of blitting what it already "
                "had";

         // …and the control for the premise: the SAME tree without Cache::Group
         // holds no bake at all. If this ever starts passing, something else
         // began caching the lattice and the assertion above stopped being
         // about Group.
         EXPECT_EQ(pair.off.composer.stats().texturesLive, 0u)
             << "the Cache::Auto lattice cached pixels by some other route, so "
                "the "
                "comparison is not Group-against-nothing";
       }},
      {"GroupDropsTheBakeOnTheFrameABindingTicks",
       [] {
         // THE MECHANISM, asserted rather than argued. A group that keeps its
         // bake while its bindings move FREEZES the animation — and a frozen
         // entrance looks perfectly correct in any still, which is exactly why
         // this needs a mechanism test and not only a pixel test.
         GroupPair pair;
         for (int index = 0; index < 12; ++index) pair.at(kGroupDone + 0.4);
         ASSERT_TRUE(pair.blitting()) << "never reached the baked state";

         // One tick of the entrance: every board's opacity and scale move.
         pair.at(0.5);
         EXPECT_FALSE(pair.blitting()) << "the bake survived a frame on which "
                                          "every bound opacity and scale "
                                          "below the node changed — the group "
                                          "is holding pixels that are no "
                                          "longer this frame's pixels";
         EXPECT_EQ(pair.bakesThisFrame(), 0u)
             << "the group re-baked on a moving frame: a bake per frame costs "
                "strictly more than the paint it replaces";

         // Hold that same phase and it comes back — the memo says "not
         // changing", not "finished".
         pair.at(0.5);
         EXPECT_TRUE(pair.blitting())
             << "the group did not re-bake once its scalars held still again";
         EXPECT_EQ(pair.bakesThisFrame(), 1u);
       }},
      {"AGroupsOwnFadeDoesNotDropItsBake",
       [] {
         // A deliberate exclusion, tested because it is a decision and not a
         // consequence. The root's own opacity and transform are applied by
         // paint()'s saveLayer and matrix OUTSIDE the bake — so a group fading
         // in as a whole must keep baking through the fade, and the fade must
         // still composite exactly. Including them in the memo would have cost
         // the bake on every frame of every entrance, for a change the bake
         // does not contain.
         static sigil::motion::Animatable<float> groupFade =
             sigil::motion::animatable(1.0f);
         const auto scene = [](Cache mode) {
           Element group = lattice(mode).opacity(groupFade);
           return stack().children({box()
                                        .cache(Cache::None)
                                        .absolute()
                                        .left(0)
                                        .top(0)
                                        .width(240)
                                        .height(240)
                                        .children({std::move(group)})});
         };
         Host on(240, 240), off(240, 240);
         on.composer.setProfiling(true);
         setBoardPhase(kGroupDone + 0.4);  // the boards hold still…
         groupFade = 1.0f;
         on.composer.render(scene(Cache::Group));
         off.composer.render(scene(Cache::Auto));
         for (int index = 0; index < 8; ++index) {
           on.frame();
           off.frame();
         }
         const Composer::NodeCost* row = requireRow(on.composer, "lattice");
         ASSERT_NE(row, nullptr);
         ASSERT_EQ(row->cacheState, Composer::CacheState::Group);

         int fadeFrames = 0, blits = 0;
         size_t bakes = 0;
         for (int index = 1; index <= 20; ++index) {  // …while the GROUP fades
           groupFade = 1.0f - 0.045f * (float)index;
           on.frame();
           off.frame();
           ++fadeFrames;
           bakes += on.composer.stats().texturesBaked;
           const Composer::NodeCost* frameRow =
               requireRow(on.composer, "lattice");
           if (frameRow && frameRow->cacheState == Composer::CacheState::Group)
             ++blits;
           ASSERT_TRUE(identicalPixels(off, on, 240, 240))
               << "the group's own opacity composited differently through the "
                  "blit "
                  "than through the live paint, at frame "
               << index;
         }
         EXPECT_EQ(blits, fadeFrames) << "the group dropped its bake for its "
                                         "OWN fade, which the bake does "
                                         "not contain — the root's paint slots "
                                         "are in the memo and must not "
                                         "be";
         EXPECT_EQ(bakes, 0u) << "the group re-baked during its own fade";
       }},
      {"AMovingGroupRidesOneLocalBakeRatherThanRemakingIt",
       [] {
         // The other documented limit, and the one whose failure is a slowdown
         // rather than a wrong picture: a device-pinned bake remade every frame
         // costs strictly more than the paint it replaces, so a group whose own
         // transform is declared as moving holds its settled subtree as ONE
         // local bake and blits it through the motion, rather than remaking a
         // device bake at every position.
         static sigil::motion::Animatable<float> slide =
             sigil::motion::animatable(0.0f);
         Host host(240, 240);
         host.composer.setProfiling(true);
         setBoardPhase(kGroupDone + 0.4);
         slide = 0.0f;
         Element group = lattice(Cache::Group).translateX(slide);
         host.composer.render(
             stack().children({box()
                                   .cache(Cache::None)
                                   .absolute()
                                   .left(0)
                                   .top(0)
                                   .width(240)
                                   .height(240)
                                   .children({std::move(group)})}));
         for (int index = 0; index < 6; ++index) host.frame();
         size_t bakes = 0;
         for (int index = 1; index <= 20; ++index) {
           slide = 0.7f * (float)index;
           host.frame();
           bakes += host.composer.stats().texturesBaked;
         }
         EXPECT_EQ(bakes, 0u)
             << "the group re-baked while it was sliding: 20 frames, " << bakes
             << " bakes";
         const Composer::NodeCost* row = requireRow(host.composer, "lattice");
         ASSERT_NE(row, nullptr);
         EXPECT_EQ(row->cacheState, Composer::CacheState::Group);
       }},
  });
}
