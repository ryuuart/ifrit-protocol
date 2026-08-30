#include "../ComposeInternal.h"  // propsEqual — the comparator, taken directly
#include "support/CoreTestSupport.h"

namespace {

/** A 100×100 box at (20,20) whose boundary is the ring `boundaryRing`
 *  samples, dressed with one red stroke. The masking family's fixture. */
Element maskBox() { return box().rect(SkRect::MakeXYWH(20, 20, 100, 100)); }

/** How much red ink is anywhere in a 200×200 host. */
int redInk(Host& host, int x0 = 0, int y0 = 0, int x1 = 200, int y1 = 200) {
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x)
      if (SkColorGetR(host.pixel(x, y)) > 140 &&
          SkColorGetG(host.pixel(x, y)) < 90)
        ++n;
  return n;
}

}  // namespace

// ---- S6 · the directional wipe, over a lattice of children ---------------

TEST(ComposeR4Mask, S6TheEdgeGateReachesTheChildren) {
  // chevreul_circle's twelve grounds arrive and withdraw as a downward
  // wipe. The CHILDREN are the point — an arc-length window has nothing to
  // say about them, and this is the sample that says the family needs more
  // than one gate kind.
  const auto lattice = [](float t) {
    Element g = box().absolute().left(20).top(20).width(160).height(160).mask(
        by::edge(90.0f, t));
    for (int i = 0; i < 4; ++i)
      g.child(box()
                  .absolute()
                  .left(0)
                  .top((float)i * 40)
                  .width(160)
                  .height(36)
                  .fill(red()));
    return stack().child(std::move(g));
  };
  Host half(200, 200), whole(200, 200);
  half.composer.render(lattice(0.5f));
  half.frame();
  whole.composer.render(lattice(1.0f));
  whole.frame();
  EXPECT_GT(redInk(half, 0, 20, 200, 95), 1000) << "the top half arrived";
  EXPECT_EQ(redInk(half, 0, 105, 200, 190), 0) << "the bottom half has not";
  EXPECT_GT(redInk(whole, 0, 105, 200, 190), 1000) << "…and at 1 it has";
}

// ---- S7 · the seal: a region gate, and its complement --------------------

TEST(ComposeR4Mask, S7TheShapeGateAndItsComplementAreBothTerms) {
  // A portrait masked to a wax-seal silhouette. Nothing in the tree could
  // express this: a study reached for `clipOut()` and `shapes::subtract` BY
  // NAME, found neither, and dropped below the Compose seam to a raw
  // SkPathOp. Both halves are terms here, and two masks INTERSECT — so a
  // set difference is one node and two lines.
  const SkRect seal = SkRect::MakeXYWH(20, 20, 60, 60);
  Host inside(200, 200), outside(200, 200), diff(200, 200);
  const auto plate = [] {
    return box().absolute().left(20).top(20).width(100).height(100).fill(red());
  };
  inside.composer.render(
      stack().child(plate().mask(by::shape(Region::rect(seal)))));
  inside.frame();
  outside.composer.render(
      stack().child(plate().mask(by::outside(Region::rect(seal)))));
  outside.frame();
  // The gate is stated in the node's LOCAL space, so the seal covers
  // (40,40)-(100,100) on the canvas.
  EXPECT_GT(SkColorGetR(inside.pixel(70, 70)), 180) << "kept inside";
  EXPECT_EQ(inside.pixel(110, 110), SK_ColorBLACK) << "…and only inside";
  EXPECT_EQ(outside.pixel(70, 70), SK_ColorBLACK) << "the complement";
  EXPECT_GT(SkColorGetR(outside.pixel(110, 110)), 180);
  // THE SET DIFFERENCE: inside one region AND outside another, which is
  // the picture the raw SkPathOp was written for.
  diff.composer.render(stack().child(
      plate()
          .mask(by::shape(Region::rect(SkRect::MakeXYWH(0, 0, 80, 80))))
          .mask(by::outside(Region::rect(SkRect::MakeXYWH(0, 0, 40, 40))))));
  diff.frame();
  EXPECT_EQ(diff.pixel(35, 35), SK_ColorBLACK) << "cut out of the middle";
  EXPECT_GT(SkColorGetR(diff.pixel(70, 70)), 180) << "inside the outer";
  EXPECT_EQ(diff.pixel(110, 110), SK_ColorBLACK) << "outside the outer";
}

