// The read-back checks themselves: that a coverage scan and a width audit
// answer over an indexed figure exactly what they answer asked piece by
// piece and edge by edge, and that neither loses a defect to its index.
//
// The fixture is the shape both checks are hardest on and the one studies
// actually hand them: a band of hundreds of quadrilaterals, one per
// sampled step of a spine, every consecutive pair sharing an edge, all in
// one path. Every shared edge is a line standing INSIDE the ink, so a
// check that trusts an outline or stops at the first edge it meets is
// wrong here and only here.
//
// Then the shapes a bounding box cannot answer for at all: a
// subdivision whose overlap and gap cancel, an outline that is not a
// box, the arcs a contour leaves dangling, and the rasterized scene
// every one of them is handed.

#include "support/CoreTestSupport.h"

namespace {

/** A width law reading nothing but its own number: the band below is
 *  built to it, so what the audit measures and what it asks for are the
 *  same fact stated twice. */
struct Width {
  float px = 0.0f;
  float across(float) const { return px; }
  float max() const { return px; }
  bool operator==(const Width&) const = default;
};

/** The spine a band is built along here: one cubic, long enough to carry
 *  hundreds of steps and curved enough that the band's edges lie in every
 *  direction rather than along two axes. */
SkPath sCurve() {
  SkPathBuilder b;
  b.moveTo(80, 300);
  b.cubicTo(300, 190, 660, 410, 900, 300);
  return b.detach();
}

/** A straight spine of the same length: on a turn the shortest chord
 *  through a station runs ACROSS THE TURN rather than across the band, so
 *  a run that is asked what the band measures is asked on a straight
 *  one. */
SkPath straightSpine() {
  SkPathBuilder b;
  b.moveTo(80, 300);
  b.lineTo(900, 300);
  return b.detach();
}

/** A band as the stroke grammar builds one: @p steps quadrilaterals along
 *  @p spine, each @p width across, consecutive pieces sharing an edge,
 *  all in ONE path under the nonzero fill. @p pinch narrows the piece at
 *  that step to a third of the width — a defect the size of one step,
 *  which is what a width audit exists to find. */
SkPath bandAlong(const SkPath& spine, float width, int steps, int pinch = -1) {
  SkPathBuilder b;
  SkContourMeasureIter it(spine, false);
  sk_sp<SkContourMeasure> contour = it.next();
  if (!contour || steps < 1) return b.detach();
  const float len = contour->length();
  SkPoint prevLeft{0, 0}, prevRight{0, 0};
  bool have = false;
  for (int i = 0; i <= steps; ++i) {
    const float d = len * (float)i / (float)steps;
    SkPoint p;
    SkVector tangent;
    if (!contour->getPosTan(d, &p, &tangent)) continue;
    float half = width * 0.5f;
    if (pinch >= 0 && (i == pinch || i == pinch + 1)) half *= 1.0f / 3.0f;
    const SkPoint left{p.x() - tangent.y() * half, p.y() + tangent.x() * half};
    const SkPoint right{p.x() + tangent.y() * half, p.y() - tangent.x() * half};
    if (have) {
      b.moveTo(prevLeft);
      b.lineTo(left);
      b.lineTo(right);
      b.lineTo(prevRight);
      b.close();
    }
    prevLeft = left;
    prevRight = right;
    have = true;
  }
  return b.detach();
}

/** The question `coverage()` answers, asked the slow way: every sample
 *  straight at every piece's own `SkPath::contains`. The check's answer
 *  must be THIS answer — an index is an arrangement of the work and never
 *  a different measurement — so this stands as the definition the fast
 *  path is held to. */
test::Coverage byThePath(std::span<const SkPath> pieces, const SkRect& box,
                         const SkPath* region, int grid, size_t witnesses = 8) {
  test::Coverage out;
  if (box.isEmpty() || grid < 2) return out;
  const float dx = box.width() / (float)grid;
  const float dy = box.height() / (float)grid;
  for (int gy = 0; gy < grid; ++gy) {
    const float y = box.top() + ((float)gy + 0.5f) * dy;
    for (int gx = 0; gx < grid; ++gx) {
      const float x = box.left() + ((float)gx + 0.5f) * dx;
      if (region && !region->contains(x, y)) continue;
      int hits = 0;
      for (const SkPath& p : pieces)
        if (p.getBounds().contains(x, y) && p.contains(x, y)) ++hits;
      ++out.samples;
      if (hits == 0) {
        ++out.uncovered;
        if (out.uncoveredAt.size() < witnesses)
          out.uncoveredAt.push_back({x, y});
      } else if (hits > 1) {
        ++out.doubled;
        if (out.doubledAt.size() < witnesses) out.doubledAt.push_back({x, y});
      }
    }
  }
  return out;
}

void expectSameCoverage(const test::Coverage& want, const test::Coverage& got) {
  EXPECT_EQ(want.samples, got.samples);
  EXPECT_EQ(want.uncovered, got.uncovered);
  EXPECT_EQ(want.doubled, got.doubled);
  ASSERT_EQ(want.uncoveredAt.size(), got.uncoveredAt.size());
  for (size_t i = 0; i < want.uncoveredAt.size(); ++i)
    EXPECT_EQ(want.uncoveredAt[i], got.uncoveredAt[i]) << "gap " << i;
  ASSERT_EQ(want.doubledAt.size(), got.doubledAt.size());
  for (size_t i = 0; i < want.doubledAt.size(); ++i)
    EXPECT_EQ(want.doubledAt[i], got.doubledAt[i]) << "overlap " << i;
}

/** Four boxes tiling a square, with one gap and one overlap put there on
 *  purpose — a six-pixel strip claimed by nobody and a square claimed
 *  twice, the two defects that cancel in every cheap check. */
std::vector<SkPath> flawedTiling() {
  const auto box = [](float l, float t, float r, float b) {
    SkPathBuilder p;
    p.addRect(SkRect::MakeLTRB(l, t, r, b));
    return p.detach();
  };
  return {box(0, 0, 50, 50), box(50, 0, 100, 50), box(0, 50, 44, 100),
          box(50, 50, 100, 100), box(60, 60, 80, 80)};
}

}  // namespace

