// The routers: how a derived route gets from one node to another, and
// which layout scheme a router's own bends belong to.

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Decorations.h>

#include "support/ShapeTestSupport.h"

TEST(ComposeRouters, OrbitFollowsTheRing) {
  const SkPoint center{100, 100};
  RailRouter router = routers::orbit(center);
  const SkPoint pts[2] = {{200, 100}, {100, 200}};
  const SkPath path = router(std::span<const SkPoint>(pts, 2));
  SkContourMeasureIter iter(path, false);
  sk_sp<SkContourMeasure> contour = iter.next();
  ASSERT_TRUE(contour);
  // Quarter circle r=100: length ~157 (a chord would be ~141), and the
  // midpoint sits ON the ring.
  EXPECT_NEAR(contour->length(), 157.1f, 3.0f);
  SkPoint mid;
  ASSERT_TRUE(contour->getPosTan(contour->length() / 2, &mid, nullptr));
  EXPECT_NEAR(SkPoint::Distance(mid, center), 100.0f, 1.5f);
}

TEST(ComposeRouters, ARedescribedRouteRecordsOnce) {
  // A router is a VALUE: two describes that mint the same stock router
  // are the same description, so the connector prunes and replays the
  // recording it already made. Minted afresh each describe on purpose —
  // that is the shape every author writes, and the shape a callable
  // could never settle from.
  const auto page = [] {
    return box()
        .width(300)
        .height(200)
        .child(box()
                   .key("a")
                   .absolute()
                   .left(Dim(10.0f))
                   .top(Dim(10.0f))
                   .width(20)
                   .height(20)
                   .fill(red()))
        .child(box()
                   .key("b")
                   .absolute()
                   .left(Dim(200.0f))
                   .top(Dim(150.0f))
                   .width(20)
                   .height(20)
                   .fill(blue()))
        .child(connector("a", "b",
                         routers::orthogonal(routers::Bend::VFirst, 6.0f))
                   .key("wire")
                   .absolute()
                   .inset(0)
                   .foreground(PathFormat{.width = 2, .strokeFill = green()}));
  };
  Host host;
  host.composer.render(page());
  host.frame();
  const unsigned recorded = host.composer.stats().picturesRecorded;
  EXPECT_GE(recorded, 1u);  // the first describe drew it
  host.composer.render(page());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an unchanged router re-patched the connector";
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
      << "an unchanged router re-recorded the route";
  // …and a router with different parameters is a different description.
  host.composer.render(
      box()
          .width(300)
          .height(200)
          .child(box()
                     .key("a")
                     .absolute()
                     .left(Dim(10.0f))
                     .top(Dim(10.0f))
                     .width(20)
                     .height(20)
                     .fill(red()))
          .child(box()
                     .key("b")
                     .absolute()
                     .left(Dim(200.0f))
                     .top(Dim(150.0f))
                     .width(20)
                     .height(20)
                     .fill(blue()))
          .child(
              connector("a", "b",
                        routers::orthogonal(routers::Bend::VFirst, 18.0f))
                  .key("wire")
                  .absolute()
                  .inset(0)
                  .foreground(PathFormat{.width = 2, .strokeFill = green()})));
  host.frame();
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "a router with a different radius pruned";
}

TEST(ComposeRouters, ARawRouteCallableNeverSettles) {
  // The escape hatch, stated as a test so the difference is visible: a
  // lambda re-minted each describe compares equal to nothing.
  Router held = [](const SkRect& from, const SkRect& to) {
    SkPathBuilder b;
    b.moveTo(from.centerX(), from.centerY());
    b.lineTo(to.centerX(), to.centerY());
    return b.detach();
  };
  EXPECT_FALSE(held.comparable());
  EXPECT_EQ(held, held);  // copies of ONE value share state
  EXPECT_TRUE(routers::straight().comparable());
  EXPECT_EQ(routers::straight(), routers::straight());
  EXPECT_NE(routers::arc(0.2f), routers::arc(0.3f));
  EXPECT_NE(routers::straight(), routers::arc(0.2f));
  EXPECT_EQ(routers::octilinear(8), routers::octilinear(8));
  EXPECT_NE(routers::octilinear(8), routers::polyline(8));
  EXPECT_EQ(routers::fromPairwise(routers::arc(0.2f)),
            routers::fromPairwise(routers::arc(0.2f)));
  EXPECT_NE(routers::fromPairwise(routers::arc(0.2f)),
            routers::fromPairwise(routers::arc(0.4f)));
  EXPECT_EQ(RailRouter{}, RailRouter{});  // the default: rail()'s polyline
}

TEST(ComposeLayouts, AbsoluteDiagonalAutoSizes) {
  // A Diagonal container sizes itself from the extent of what it placed, so
  // an author does not have to compute the skewed bounding box by hand.
  Host host;
  host.composer.render(
      box().child(Element(layout(layouts::Diagonal{.skewDeg = -20, .gap = 10}))
                      .key("battery")
                      .absolute()
                      .left(Dim(30.0f))
                      .top(Dim(20.0f))
                      .child(box().width(80).height(24).fill(red()))
                      .child(box().width(80).height(24).fill(blue()))
                      .child(box().width(80).height(24).fill(green()))));
  host.frame();
  auto b = host.composer.bounds("battery");
  ASSERT_TRUE(b.has_value());
  // Three rows: height 3*24 + 2*10 = 92; x-drift = tan(20°)*68 ≈ 24.7 +
  // 80 wide rows → width ≈ 104.7.
  EXPECT_NEAR(b->height(), 92, 1.0f);
  EXPECT_NEAR(b->width(), 104.7f, 2.0f);
}