TEST(ComposeR4Mask, S7bTheAlphaGateTakesItsCoverageFromAMaterial) {
  // …and SOFT-EDGED, which is the other half of what a coverage gate is
  // for. Without it, the only way to fade a node by a gradient is to hand-
  // roll a Material plus a kDstIn layer at every call site.
  Host host(200, 200);
  host.composer.render(stack().child(
      box().absolute().left(20).top(20).width(160).height(160).fill(red()).mask(
          by::alpha(Material::linear(
              {0, 0}, {160, 0},
              {{0.0f, {1, 1, 1, 1}}, {1.0f, {1, 1, 1, 0}}})))));
  host.frame();
  // Opaque at the left of the ramp, gone at the right, monotone between.
  EXPECT_GT(SkColorGetR(host.pixel(25, 100)), 200);
  EXPECT_LT(SkColorGetR(host.pixel(175, 100)), 40);
  EXPECT_GT(SkColorGetR(host.pixel(60, 100)),
            SkColorGetR(host.pixel(140, 100)));
}

namespace {

/** Five white plates side by side, each masked by its own coverage gate.
 *  White over the black clear colour, so the byte a plate shows IS its
 *  coverage times 255 and nothing else is in the arithmetic. */
Element coveragePlates(const std::vector<Gate>& gates) {
  Element root = stack();
  int i = 0;
  for (const Gate& g : gates) {
    root.child(box()
                   .absolute()
                   .left(10.0f + 38.0f * (float)i)
                   .top(20)
                   .width(30)
                   .height(160)
                   .fill(Fill::color({1, 1, 1, 1}))
                   .mask(g));
    ++i;
  }
  return root;
}

int plateByte(Host& host, int i) {
  return (int)SkColorGetR(host.pixel(25 + 38 * i, 100));
}

}  // namespace

TEST(ComposeR4Mask, S7cTheLumaGateIsRec601OnEncodedPremultipliedValues) {
  // THE PIXEL PIN FOR THE LUMA LAW, and it is deliberately not pinned with
  // greys: a grey pins NOTHING about the coefficients (every weighting of
  // equal channels is the same number), and the primaries alone pin nothing
  // about the transfer function (0 and 1 are the sRGB curve's fixed points,
  // the trap `RendersClearColorWhenEmpty` fell into one library over).
  //
  // Five plates, five different wrong answers ruled out:
  //
  //   matte           Rec.601   Rec.709   any weights   unpremultiplied
  //                   encoded   encoded   on LINEAR     luma
  //   pure red         76        54        76            76
  //   pure green      150       182       150           150
  //   pure blue        29        18        29            29
  //   50% grey        128       128        55           128
  //   50% white       128       128        55           255
  //
  // Column 1 is the law (by::luma). Column 2 is the Poynton mistake —
  // LUMINANCE coefficients, which are defined on linear light, applied to
  // encoded values. Column 3 is linearising first, which compose has no
  // stage for: its surfaces carry NO colour space, so a shader's channels
  // are the display-encoded numbers the author wrote. Column 4 ignores the
  // matte's alpha; premultiplied is what makes a transparent matte read as
  // black, the way After Effects' luma matte does.
  Host host(200, 200);
  host.composer.render(coveragePlates({
      by::luma(Material::solid({1, 0, 0, 1})),
      by::luma(Material::solid({0, 1, 0, 1})),
      by::luma(Material::solid({0, 0, 1, 1})),
      by::luma(Material::solid({0.5f, 0.5f, 0.5f, 1})),
      by::luma(Material::solid({1, 1, 1, 0.5f})),
  }));
  host.frame();
  EXPECT_NEAR(plateByte(host, 0), 76, 2) << "red is 0.299, not 0.2126 (Rec.709 "
                                            "luminance) and not 1/3";
  EXPECT_NEAR(plateByte(host, 1), 150, 2) << "green is 0.587, not 0.7152";
  EXPECT_NEAR(plateByte(host, 2), 29, 2) << "blue is 0.114, not 0.0722";
  EXPECT_NEAR(plateByte(host, 3), 128, 2)
      << "50% grey weighs 0.5 — the luma is taken on the ENCODED value. A "
         "linearised reading gives 0.214 and this plate reads 55";
  EXPECT_NEAR(plateByte(host, 4), 128, 2)
      << "half-transparent white IS 50% grey: the luma is premultiplied, so "
         "a transparent matte reads as black and hides";
}