TEST(ComposeChecks, CoverageAnswersOverABandWhatThePathsAnswer) {
  // Hundreds of pieces in one path: the index files the band's segments
  // by lattice row and a sample reads the few that span its own row, and
  // the count it arrives at is the count the path arrives at, sample for
  // sample, including which points are handed back as witnesses.
  const SkPath band = bandAlong(sCurve(), 40.0f, 719);
  const SkPath pinched = bandAlong(sCurve(), 40.0f, 719, 360);
  const std::vector<SkPath> pieces{band, pinched};
  const SkRect box = band.getBounds();
  expectSameCoverage(byThePath(pieces, box, nullptr, 128),
                     test::coverage(pieces, box, 128));

  // The same band alone answers a real question about itself: a step's
  // worth of it is claimed twice by nobody and missed by nobody.
  const std::vector<SkPath> alone{band};
  expectSameCoverage(byThePath(alone, box, nullptr, 128),
                     test::coverage(alone, box, 128));
}

TEST(ComposeChecks, CoverageAnswersOverATilingWhatThePathsAnswer) {
  const std::vector<SkPath> pieces = flawedTiling();
  const SkRect square = SkRect::MakeWH(100, 100);
  const test::Coverage cov = test::coverage(pieces, square, 100);
  expectSameCoverage(byThePath(pieces, square, nullptr, 100), cov);
  // And the two defects are both seen, which is the whole point of
  // sampling rather than summing area: the strip and the double claim
  // are nearly the same size and would have cancelled.
  EXPECT_GT(cov.uncovered, 0);
  EXPECT_GT(cov.doubled, 0);
}

TEST(ComposeChecks, CoverageInsideARegionAnswersWhatThePathsAnswer) {
  const std::vector<SkPath> pieces = flawedTiling();
  // A polygonal region is indexed the way a piece is; a region carrying
  // curves is not, and every sample is asked of the path — the same
  // answer either way, which is what these two cases stand for.
  SkPathBuilder poly;
  poly.moveTo(10, 10);
  poly.lineTo(90, 20);
  poly.lineTo(70, 95);
  poly.lineTo(15, 80);
  poly.close();
  const SkPath polygon = poly.detach();
  SkPathBuilder round;
  round.addOval(SkRect::MakeLTRB(8, 8, 92, 92));
  const SkPath disc = round.detach();

  for (const SkPath* region : {&polygon, &disc}) {
    const test::Coverage got = test::coverage(pieces, *region, 96);
    expectSameCoverage(byThePath(pieces, region->getBounds(), region, 96), got);
    EXPECT_GT(got.samples, 0);
    EXPECT_LT(got.samples, 96 * 96);  // the region really did exclude some
  }
}