namespace {

/** The path's line-verb skeleton: every on-curve point in order, with the
 *  verb census alongside — the geometry assertions below read this. */
struct PathDump {
  std::vector<SkPoint> pts;
  int moves = 0, lines = 0, curves = 0, closes = 0;
};

PathDump dumpPath(const SkPath& p) {
  PathDump d;
  SkPath::Iter it(p, false);
  SkPoint v[4];
  for (SkPath::Verb verb; (verb = it.next(v)) != SkPath::kDone_Verb;) {
    switch (verb) {
      case SkPath::kMove_Verb:
        ++d.moves;
        d.pts.push_back(v[0]);
        break;
      case SkPath::kLine_Verb:
        ++d.lines;
        d.pts.push_back(v[1]);
        break;
      case SkPath::kQuad_Verb:
      case SkPath::kConic_Verb:
        ++d.curves;
        d.pts.push_back(v[2]);
        break;
      case SkPath::kCubic_Verb:
        ++d.curves;
        d.pts.push_back(v[3]);
        break;
      case SkPath::kClose_Verb:
        ++d.closes;
        break;
      default:
        break;
    }
  }
  return d;
}

}  // namespace

TEST(ComposeRouters, ManhattanIsARailRouterAndCollapsesCollinearRuns) {
  // rail() takes a RailRouter, and orthogonal() is a pairwise Router — so
  // orthogonal routing was unreachable from a rail at all. This line
  // compiling is half of what is being checked.
  RailRouter router = routers::manhattan();

  // An axis-aligned pair: ONE segment, no zero-length verbs.
  const SkPoint aligned[2] = {{20, 100}, {180, 100}};
  PathDump collapsed = dumpPath(router(std::span(aligned, 2)));
  EXPECT_EQ(collapsed.moves, 1);
  EXPECT_EQ(collapsed.lines, 1);
  ASSERT_EQ(collapsed.pts.size(), 2u);
  EXPECT_EQ(collapsed.pts[0], SkPoint::Make(20, 100));
  EXPECT_EQ(collapsed.pts[1], SkPoint::Make(180, 100));

  // Three collinear anchors thread as ONE straight run.
  const SkPoint three[3] = {{20, 100}, {100, 100}, {180, 100}};
  PathDump merged = dumpPath(router(std::span(three, 3)));
  EXPECT_EQ(merged.lines, 1);
  ASSERT_EQ(merged.pts.size(), 2u);
  EXPECT_EQ(merged.pts[1], SkPoint::Make(180, 100));

  // The contrast: collapsing collinear runs is manhattan()'s own promise,
  // not orthogonal()'s. orthogonal() bends at the midpoint and keeps every
  // leg it made, degenerate or not -- harmless, since Skia's stroker skips
  // exactly-degenerate segments -- so it answers the same pair with a bend
  // where manhattan() answers with one run.
  Router pairwise = routers::orthogonal();
  PathDump bent = dumpPath(pairwise(SkRect::MakeXYWH(10, 90, 20, 20),
                                    SkRect::MakeXYWH(170, 90, 20, 20)));
  EXPECT_GT(bent.lines, collapsed.lines)
      << "orthogonal() collapsed its legs, so manhattan()'s promise to do "
         "so says nothing";
  ASSERT_GE(bent.pts.size(), 2u);
  EXPECT_EQ(bent.pts[1], SkPoint::Make(100, 100)) << "the bend is at midX";
}

TEST(ComposeRouters, BendPoliciesTakeTheNamedColumns) {
  const SkPoint run[2] = {{20, 20}, {180, 160}};
  // HFirst: horizontal out of the source, with the L bending AT the target
  // column — the shape a circuit-style graph wants and the midpoint router
  // cannot produce.
  PathDump h =
      dumpPath(routers::manhattan(routers::Bend::HFirst)(std::span(run, 2)));
  ASSERT_EQ(h.pts.size(), 3u);
  EXPECT_EQ(h.pts[1], SkPoint::Make(180, 20));
  // VFirst: the other L, down the source column first.
  PathDump v =
      dumpPath(routers::manhattan(routers::Bend::VFirst)(std::span(run, 2)));
  ASSERT_EQ(v.pts.size(), 3u);
  EXPECT_EQ(v.pts[1], SkPoint::Make(20, 160));
  // MidX stays the stock Z, bending half way over.
  PathDump z =
      dumpPath(routers::manhattan(routers::Bend::MidX)(std::span(run, 2)));
  ASSERT_EQ(z.pts.size(), 4u);
  EXPECT_EQ(z.pts[1], SkPoint::Make(100, 20));
  EXPECT_EQ(z.pts[2], SkPoint::Make(100, 160));
  // The pairwise spelling routes the same shape from rects.
  PathDump hr = dumpPath(routers::orthogonal(routers::Bend::HFirst)(
      SkRect::MakeXYWH(15, 15, 10, 10), SkRect::MakeXYWH(175, 155, 10, 10)));
  ASSERT_EQ(hr.pts.size(), 3u);
  EXPECT_EQ(hr.pts[1], SkPoint::Make(180, 20));
}