TEST(ComposeR4Mask, S7cTheLumaLawIsTheSameThroughAShader) {
  // The colour path and the shader path are two implementations of one law
  // (a dot product in C++, an SkSL pass over the coverage layer), which is
  // exactly the shape of asymmetry that ships wrong. One ramp, green to
  // blue, checked at both ends and in the middle:
  //   left  (0,1,0) -> 0.587          -> 150
  //   mid   (0,.5,.5) -> .2935+.057   ->  89
  //   right (0,0,1) -> 0.114          ->  29
  Host host(200, 200);
  host.composer.render(
      stack().child(box()
                        .absolute()
                        .left(20)
                        .top(20)
                        .width(160)
                        .height(160)
                        .fill(Fill::color({1, 1, 1, 1}))
                        .mask(by::luma(Material::linear(
                            {0, 0}, {160, 0},
                            {{0.0f, {0, 1, 0, 1}}, {1.0f, {0, 0, 1, 1}}})))));
  host.frame();
  EXPECT_NEAR((int)SkColorGetR(host.pixel(22, 100)), 150, 4) << "green end";
  EXPECT_NEAR((int)SkColorGetR(host.pixel(100, 100)), 89, 4) << "the mix";
  EXPECT_NEAR((int)SkColorGetR(host.pixel(177, 100)), 29, 4) << "blue end";
}

TEST(ComposeR4Mask, S7dEachCoverageGateHasItsComplementAsItsOwnTerm) {
  // `by::outside(r)` made the region complement a TERM rather than a mode
  // flag; the coverage sources get the same treatment and the same law —
  // a gate is a SHOW set, and the complement shows exactly the rest.
  // Mechanically it is kDstOut instead of kDstIn, which is dst·(1-a): the
  // pair of plates must sum to 255 at every sample, not merely differ.
  Host host(200, 200);
  const Material ramp = Material::solid({0.5f, 0.5f, 0.5f, 0.25f});
  host.composer.render(coveragePlates({
      by::alpha(ramp),     // 0.25            ->  64
      by::alphaOut(ramp),  // 1 - 0.25        -> 191
      by::luma(ramp),      // 0.25 * 0.5      ->  32
      by::lumaOut(ramp),   // 1 - 0.125       -> 223
  }));
  host.frame();
  EXPECT_NEAR(plateByte(host, 0), 64, 2);
  EXPECT_NEAR(plateByte(host, 1), 191, 2) << "alphaOut is not alpha";
  EXPECT_EQ(plateByte(host, 0) + plateByte(host, 1), 255)
      << "the alpha complement must show exactly the rest";
  EXPECT_NEAR(plateByte(host, 2), 32, 2);
  EXPECT_NEAR(plateByte(host, 3), 223, 2) << "lumaOut is not luma";
  EXPECT_EQ(plateByte(host, 2) + plateByte(host, 3), 255)
      << "…and so must the luma complement";
}

TEST(ComposeR4Mask, ACoverageGatesChannelAndSenseReachTheComparator) {
  // Taken against propsEqual() DIRECTLY. A harness that renders two trees
  // and counts patches can pass while the comparator is broken, because
  // keyed siblings never prune into one another.
  //
  // `outside` is the field to worry about here. It is compared in the Shape
  // arm already, so the compile-time field pin cannot notice the Coverage
  // arm ignoring it — and a matte that compares equal to its own INVERSE
  // prunes and stays showing the wrong half for as long as the node lives.
  const Material m = Material::solid({0.5f, 0.5f, 0.5f, 1});
  const auto node = [&](Gate g) {
    Element el = box().mask(std::move(g));
    return el.node();
  };
  const auto same = [&](Gate a, Gate b) {
    return detail::propsEqual(*node(std::move(a)), *node(std::move(b)));
  };
  EXPECT_TRUE(same(by::alpha(m), by::alpha(m))) << "a static matte prunes";
  EXPECT_TRUE(same(by::lumaOut(m), by::lumaOut(m)));
  EXPECT_FALSE(same(by::alpha(m), by::alphaOut(m)))
      << "a matte compares equal to its own INVERSE — `outside` is missing "
         "from the Coverage arm of Gate::operator==";
  EXPECT_FALSE(same(by::luma(m), by::lumaOut(m))) << "…and for luma";
  EXPECT_FALSE(same(by::alpha(m), by::luma(m)))
      << "the two channels compare equal — `channel` is missing from the "
         "Coverage arm of Gate::operator==";
  EXPECT_FALSE(same(by::alphaOut(m), by::lumaOut(m)));
}