TEST(ComposeChecks, WidthAlongMeasuresABandOfSevenHundredSteps) {
  // The fixture a study hands the audit: hundreds of overlapping steps,
  // each measured across the FILLED region rather than to the first edge
  // met, which on this shape is an interior seam at every step.
  const geometry::path::Profile flat{Width{40.0f}};
  const SkPath spine = straightSpine();
  const test::WidthAlong audit =
      test::widthAlong(bandAlong(spine, 40.0f, 719), spine, flat);
  EXPECT_EQ(audit.samples, 192);
  EXPECT_LT(audit.maxError, 1.0f) << "worst " << audit.maxError << " px";
  ASSERT_FALSE(audit.worst.empty());
  EXPECT_FLOAT_EQ(audit.worst.front().intended, 40.0f);

  // And the run is pinned station for station, not only bounded: these
  // are the numbers the audit answers when every ray reads every edge of
  // the band, and culling the rays to their own lines is an arrangement
  // of that work rather than a different measurement.
  EXPECT_NEAR(audit.maxError, 0.98236084f, 1e-3f);
  EXPECT_NEAR(audit.rmsError, 0.605008721f, 1e-3f);
  EXPECT_NEAR(audit.worst.front().measured, 39.0176392f, 1e-3f);
  EXPECT_NEAR(audit.worst.front().along, 682.0f, 1e-3f);
}

TEST(ComposeChecks, WidthAlongFindsOneNarrowedStepInSevenHundred) {
  // The defect a spatial cull would lose if it were not exact: two steps
  // of a seven-hundred-step band narrowed to a third of the width, a
  // pinch two pixels long in a band eight hundred long, on a spine that
  // turns. The audit has to name it, and name where it is.
  const geometry::path::Profile flat{Width{40.0f}};
  const SkPath spine = sCurve();
  const int step = 360;
  const test::WidthAlong audit =
      test::widthAlong(bandAlong(spine, 40.0f, 719, step), spine, flat);
  ASSERT_FALSE(audit.worst.empty());
  const test::WidthStation& worst = audit.worst.front();
  EXPECT_LT(worst.measured, 15.0f) << "the pinch was not measured";
  SkContourMeasureIter it(spine, false);
  const sk_sp<SkContourMeasure> contour = it.next();
  ASSERT_TRUE(contour);
  const float pinchAt = contour->length() * (float)step / 719.0f;
  EXPECT_NEAR(worst.along, pinchAt, 20.0f) << "found in the wrong place";

  // Pinned, as the straight run is: the same stations, the same chords.
  EXPECT_EQ(audit.samples, 196);
  EXPECT_NEAR(audit.maxError, 27.361721f, 1e-3f);
  EXPECT_NEAR(audit.rmsError, 3.6026628f, 1e-3f);
  EXPECT_NEAR(worst.measured, 12.6382799f, 1e-3f);
  EXPECT_NEAR(worst.along, 418.0f, 1e-3f);
}

// -------------------------------------------------------------------------
// The same two checks over what a bounding box cannot answer: a
// subdivision whose overlap and gap cancel, an outline that is not a
// box, and the arcs a contour leaves dangling.

