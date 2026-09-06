/** @file
 * A path read as segments and written back, whether two outlines have
 * the same nodes in the same order, and the rewrites that only change
 * which way round a contour is drawn and which node it starts at.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>

#include <vector>

#include "sigilgeometry/path/Direction.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Segments.h"
#include "support/Paths.h"

using namespace sigil::geometry::path;
using sigil::geometry::test::rect;

namespace {

/** Every verb, every point and every conic weight of a path, as one
 *  string — what "the same path" means when the claim is byte identity
 *  rather than the same picture. */
std::string spelling(const SkPath& path) {
  std::string out;
  for (const SegmentContour& contour : segments(path)) {
    out += "M";
    for (const Segment& segment : contour.segments) {
      out += " " + std::to_string((int)segment.kind);
      for (int i = 0; i < segment.size(); ++i)
        out += "," + std::to_string(segment.points[(size_t)i].x) + ":" +
               std::to_string(segment.points[(size_t)i].y);
      out += "@" + std::to_string(segment.weight);
    }
    out += contour.closed ? " Z\n" : "\n";
  }
  return out;
}

/** One contour holding one of every verb kind, closed. */
SkPath everyKind() {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(20, 0);
  b.quadTo(40, 0, 40, 20);
  b.conicTo(40, 40, 20, 40, 0.7071f);
  b.cubicTo(10, 40, 0, 30, 0, 20);
  b.close();
  return b.detach();
}

TEST(PathSegments, ReadsEveryVerbKindWithItsPoints) {
  const std::vector<SegmentContour> read = segments(everyKind());
  ASSERT_EQ(read.size(), 1u);
  EXPECT_TRUE(read[0].closed);
  ASSERT_EQ(read[0].segments.size(), 4u);
  EXPECT_EQ(read[0].segments[0].kind, SegmentKind::Line);
  EXPECT_EQ(read[0].segments[1].kind, SegmentKind::Quad);
  EXPECT_EQ(read[0].segments[2].kind, SegmentKind::Conic);
  EXPECT_EQ(read[0].segments[3].kind, SegmentKind::Cubic);
  EXPECT_FLOAT_EQ(read[0].segments[2].weight, 0.7071f);
  EXPECT_EQ(read[0].segments[3].end(), glm::vec2(0, 20));
}

TEST(PathSegments, RoundTripsAPathVerbForVerbAndWeightForWeight) {
  const SkPath source = everyKind();
  EXPECT_EQ(spelling(toPath(segments(source), source.getFillType())),
            spelling(source));
  EXPECT_TRUE(toPath(segments(source), source.getFillType()) == source);
}

// A closed contour's closing line is the closure and not a segment, so a
// rectangle is three segments and rebuilding it gives back the four
// points Skia stored rather than five.
TEST(PathSegments, DoesNotDuplicateAClosedContoursClosingLine) {
  const SkPath square = rect(0, 0, 10, 10);
  const std::vector<SegmentContour> read = segments(square);
  ASSERT_EQ(read.size(), 1u);
  EXPECT_EQ(read[0].segments.size(), 3u);
  EXPECT_TRUE(read[0].closed);
  EXPECT_TRUE(toPath(read, square.getFillType()) == square);
}

TEST(PathSegments, ReadsEveryContourOfAMultiContourPath) {
  SkPathBuilder b;
  b.addRect(SkRect::MakeXYWH(0, 0, 40, 40));
  b.addRect(SkRect::MakeXYWH(10, 10, 20, 20));
  const SkPath two = b.detach();
  EXPECT_EQ(segments(two).size(), 2u);
  EXPECT_TRUE(toPath(segments(two), two.getFillType()) == two);
}

// ---------------------------------------------------------------------------
// Reversal and restart: the drawn shape stands still, the pen turns round.

TEST(PathSegments, ReversingTwiceGivesTheSourceBackVerbForVerb) {
  const SkPath source = everyKind();
  EXPECT_EQ(spelling(reverse(reverse(source))), spelling(source));
}

TEST(PathSegments, ReversalKeepsTheCurvesAndFlipsTheWinding) {
  const SkPath source = everyKind();
  const SkPath back = reverse(source);
  const std::vector<SegmentContour> read = segments(back);
  ASSERT_EQ(read.size(), 1u);
  int curves = 0;
  for (const Segment& segment : read[0].segments)
    if (segment.kind != SegmentKind::Line) ++curves;
  EXPECT_EQ(curves, 3);
  EXPECT_EQ(read[0].start(), glm::vec2(0, 0));
  EXPECT_LT(flatten(back).front().signedArea() *
                flatten(source).front().signedArea(),
            0);
}

TEST(PathSegments, RestartingAContourMovesTheFirstNodeAndNothingElse) {
  const SkPath square = rect(0, 0, 10, 10);
  const SkPath rolled = startAt(square, 0, 2);
  EXPECT_NE(segments(rolled)[0].start(), segments(square)[0].start());
  EXPECT_EQ(segments(rolled)[0].segments.size(),
            segments(square)[0].segments.size());
  EXPECT_EQ(rolled.computeTightBounds(), square.computeTightBounds());
  EXPECT_EQ(compatible(square, rolled), Compatible::Yes);
}

// ---------------------------------------------------------------------------
// Compatibility: the precondition an exact interpolation and a mastered
// pair of outlines both stand on.