TEST(ComposeRouters, AStampedRouteCarriesAWholeCountOfTilesPerLeg) {
  // A route dressed with a stamped brush is not a line the marks are laid
  // over: the marks ARE the route. `Stamp` is what makes a leg an exact
  // number of them — the turn lands on a whole count from the leg's
  // start, and each end gives up room so the first tile stands clear of
  // whatever the route leaves.
  constexpr float kCell = 24.0f;
  const auto stamped =
      routers::manhattan(routers::Bend::VFirst, 0.0f, 0.0f,
                         {.advance = kCell, .endInset = kCell * 0.5f});
  // An end three px OFF the count: the plain router turns at the far row,
  // the stamped one turns at the nearest whole number of cells from the
  // start, which is what gives the leg an exact tile count.
  const SkPoint off[2] = {{20, 20}, {116, 137}};  // 4 cells over, ~4.9 down
  const PathDump plain =
      dumpPath(routers::manhattan(routers::Bend::VFirst)(std::span(off, 2)));
  ASSERT_EQ(plain.pts.size(), 3u);
  EXPECT_EQ(plain.pts[1], SkPoint::Make(20, 137));
  const PathDump quantised = dumpPath(stamped(std::span(off, 2)));
  ASSERT_EQ(quantised.pts.size(), 3u);
  EXPECT_EQ(quantised.pts[1], SkPoint::Make(20, 20 + 5 * kCell));

  // On the count, which is what a route between grid-placed nodes is:
  // both legs are axis-aligned and each END gives up half a cell along
  // its own leg.
  const SkPoint run[2] = {{20, 20}, {116, 140}};  // 4 cells over, 5 down
  const PathDump onGrid = dumpPath(stamped(std::span(run, 2)));
  ASSERT_EQ(onGrid.pts.size(), 3u);
  EXPECT_EQ(onGrid.pts[1], SkPoint::Make(20, 140));
  EXPECT_EQ(onGrid.pts.front(), SkPoint::Make(20, 20 + kCell * 0.5f));
  EXPECT_EQ(onGrid.pts.back(), SkPoint::Make(116 - kCell * 0.5f, 140));

  // A STRAIGHT run collapses first and is then inset from both ends of
  // the one segment it became, so a stamped axis-aligned route is still
  // one segment and not three.
  const SkPoint flat[2] = {{20, 20}, {212, 20}};
  const PathDump line = dumpPath(stamped(std::span(flat, 2)));
  ASSERT_EQ(line.pts.size(), 2u);
  EXPECT_EQ(line.pts.front(), SkPoint::Make(20 + kCell * 0.5f, 20));
  EXPECT_EQ(line.pts.back(), SkPoint::Make(212 - kCell * 0.5f, 20));

  // The default stamp is no stamp: an unstated route is what it was.
  EXPECT_EQ(routers::manhattan(routers::Bend::VFirst),
            routers::manhattan(routers::Bend::VFirst, 0.0f, 0.0f, {}));
  EXPECT_NE(routers::manhattan(routers::Bend::VFirst),
            routers::manhattan(routers::Bend::VFirst, 0.0f, 0.0f,
                               {.advance = kCell}));
}

TEST(ComposeRouters, FromPairwiseStitchesOneContourAndKeepsCurves) {
  // The adapter: any pairwise Router rides rail(). Three stations, the
  // legs stitch into ONE contour (terminal caps fire once, junction
  // moves dropped) and the old router's zero-length verbs collapse.
  const SkPoint stops[3] = {{20, 100}, {100, 100}, {100, 180}};
  RailRouter rr = routers::fromPairwise(routers::orthogonal());
  const SkPath path = rr(std::span(stops, 3));
  PathDump d = dumpPath(path);
  EXPECT_EQ(d.moves, 1);  // ONE contour, not one per pair
  ASSERT_GE(d.pts.size(), 2u);
  EXPECT_EQ(d.pts.front(), SkPoint::Make(20, 100));
  EXPECT_EQ(d.pts.back(), SkPoint::Make(100, 180));
  for (size_t i = 1; i < d.pts.size(); ++i)  // every segment has length
    EXPECT_NE(d.pts[i], d.pts[i - 1]);
  // Collinear merge across the stitch: both legs of the first pair run
  // y=100, so the horizontal approach is one segment.
  EXPECT_EQ(d.lines, 2);
  // A curved router survives the adapter with its curves intact.
  PathDump arc =
      dumpPath(routers::fromPairwise(routers::arc(0.3f))(std::span(stops, 3)));
  EXPECT_EQ(arc.moves, 1);
  EXPECT_GT(arc.curves, 0);
}