TEST(ComposeR4Mask, S8PlusThreeMasksAtThreeRatesIntersectPerFrame) {
  // Masks whose selections overlap INTERSECT, and each carries its OWN
  // animation, so three masks can run at three rates on one node. Sharing
  // one animation slot would make the second gate retarget the first, and
  // the result would be a race rather than a picture; maskAnims is indexed
  // per mask, which is what keeps them independent.
  choreograph::Output<float> slow{1.0f}, fast{1.0f};
  Host host(200, 200);
  host.composer.render(stack().child(
      box()
          .absolute()
          .left(20)
          .top(20)
          .width(160)
          .height(160)
          .fill(red())
          .mask(by::edge(0.0f, &fast))    // from the left
          .mask(by::edge(180.0f, &slow))  // …and from the right
          .mask(by::shape(Region::rect(SkRect::MakeXYWH(0, 40, 160, 80))))));
  host.frame();
  // All three open: the band the shape gate leaves is fully lit.
  EXPECT_GT(redInk(host, 25, 65, 175, 155), 8000);
  EXPECT_EQ(redInk(host, 0, 0, 200, 58), 0) << "the shape gate holds";
  EXPECT_EQ(redInk(host, 0, 142, 200, 200), 0);

  // Now drive them at DIFFERENT rates and pin the intersection at pixels:
  // fast shows [left, 0.25], slow shows [0.5 from the right, right] — the
  // two half-planes are disjoint, so their intersection is EMPTY, and no
  // single-gate implementation can produce that answer.
  fast = 0.25f;
  slow = 0.25f;
  host.frame();
  EXPECT_EQ(redInk(host), 0) << "disjoint half-planes intersect to nothing";

  // …and where they DO overlap, only the overlap paints.
  fast = 0.75f;  // shows x in [20, 140]
  slow = 0.75f;  // shows x in [60, 180]
  host.frame();
  EXPECT_EQ(redInk(host, 20, 60, 55, 150), 0) << "left of the slow edge";
  EXPECT_GT(redInk(host, 70, 65, 130, 150), 2000) << "the overlap paints";
  EXPECT_EQ(redInk(host, 145, 60, 180, 150), 0) << "right of the fast edge";
}

// ---- Region is a VALUE ---------------------------------------------------

TEST(ComposeR4Mask, RegionIsComparableFromDayOne) {
  // The shape gate's obvious signature takes an OutlineFn — an
  // incomparable std::function, whose node never prunes and therefore never
  // caches. Region is a closed, comparable value instead, which is what lets
  // a shape gate sit on a node without disabling every cache above it.
  EXPECT_TRUE(Region::own() == Region::own());
  EXPECT_TRUE(Region::rect(SkRect::MakeWH(4, 4)) ==
              Region::rect(SkRect::MakeWH(4, 4)));
  EXPECT_FALSE(Region::rect(SkRect::MakeWH(4, 4)) ==
               Region::rect(SkRect::MakeWH(4, 5)));
  EXPECT_FALSE(Region::rect(SkRect::MakeWH(4, 4)) ==
               Region::oval(SkRect::MakeWH(4, 4)));
  EXPECT_FALSE(Region::own() == Region::rect(SkRect::MakeWH(4, 4)));
  SkPathBuilder a, b;
  a.addRect(SkRect::MakeWH(3, 3));
  b.addRect(SkRect::MakeWH(3, 3));
  EXPECT_TRUE(Region::path(a.detach()) == Region::path(b.detach()));
  // …and the gates built from them compare, which is what the reconciler
  // actually asks.
  EXPECT_TRUE(by::shape(Region::own()) == by::shape(Region::own()));
  EXPECT_FALSE(by::shape(Region::own()) == by::outside(Region::own()));
  EXPECT_FALSE(by::edge(0.0f, 0.5f) == by::edge(90.0f, 0.5f));
  EXPECT_FALSE(by::edge(0.0f, 0.5f) == by::edge(0.0f, 0.6f));
  EXPECT_FALSE(by::spans(spans::upTo(0.4f)) == by::edge(0.0f, 0.4f));
  EXPECT_TRUE(by::spans(spans::upTo(0.4f)) == by::spans(spans::upTo(0.4f)));
  // Parts too: a selection is a value that can be compared and inspected,
  // not an opaque predicate.
  EXPECT_TRUE(parts::marks() == parts::marks());
  EXPECT_FALSE(parts::marks() == parts::surface());
  EXPECT_FALSE(parts::named("a") == parts::named("b"));
  EXPECT_TRUE((parts::surface() | parts::content() | parts::children() |
               parts::marks()) == parts::all());
}

// ---- the positioned leaf set --------------------------------------------

