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
SkPath bandAlong(const SkPath& spine, float width, int steps,
                 int pinch = -1) {
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
                         const SkPath* region, int grid,
                         size_t witnesses = 8) {
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
    expectSameCoverage(
        byThePath(pieces, region->getBounds(), region, 96), got);
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