TEST(ComposeDebug, CoverageCatchesWhatAreaAndContainmentMiss) {
  // A subdivision that OVERLAPS in one place and GAPS in another passes
  // both cheap checks. Area conservation passes because the two errors
  // cancel exactly; containment passes because every piece really is
  // inside the parent. Only point sampling sees it.
  const SkRect region = SkRect::MakeWH(100, 100);

  // An honest split of the square into two halves.
  auto rect = [](float l, float t, float r, float b) {
    SkPathBuilder p;
    p.addRect(SkRect::MakeLTRB(l, t, r, b));
    return p.detach();
  };
  const std::vector<SkPath> exact = {rect(0, 0, 50, 100),
                                     rect(50, 0, 100, 100)};
  const auto good = test::coverage(exact, region, 64);
  EXPECT_TRUE(good.exact());
  EXPECT_EQ(good.uncovered, 0);
  EXPECT_EQ(good.doubled, 0);

  // The same two halves, one shifted 10 px right: a 10-wide gap on the
  // left, a 10-wide overlap in the middle. Equal areas, so the total is
  // unchanged and both pieces are still inside the square.
  const std::vector<SkPath> broken = {rect(10, 0, 60, 100),
                                      rect(50, 0, 100, 100)};
  float area = 0;
  for (const SkPath& p : broken)
    area += p.getBounds().width() * p.getBounds().height();
  EXPECT_FLOAT_EQ(area, 100 * 100);  // area conservation: PASSES
  for (const SkPath& p : broken)
    EXPECT_TRUE(region.contains(p.getBounds()));  // containment: PASSES

  const auto bad = test::coverage(broken, region, 64);
  EXPECT_FALSE(bad.exact());  // …and coverage does not
  EXPECT_NEAR(bad.uncoveredFraction(), 0.10f, 0.02f);
  EXPECT_NEAR(bad.doubledFraction(), 0.10f, 0.02f);
  ASSERT_FALSE(bad.uncoveredAt.empty());
  EXPECT_LT(bad.uncoveredAt.front().x(), 10.0f);  // the witness is the gap
}

TEST(ComposeDebug, EndpointDegreesFindTheDanglingArc) {
  // The chaining test for a decorated tiling: on the Oxford Penrose
  // paving every interior arc endpoint must have degree 2, or the
  // stainless bands do not link up into rings.
  auto seg = [](float x0, float y0, float x1, float y1) {
    SkPathBuilder p;
    p.moveTo(x0, y0).lineTo(x1, y1);
    return p.detach();
  };
  // Three segments chained head-to-tail: two interior joints (degree 2),
  // two loose ends (degree 1).
  const std::vector<SkPath> chain = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                                     seg(20, 0, 30, 0)};
  const auto degrees = test::endpointDegrees(chain);
  EXPECT_EQ(degrees.points.size(), 4u);
  EXPECT_EQ(degrees.outside(2, 2).size(), 2u);  // the two loose ends

  // Move one segment off its joint: now four loose ends, not two.
  const std::vector<SkPath> broken = {seg(0, 0, 10, 0), seg(11, 0, 20, 0),
                                      seg(20, 0, 30, 0)};
  EXPECT_EQ(test::endpointDegrees(broken).outside(2, 2).size(), 4u);
}

TEST(ComposeDebug, CoverageOverAnArbitraryRegionAndComponentCounting) {
  // An annulus, a sector, a plate — anything whose outline is not a box
  // cannot be tested against its bounds without counting the parts
  // outside it as gaps. A ring of segments compared against a true circle
  // reports chord error as gaps, dozens of them, none of them real.
  auto rect = [](float l, float t, float r, float b) {
    SkPathBuilder p;
    p.addRect(SkRect::MakeLTRB(l, t, r, b));
    return p.detach();
  };
  // A DISC covered by two half-squares that also spill outside it. The
  // rect overload would call the spill "doubled" nowhere and the corners
  // "uncovered"; the region overload only asks about the disc.
  SkPathBuilder discBuilder;
  discBuilder.addCircle(50, 50, 40);
  const SkPath disc = discBuilder.detach();
  const std::vector<SkPath> halves = {rect(0, 0, 50, 100),
                                      rect(50, 0, 100, 100)};

  const auto onRect = test::coverage(halves, SkRect::MakeWH(100, 100), 64);
  EXPECT_TRUE(onRect.exact());  // the square really is covered exactly
  const auto onDisc = test::coverage(halves, disc, 64);
  EXPECT_TRUE(onDisc.exact());
  EXPECT_LT(onDisc.samples, onRect.samples);  // it tested fewer points…
  EXPECT_GT(onDisc.samples, 1000);            // …but a real number of them

  // components(): "is this one piece of metal?" — the question a rete, a
  // knot and a decorated tiling all actually ask, which the degree list
  // alone cannot answer.
  auto seg = [](float x0, float y0, float x1, float y1) {
    SkPathBuilder p;
    p.moveTo(x0, y0).lineTo(x1, y1);
    return p.detach();
  };
  const std::vector<SkPath> chain = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                                     seg(20, 0, 30, 0)};
  EXPECT_EQ(test::endpointDegrees(chain).components(), 1u);

  const std::vector<SkPath> split = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                                     seg(40, 0, 50, 0)};
  EXPECT_EQ(test::endpointDegrees(split).components(), 2u);
}