TEST(ComposePositioned, RectsAreHonoredAndYogaFree) {
  Host host;
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(box().key("a").left(10).top(20).width(50).height(30).fill(
              green()))
          .child(box().key("b").left(70).top(90).width(40).height(40).fill(
              Fill::color({1, 0, 0, 1}))));
  host.frame();
  const auto a = host.composer.bounds("a");
  const auto b = host.composer.bounds("b");
  ASSERT_TRUE(a && b);
  EXPECT_EQ(*a, SkRect::MakeXYWH(10, 20, 50, 30));
  EXPECT_EQ(*b, SkRect::MakeXYWH(70, 90, 40, 40));
  EXPECT_EQ(host.pixel(30, 30), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(90, 110), SK_ColorRED);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);
  // The whole point: root + container carry Yoga nodes, the leaves do not.
  const Composer::Stats& stats = host.composer.stats();
  EXPECT_EQ(stats.instances, 3u);
  EXPECT_EQ(stats.yogaNodes, 1u);
}

TEST(ComposePositioned, NestedRectsComposeYogaFreeAllTheWayDown) {
  Host host;
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(
              box().key("outer").left(40).top(40).width(100).height(100).child(
                  box().key("inner").left(10).top(20).width(30).height(30))));
  host.frame();
  const auto inner = host.composer.bounds("inner");
  ASSERT_TRUE(inner);
  // bounds() is absolute: outer's origin + inner's own rect.
  EXPECT_EQ(*inner, SkRect::MakeXYWH(50, 60, 30, 30));
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  EXPECT_EQ(host.composer.stats().instances, 3u);
}

TEST(ComposePositioned, PctAndOpposingInsetsResolve) {
  Host host;  // 200x200 canvas; container fills it
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          // pct dims resolve against the container's rect
          .child(
              box().key("half").left(0).top(0).width(pct(50)).height(pct(25)))
          // open width + right inset pins the far edge
          .child(box().key("pinned").left(20).top(100).right(30).height(10)));
  host.frame();
  const auto half = host.composer.bounds("half");
  const auto pinned = host.composer.bounds("pinned");
  ASSERT_TRUE(half && pinned);
  EXPECT_EQ(*half, SkRect::MakeXYWH(0, 0, 100, 50));
  EXPECT_EQ(*pinned, SkRect::MakeXYWH(20, 100, 150, 10));
}

TEST(ComposePositioned, TextMeasuresAgainstItsSuppliedWidth) {
  Host host;
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 14;
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(text(u8"wrap me across a narrow measure", style)
                     .key("t")
                     .left(10)
                     .top(10)
                     .width(90)));
  host.frame();
  const auto t = host.composer.bounds("t");
  ASSERT_TRUE(t);
  EXPECT_EQ(t->left(), 10);
  EXPECT_EQ(t->width(), 90);
  // Height is left open, so it comes from measurement: a wrapped run is
  // taller than a single line.
  EXPECT_GT(t->height(), 20.0f);
}

TEST(ComposePositioned, StructuralPruneAndMovesStillWork) {
  Host host;
  auto tree = [](float x) {
    return positioned()
        .inset(0, 0, 0, 0)
        .child(
            box().key("m").left(x).top(10).width(40).height(40).fill(green()));
  };
  host.composer.render(tree(10));
  host.frame();
  host.composer.render(tree(10));  // identical: prune, no cache writes
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(120));  // moved: repaints, and the rect follows
  host.frame();
  EXPECT_EQ(host.pixel(140, 30), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLACK);
  const auto m = host.composer.bounds("m");
  ASSERT_TRUE(m);
  EXPECT_EQ(m->left(), 120);
}

TEST(ComposePositioned, HitTestAndZOrderSeeChildren) {
  Host host;
  host.composer.render(positioned()
                           .inset(0, 0, 0, 0)
                           .child(box()
                                      .key("under")
                                      .left(20)
                                      .top(20)
                                      .width(60)
                                      .height(60)
                                      .fill(Fill::color({1, 0, 0, 1}))
                                      .hitTestable(true))
                           .child(box()
                                      .key("over")
                                      .left(50)
                                      .top(50)
                                      .width(60)
                                      .height(60)
                                      .zIndex(1)
                                      .fill(green())
                                      .hitTestable(true)));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorGREEN);  // zIndex 1 paints on top
  const auto hitOver = host.composer.hitTest({60, 60});
  const auto hitUnder = host.composer.hitTest({25, 25});
  ASSERT_TRUE(hitOver && hitUnder);
  EXPECT_EQ(*hitOver, "over");
  EXPECT_EQ(*hitUnder, "under");
}

