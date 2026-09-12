/** @file
 * Strip joinery: pieces of stock planed to the seams they share at a
 * node, the figure they unite into, and the half-laps where two pieces
 * cross rather than meet.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Polyline.h"

using namespace sigil::geometry::path;
using operations::Strip;
using operations::StripOptions;

namespace {

Strip spoke(float deg, float length, float width) {
  const float a = deg * std::numbers::pi_v<float> / 180.0f;
  return {{0, 0}, {length * std::cos(a), length * std::sin(a)}, width};
}

std::vector<glm::vec2> cornersOf(const SkPath& path) {
  std::vector<glm::vec2> out;
  const std::vector<Polyline> lines = flatten(path, 0.01f);
  if (lines.empty()) return out;
  out = lines.front().points;
  // A closed contour that came back to where it started names the point
  // twice; the corners are what is distinct.
  if (out.size() > 1 && std::abs(out.front().x - out.back().x) < 1e-4f &&
      std::abs(out.front().y - out.back().y) < 1e-4f)
    out.pop_back();
  return out;
}

bool holds(const std::vector<glm::vec2>& corners, glm::vec2 want,
           float slack = 1e-3f) {
  for (glm::vec2 c : corners)
    if (std::abs(c.x - want.x) < slack && std::abs(c.y - want.y) < slack)
      return true;
  return false;
}

float areaOf(const SkPath& path) {
  float total = 0;
  for (const Polyline& line : flatten(path, 0.01f))
    total += std::abs(line.signedArea());
  return total;
}

}  // namespace

TEST(StripJoinery, APieceThatMeetsNothingIsCutSquareAcross) {
  const Strip alone[1] = {{{10, 20}, {110, 20}, 8}};
  const std::vector<SkPath> outlines = operations::stripOutlines(alone);
  ASSERT_EQ(outlines.size(), 1u);
  const std::vector<glm::vec2> c = cornersOf(outlines[0]);
  EXPECT_EQ(c.size(), 4u);
  EXPECT_TRUE(holds(c, {10, 16}));
  EXPECT_TRUE(holds(c, {10, 24}));
  EXPECT_TRUE(holds(c, {110, 16}));
  EXPECT_TRUE(holds(c, {110, 24}));
  EXPECT_NEAR(areaOf(outlines[0]), 100.0f * 8.0f, 1e-2f);
}

TEST(StripJoinery, ASetWithNoJointsIsEachPieceAsItsOwnRectangle) {
  const Strip apart[3] = {
      {{0, 0}, {40, 0}, 6}, {{0, 50}, {40, 50}, 10}, {{0, 100}, {30, 130}, 4}};
  const std::vector<SkPath> outlines = operations::stripOutlines(apart);
  ASSERT_EQ(outlines.size(), 3u);
  for (size_t i = 0; i < 3u; ++i) {
    const std::vector<glm::vec2> c = cornersOf(outlines[i]);
    EXPECT_EQ(c.size(), 4u) << "piece " << i;
    const glm::vec2 d = apart[i].to - apart[i].from;
    EXPECT_NEAR(areaOf(outlines[i]),
                std::sqrt(d.x * d.x + d.y * d.y) * apart[i].width, 1e-2f);
  }
  // Nothing overlaps, so the joined figure is exactly the three of them.
  EXPECT_NEAR(areaOf(operations::strips(apart)),
              areaOf(outlines[0]) + areaOf(outlines[1]) + areaOf(outlines[2]),
              1e-1f);
}

TEST(StripJoinery, TwoPiecesMeetingSquareAreMitredOnTheCornerDiagonal) {
  // A picture frame's corner: the seam is one straight face through the
  // node, so each piece stays a parallelogram.
  const Strip corner[2] = {spoke(0, 100, 20), spoke(90, 100, 20)};
  const std::vector<SkPath> outlines = operations::stripOutlines(corner);
  const std::vector<glm::vec2> c = cornersOf(outlines[0]);
  EXPECT_EQ(c.size(), 4u);
  EXPECT_TRUE(holds(c, {10, 10}));
  EXPECT_TRUE(holds(c, {-10, -10}));
}

TEST(StripJoinery, ThreePiecesMeetingAtSixtyDegreesTakeAWedgeEach) {
  // The lattice node: three arms, and each piece is planed to the
  // bisector it shares with the arm on either side of it.
  const Strip node[3] = {spoke(0, 80, 12), spoke(120, 80, 12),
                         spoke(240, 80, 12)};
  const std::vector<SkPath> outlines = operations::stripOutlines(node);
  const std::vector<glm::vec2> c = cornersOf(outlines[0]);
  // Four along the piece plus the node itself, which is the apex of the
  // wedge cut out of its end.
  EXPECT_EQ(c.size(), 5u);
  EXPECT_TRUE(holds(c, {0, 0}));
  // The bisector stands at sixty degrees and the rail six from the axis,
  // so the mitre point is six over the sine of sixty along it.
  const float reach =
      6.0f / std::sin(60.0f * std::numbers::pi_v<float> / 180.0f);
  const float x = reach * std::cos(60.0f * std::numbers::pi_v<float> / 180.0f);
  const float y = reach * std::sin(60.0f * std::numbers::pi_v<float> / 180.0f);
  EXPECT_TRUE(holds(c, {x, y}));
  EXPECT_TRUE(holds(c, {x, -y}));
  EXPECT_NEAR(x, 3.4641f, 1e-3f);
  EXPECT_NEAR(y, 6.0f, 1e-3f);

  // Three wedges filling the turn: the joined figure is the three pieces
  // with nothing counted twice and nothing left over.
  float pieces = 0;
  for (const SkPath& p : outlines) pieces += areaOf(p);
  EXPECT_NEAR(areaOf(operations::strips(node)), pieces, 1e-1f);
}

TEST(StripJoinery, FourPiecesMeetingSquareNotchEachOtherOnTheDiagonals) {
  const Strip cross[4] = {spoke(0, 60, 16), spoke(90, 60, 16),
                          spoke(180, 60, 16), spoke(270, 60, 16)};
  const std::vector<SkPath> outlines = operations::stripOutlines(cross);
  const std::vector<glm::vec2> c = cornersOf(outlines[0]);
  EXPECT_EQ(c.size(), 5u);
  // Half a width is eight, and the bisectors run at forty-five degrees,
  // so the notch's two lips stand at eight across and eight along.
  EXPECT_TRUE(holds(c, {8, 8}));
  EXPECT_TRUE(holds(c, {8, -8}));
  EXPECT_TRUE(holds(c, {0, 0}));
  EXPECT_TRUE(holds(c, {60, 8}));
  EXPECT_TRUE(holds(c, {60, -8}));
}

TEST(StripJoinery, TheMitreLimitBluntsANeedleAndBevelStopsEveryPointShort) {
  // Two pieces meeting almost head on: a true mitre reaches off the
  // page, so it is cut back at the limit.
  const Strip needle[2] = {spoke(0, 100, 10), spoke(6, 100, 10)};
  const std::vector<glm::vec2> mitred =
      cornersOf(operations::stripOutlines(needle, {.miterLimit = 3.0f})[0]);
  float reach = 0;
  for (glm::vec2 p : mitred) reach = std::max(reach, std::hypot(p.x, p.y));
  // The far end stands a hundred out; what matters is the near one.
  float nearest = 1e9f;
  for (glm::vec2 p : mitred)
    if (std::hypot(p.x, p.y) < 50.0f)
      nearest = std::min(nearest, std::hypot(p.x, p.y));
  EXPECT_NEAR(nearest, 3.0f * 5.0f, 1e-2f);

  const std::vector<glm::vec2> bevelled = cornersOf(
      operations::stripOutlines(needle, {.join = operations::Join::Bevel})[0]);
  for (glm::vec2 p : bevelled)
    if (std::hypot(p.x, p.y) < 50.0f)
      EXPECT_NEAR(std::hypot(p.x, p.y), 5.0f, 1e-2f);
}

TEST(StripJoinery, ARoundJoinFinishesEachEndWithAnArcOfItsOwnHalfWidth) {
  const Strip alone[1] = {{{0, 0}, {50, 0}, 20}};
  const SkPath outline =
      operations::stripOutlines(alone, {.join = operations::Join::Round})[0];
  // A rectangle with a half-disc on either end: the arc stands its own
  // half-width past each node and nowhere else.
  const SkRect box = outline.getBounds();
  EXPECT_NEAR(box.left(), -10.0f, 1e-3f);
  EXPECT_NEAR(box.right(), 60.0f, 1e-3f);
  EXPECT_NEAR(box.top(), -10.0f, 1e-3f);
  EXPECT_NEAR(box.bottom(), 10.0f, 1e-3f);
  EXPECT_NEAR(areaOf(outline),
              50.0f * 20.0f + std::numbers::pi_v<float> * 100.0f, 1.5f);
}

TEST(StripJoinery, TwoPiecesThatCrossRatherThanMeetAreALap) {
  const Strip crossing[2] = {{{-50, 0}, {50, 0}, 10}, {{0, -50}, {0, 50}, 6}};
  const std::vector<operations::StripLap> laps =
      operations::stripLaps(crossing);
  ASSERT_EQ(laps.size(), 1u);
  EXPECT_EQ(laps[0].pieces[0], 0);
  EXPECT_EQ(laps[0].pieces[1], 1);
  EXPECT_NEAR(laps[0].at.x, 0.0f, 1e-4f);
  EXPECT_NEAR(laps[0].at.y, 0.0f, 1e-4f);
  EXPECT_NEAR(laps[0].at01[0], 0.5f, 1e-5f);
  EXPECT_NEAR(laps[0].at01[1], 0.5f, 1e-5f);
  // Square across, so the seam on each piece is the OTHER piece's width.
  EXPECT_NEAR(laps[0].halfSpan[0], 3.0f, 1e-4f);
  EXPECT_NEAR(laps[0].halfSpan[1], 5.0f, 1e-4f);
}

TEST(StripJoinery, ALapRunsLongerAsTheAngleBetweenThePiecesCloses) {
  const Strip slanted[2] = {{{-50, 0}, {50, 0}, 10},
                            {{-50 * std::cos(0.5236f), -50 * std::sin(0.5236f)},
                             {50 * std::cos(0.5236f), 50 * std::sin(0.5236f)},
                             6}};
  const std::vector<operations::StripLap> laps = operations::stripLaps(slanted);
  ASSERT_EQ(laps.size(), 1u);
  // Thirty degrees: the crossing piece's width carried across doubles.
  EXPECT_NEAR(laps[0].halfSpan[0], 6.0f, 1e-2f);
  EXPECT_NEAR(laps[0].halfSpan[1], 10.0f, 1e-2f);

  // A grazing crossing is bounded rather than allowed to run away.
  const Strip grazing[2] = {{{-50, 0}, {50, 0}, 10},
                            {{-50, -0.5f}, {50, 0.5f}, 6}};
  const std::vector<operations::StripLap> bounded =
      operations::stripLaps(grazing, {.lapLimit = 3.0f});
  ASSERT_EQ(bounded.size(), 1u);
  EXPECT_NEAR(bounded[0].halfSpan[0], 18.0f, 1e-3f);
  EXPECT_NEAR(bounded[0].halfSpan[1], 30.0f, 1e-3f);
}

TEST(StripJoinery, AMeetingAtAnEndIsNotALapAndParallelPiecesNeverCross) {
  const Strip meeting[2] = {spoke(0, 100, 10), spoke(90, 100, 10)};
  EXPECT_TRUE(operations::stripLaps(meeting).empty());

  const Strip tee[2] = {{{-50, 0}, {50, 0}, 10}, {{0, 0}, {0, 50}, 10}};
  EXPECT_TRUE(operations::stripLaps(tee).empty());

  const Strip parallel[2] = {{{0, 0}, {50, 0}, 10}, {{0, 20}, {50, 20}, 10}};
  EXPECT_TRUE(operations::stripLaps(parallel).empty());
}