TEST(PathCompatibility, TwoRoundedRectsOfDifferentRadiiPair) {
  const SkPath a = SkPath::RRect(SkRRect::MakeRectXY(SkRect::MakeWH(80, 40), 6, 6));
  const SkPath b = SkPath::RRect(SkRRect::MakeRectXY(SkRect::MakeWH(80, 40), 14, 14));
  EXPECT_EQ(compatible(a, b), Compatible::Yes);
}

TEST(PathCompatibility, NamesTheCoarsestReasonAPairDoesNot) {
  const SkPath square = rect(0, 0, 10, 10);
  SkPathBuilder two;
  two.addRect(SkRect::MakeWH(10, 10));
  two.addRect(SkRect::MakeXYWH(20, 0, 10, 10));
  EXPECT_EQ(compatible(square, two.detach()), Compatible::ContourCount);

  SkPathBuilder triangle;
  triangle.moveTo(0, 0);
  triangle.lineTo(10, 0);
  triangle.lineTo(5, 10);
  triangle.close();
  EXPECT_EQ(compatible(square, triangle.detach()), Compatible::SegmentCount);
}

// A rounded rect and the same rounded rect started at another node hold
// the same kinds in a different order — the one incompatibility a caller
// can repair without redrawing anything, and `startAt` repairs it.
TEST(PathCompatibility, TellsAMovedStartPointFromADifferentDrawing) {
  const SkPath a = SkPath::RRect(SkRRect::MakeRectXY(SkRect::MakeWH(80, 40), 6, 6));
  const SkPath rolled = startAt(a, 0, 1);
  EXPECT_EQ(compatible(a, rolled), Compatible::StartPoint);
  EXPECT_EQ(compatible(a, startAt(rolled, 0, segments(rolled)[0].segments.size() - 1)),
            Compatible::Yes);
}

// ---------------------------------------------------------------------------
// Direction: winding by nesting, contour order, start point.

/** A ring of `radius` about the origin, drawn the way `clockwise` says. */
SkPath ring(float radius, bool clockwise) {
  return SkPath::Circle(0, 0, radius,
                        clockwise ? SkPathDirection::kCW : SkPathDirection::kCCW);
}

TEST(PathDirection, NestingAnswersDepthAndTheTightestEnclosingRing) {
  SkPathBuilder b;
  b.addCircle(0, 0, 90);
  b.addCircle(0, 0, 60);
  b.addCircle(0, 0, 30);
  const std::vector<Polyline> rings = flatten(b.detach());
  ASSERT_EQ(rings.size(), 3u);
  const std::vector<Nesting> where = nesting(rings);
  EXPECT_EQ(where[0], (Nesting{0, -1}));
  EXPECT_EQ(where[1], (Nesting{1, 0}));
  EXPECT_EQ(where[2], (Nesting{2, 1}));
}

TEST(PathDirection, WindsOutersAndHolesTheWayItIsAsked) {
  SkPathBuilder b;
  b.addPath(ring(90, true));
  b.addPath(ring(30, true));  // both the same way: the hole is not a hole
  const SkPath source = b.detach();

  for (const Winding winding :
       {Winding::OutersClockwise, Winding::OutersCounterClockwise}) {
    const SkPath fixed = direction(source, {.winding = winding});
    const std::vector<Polyline> rings = flatten(fixed);
    ASSERT_EQ(rings.size(), 2u);
    // A positive signed area is a clockwise ring in Skia's y-down space.
    const bool outerClockwise = rings[0].signedArea() > 0;
    const bool holeClockwise = rings[1].signedArea() > 0;
    EXPECT_EQ(outerClockwise, winding == Winding::OutersClockwise);
    EXPECT_NE(holeClockwise, outerClockwise);
  }
}

TEST(PathDirection, LeavesTheDrawnShapeExactlyWhereItWas) {
  SkPathBuilder b;
  b.addPath(ring(90, true));
  b.addPath(ring(30, true));
  const SkPath source = b.detach();
  EXPECT_EQ(direction(source).computeTightBounds(),
            source.computeTightBounds());
}

TEST(PathDirection, OrdersContoursOutersFirst) {
  SkPathBuilder b;
  b.addCircle(0, 0, 30);  // the hole, drawn first
  b.addCircle(0, 0, 90);
  const SkPath fixed = direction(b.detach(), {.orderContours = true});
  const std::vector<Polyline> rings = flatten(fixed);
  ASSERT_EQ(rings.size(), 2u);
  EXPECT_GT(rings[0].bounds().width(), rings[1].bounds().width());
}

// Every closed contour ends up starting at its bottom-left node, so two
// outlines built by different routes start their contours at nodes that
// correspond — which is what turns a StartPoint answer into a Yes.
TEST(PathDirection, StartsEveryClosedContourAtItsBottomLeftNode) {
  const SkPath square = rect(0, 0, 10, 10);
  const SkPath rolled = startAt(square, 0, 2);
  const SkPath a = direction(square, {.resetStart = true});
  const SkPath b = direction(rolled, {.resetStart = true});
  EXPECT_EQ(segments(a)[0].start(), segments(b)[0].start());
  EXPECT_EQ(segments(a)[0].start(), glm::vec2(0, 10));
}

}  // namespace
