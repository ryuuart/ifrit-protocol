// The shape binary's share of ComposeTestLines.cpp: the suites whose subjects
// are shape-tier values, cut from that file so each test binary links only the
// target it exercises.

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

  // The contrast, frozen deliberately: the zero-argument orthogonal() keeps
  // its degenerate verbs — a move, then THREE lines, two of them
  // zero-length. Those verbs are harmless in practice (Skia's stroker skips
  // exactly-degenerate segments, and the render is byte-identical to clean
  // geometry), so its output is pinned as-is and only manhattan() collapses.
  // Changing it would move pixels for no benefit.
  Router old = routers::orthogonal();
  PathDump frozen = dumpPath(
      old(SkRect::MakeXYWH(10, 90, 20, 20), SkRect::MakeXYWH(170, 90, 20, 20)));
  EXPECT_EQ(frozen.lines, 3);
  ASSERT_EQ(frozen.pts.size(), 4u);
  EXPECT_EQ(frozen.pts[1], SkPoint::Make(100, 100));  // midX
  EXPECT_EQ(frozen.pts[2], SkPoint::Make(100, 100));  // zero-length V leg
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