TEST(ComposePositioned, TogglingPositionedRemountsCleanly) {
  Host host;
  auto child = []() {
    return box().key("c").left(10).top(10).width(30).height(30).fill(green());
  };
  host.composer.render(positioned().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  // The same child under a plain box: rejoins the Yoga world.
  host.composer.render(box().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 2u);
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);  // absolute insets agree
  // And back again.
  host.composer.render(positioned().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);
}

namespace {

/** The profile row for the node keyed `key`, from the last draw (labels
 *  are "<key> (<kind> WxH)"). */
const Composer::NodeCost* rowOf(Host& host, const char* key) {
  const std::string prefix = std::string(key) + " (";
  for (const Composer::NodeCost& row : host.composer.profile())
    if (row.label.rfind(prefix, 0) == 0) return &row;
  return nullptr;
}

}  // namespace

// ---- the memo carve-outs and the lanes they must not forget ---------------
//
// The content-volatility terms are enumerated in several places: once for
// `ownContent`, once for the group memo, and once inside each memo
// carve-out. Every copy has to name every term. A carve-out that omits, say,
// a bound fill or a live effect lets a node carrying one of those AND an
// animated gate take a memo it has no right to — and replay a recording that
// baked the old colour.
//
// This is why the terms are named once and each consumer subtracts from
// them, rather than each site listing what it cares about.

TEST(ComposeCache, ABoundFillMovingUnderAHeldGateRepaints) {
  // A node carrying BOTH a bound fill() and an animated mask gate. The
  // scalar memo holds the recording while the GATE's floats hold still — and
  // that recording baked the fill colour into it. If the bound fill is not
  // part of the memo's comparison, moving it while the gate holds replays
  // the old colour.
  choreograph::Output<float> reveal{1.0f};
  choreograph::Output<Fill> tint{Fill::color({1, 0, 0, 1})};  // red
  Host host(200, 200);
  host.composer.render(
      box().child(maskBox().fill(&tint).mask(by::spans(spans::upTo(&reveal)))));
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
  host.composer.render(box().child(
      maskBox()
          .fill(Fill::color({1, 0, 0, 1}))
          .effect(Effect::shader(fx, {{"amt", 1.0f}}).uniform("amt", &amt))
          .mask(by::spans(spans::upTo(&reveal)))));
  host.frame();
  for (int i = 0; i < 4; ++i) host.frame(0.016);
  EXPECT_GT(redInk(host, 25, 25, 115, 115), 4000) << "red to begin with";
  amt = 0.0f;  // the effect must now kill the red channel
  host.frame(0.016);
  EXPECT_LT(redInk(host, 25, 25, 115, 115), 100)
      << "the live effect moved and the node replayed a stale recording";
}

TEST(ComposeCache, ALiveEffectMovingOverAHeldMaterialRepaints) {
  // The OTHER memo, same hole: liveMatOnly holds the recording while the
  // MATERIAL's resolved shader pointer is stable, and its refusal list
  // does not mention a live effect either.
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader content; uniform float amt;"
                 "half4 main(float2 p) { half4 c = content.eval(p);"
                 "  return half4(c.r * half(amt), c.g, c.b, c.a); }"));
    if (!e) ADD_FAILURE() << err.c_str();
    return e;
  }();
  static sk_sp<SkRuntimeEffect> matfx = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform float lift;"
                 "half4 main(float2 p) { return half4(1.0, half(lift), 0.0,"
                 "                                    1.0); }"));
    if (!e) ADD_FAILURE() << err.c_str();
    return e;
  }();
  choreograph::Output<float> lift{0.0f};  // the material's own bound uniform
  choreograph::Output<float> amt{1.0f};   // the effect's
  Host host(200, 200);
  host.composer.render(box().child(
      maskBox()
          .fill(Material::sksl(matfx).uniform("lift", &lift))
          .effect(Effect::shader(fx, {{"amt", 1.0f}}).uniform("amt", &amt))));
  host.frame();
  for (int i = 0; i < 4; ++i) host.frame(0.016);
  EXPECT_GT(redInk(host, 25, 25, 115, 115), 4000) << "red to begin with";
  amt = 0.0f;  // the material holds; only the EFFECT moves
  host.frame(0.016);
  EXPECT_LT(redInk(host, 25, 25, 115, 115), 100)
      << "the live effect moved and the live-material memo replayed stale";
}