TEST(ComposeDebug, ClosedContoursHaveNoEndpointsAndSaySo) {
  // A closed contour has NO endpoints, so reporting one per contour is not
  // merely wrong, it is meaningless — and silently so, since a plausible
  // count comes back either way. The endpoint count is reported instead.
  auto sector = [](float a0, float a1) {
    SkPathBuilder p;
    p.moveTo(0, 0)
        .lineTo(std::cos(a0) * 50, std::sin(a0) * 50)
        .lineTo(std::cos(a1) * 50, std::sin(a1) * 50)
        .close();
    return p.detach();
  };
  std::vector<SkPath> ring;
  ring.reserve(12);
  for (int i = 0; i < 12; ++i)
    ring.push_back(sector((float)i * SK_FloatPI / 6.0f,
                          (float)(i + 1) * SK_FloatPI / 6.0f));

  const auto d = test::endpointDegrees(ring);
  EXPECT_EQ(d.closedContours, 12u);
  EXPECT_TRUE(d.points.empty());  // …and no phantom degree-1 vertices
  EXPECT_TRUE(d.outside(2, 2).empty());

  // Open contours still work exactly as before, and mixing the two keeps
  // the open ones' endpoints while counting the closed ones.
  auto seg = [](float x0, float y0, float x1, float y1) {
    SkPathBuilder p;
    p.moveTo(x0, y0).lineTo(x1, y1);
    return p.detach();
  };
  std::vector<SkPath> mixed = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                               sector(0.0f, 0.5f)};
  const auto m = test::endpointDegrees(mixed);
  EXPECT_EQ(m.closedContours, 1u);
  EXPECT_EQ(m.points.size(), 3u);         // the chain's three endpoints
  EXPECT_EQ(m.outside(2, 2).size(), 2u);  // its two loose ends
}

// -------------------------------------------------------------------------
// The scene rasterized and read back, which is what the other checks
// are handed.

TEST(ComposeDebug, RasterizeReadsBackWhatWasDrawn) {
  // Checking a claim against PIXELS rather than against the description that
  // produced them otherwise means hand-rolling a surface, a draw and a
  // read-back at every call site.
  //
  // The F16 default is the non-obvious half. Measuring a falloff whose tail
  // sits at a small fraction of its peak, an 8-bit read-back quantises that
  // tail to a couple of levels — which yields a confident wrong exponent
  // rather than an obviously broken one.
  const auto r = test::rasterize(
      box().absolute().inset(0).fill(Fill::color({1.0f, 0.25f, 0.0f, 1})),
      fonts(), {32, 32});
  ASSERT_TRUE(r.valid());
  EXPECT_EQ(r.width(), 32);
  const SkColor4f c = r.at(16, 16);
  EXPECT_NEAR(c.fR, 1.0f, 0.02f);
  EXPECT_NEAR(c.fG, 0.25f, 0.02f);
  EXPECT_NEAR(c.fB, 0.0f, 0.02f);

  // The point of F16: a ratio far below 8-bit resolution survives.
  // 1/500 of full scale is 0.51 of a 255-step — it quantises to 0 or 1
  // in N32 and is measurable in float.
  const float faint = 1.0f / 500.0f;
  const auto dim = test::rasterize(
      box().absolute().inset(0).fill(Fill::color({faint, faint, faint, 1})),
      fonts(), {8, 8});
  ASSERT_TRUE(dim.valid());
  EXPECT_NEAR(dim.at(4, 4).fR, faint, faint * 0.25f);

  // Out of bounds is transparent rather than undefined.
  EXPECT_EQ(r.at(-1, 0).fA, 0.0f);
  EXPECT_EQ(r.at(0, 999).fA, 0.0f);
}
