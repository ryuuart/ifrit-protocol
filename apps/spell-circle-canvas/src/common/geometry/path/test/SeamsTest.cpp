/** @file
 * The two comparable seams a mark is deviated and widened through — the
 * shaper that bends one and the profile that says how wide it is at each
 * point along it — and the band a width law cuts.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <utility>
#include <vector>

#include "sigilgeometry/path/Band.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Profile.h"
#include "sigilgeometry/path/Shaper.h"

using namespace sigil::geometry::path;

namespace {

// ---------------------------------------------------------------------------
// The shaper seam.

namespace {
struct NudgeX {
  float dx = 0;
  float bleed() const { return std::abs(dx); }
  SkPath shape(const SkPath& p) const {
    return p.makeTransform(SkMatrix::Translate(dx, 0));
  }
  bool operator==(const NudgeX&) const = default;
};
struct Identity {
  SkPath shape(const SkPath& p) const { return p; }
  bool operator==(const Identity&) const = default;
};
}  // namespace

TEST(PathShaper, ComparesByTheHeldSchemeAndItsParameters) {
  EXPECT_TRUE(Shaper(NudgeX{3}) == Shaper(NudgeX{3}));
  EXPECT_FALSE(Shaper(NudgeX{3}) == Shaper(NudgeX{4}));
  // Two different schemes are never equal, whatever they do.
  EXPECT_FALSE(Shaper(NudgeX{0}) == Shaper(Identity{}));
  // The empty shaper is reflexive, or every holder patches forever.
  EXPECT_TRUE(Shaper() == Shaper());
  EXPECT_FALSE(Shaper() == Shaper(Identity{}));
}

TEST(PathShaper, BleedIsReadOffTheSchemeAndIsZeroWhenNotDeclared) {
  EXPECT_FLOAT_EQ(Shaper(NudgeX{-5}).bleed(), 5.0f);
  EXPECT_FLOAT_EQ(Shaper(Identity{}).bleed(), 0.0f);
  // An empty shaper passes its path through untouched.
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(10, 10));
  const SkPath src = b.detach();
  EXPECT_EQ(Shaper().shape(src), src);
  EXPECT_EQ(Shaper(NudgeX{2}).shape(src).getBounds().left(), 2.0f);
}

// ---------------------------------------------------------------------------
// The profile seam.

namespace {
struct Taper {
  float peak = 10;
  float across(float along) const { return peak * (1.0f - along); }
  float max() const { return peak; }
  bool operator==(const Taper&) const = default;
};
struct PxTaper {
  static constexpr bool alongIsPx = true;
  float across(float alongPx) const { return alongPx * 0.1f; }
  float max() const { return 100.0f; }
  bool operator==(const PxTaper&) const = default;
};
}  // namespace

TEST(Profile, ThePresetsAreTheLawsEveryOtherIsDefinedAgainst) {
  EXPECT_FLOAT_EQ(profile::self().across(0.5f), 0.0f);
  EXPECT_FLOAT_EQ(profile::self().max(), 0.0f);
  EXPECT_FLOAT_EQ(profile::offset(-7.0f).across(0.5f), -7.0f);
  // max() is a REACH, so it is the magnitude — cull is sized from it.
  EXPECT_FLOAT_EQ(profile::offset(-7.0f).max(), 7.0f);
  EXPECT_TRUE(profile::offset(4) == profile::offset(4));
  EXPECT_FALSE(profile::offset(4) == profile::offset(5));
  EXPECT_FALSE(profile::offset(0) == profile::self());
  EXPECT_TRUE(Profile() == Profile());
  EXPECT_FALSE(Profile() == profile::self());
}

TEST(Profile, ATaperRunsLinearlyBetweenTwoSignedEnds) {
  const Profile p = profile::taper(10.0f, 2.0f);
  EXPECT_FLOAT_EQ(p.across(0.0f), 10.0f);
  EXPECT_FLOAT_EQ(p.across(0.5f), 6.0f);
  EXPECT_FLOAT_EQ(p.across(1.0f), 2.0f);
  // Clamped past its own ends, so a caller off [0,1] cannot widen it.
  EXPECT_FLOAT_EQ(p.across(-1.0f), 10.0f);
  EXPECT_FLOAT_EQ(p.across(2.0f), 2.0f);
  // max() is the widest reach, which is what every cull is sized from —
  // and a signed taper reaches on BOTH sides.
  EXPECT_FLOAT_EQ(p.max(), 10.0f);
  EXPECT_FLOAT_EQ(profile::taper(-8.0f, 3.0f).max(), 8.0f);
  EXPECT_TRUE(profile::taper(4, 0) == profile::taper(4, 0));
  EXPECT_FALSE(profile::taper(4, 0) == profile::taper(4, 1));
  EXPECT_FALSE(profile::taper(6, 6) == profile::offset(6));
}

TEST(Profile, StepsHoldOneWidthPerSpanAndDoNotInterpolate) {
  // Three widths against two boundaries: the last width holds to the end.
  const Profile p = profile::spans({0.25f, 0.75f}, {3.0f, 9.0f, 5.0f});
  EXPECT_FLOAT_EQ(p.across(0.0f), 3.0f);
  EXPECT_FLOAT_EQ(p.across(0.2f), 3.0f);
  EXPECT_FLOAT_EQ(p.across(0.25f), 9.0f);  // the boundary opens the next span
  EXPECT_FLOAT_EQ(p.across(0.5f), 9.0f);
  EXPECT_FLOAT_EQ(p.across(0.9f), 5.0f);
  EXPECT_FLOAT_EQ(p.across(1.0f), 5.0f);
  EXPECT_FLOAT_EQ(p.max(), 9.0f);
  // A short table reads the last width there is rather than running off.
  EXPECT_FLOAT_EQ(profile::spans({0.5f}, {2.0f}).across(0.9f), 2.0f);
  EXPECT_FLOAT_EQ(profile::spans({}, {}).across(0.5f), 0.0f);
  EXPECT_TRUE(profile::spans({0.5f}, {1, 2}) == profile::spans({0.5f}, {1, 2}));
  EXPECT_FALSE(profile::spans({0.5f}, {1, 2}) ==
               profile::spans({0.6f}, {1, 2}));
}

TEST(Profile, APxKeyedLawIsConvertedOnceByTheSeam) {
  const Profile fraction = Taper{10};
  const Profile px = PxTaper{};
  EXPECT_FALSE(fraction.keyedInPx());
  EXPECT_TRUE(px.keyedInPx());
  // acrossAt is the one call a measured consumer makes: a fraction-keyed
  // law ignores the length, a px-keyed one is handed along * length.
  EXPECT_FLOAT_EQ(fraction.acrossAt(0.25f, 200.0f), 7.5f);
  EXPECT_FLOAT_EQ(px.acrossAt(0.25f, 200.0f), 5.0f);
}

// ---------------------------------------------------------------------------
// The band a width law cuts.

namespace {
/** A width that VARIES and comes back: positive throughout, and equal at
 *  0 and 1, so a closed contour's seam is not a width step and a corner
 *  is the only thing under test. */
struct Swell {
  float base = 12.0f;
  float swing = 6.0f;
  float across(float along) const {
    return base + swing * std::sin(2.0f * 3.14159265f * along);
  }
  float max() const { return std::abs(base) + std::abs(swing); }
  bool operator==(const Swell&) const = default;
};

/** HOW MANY LOOPS A RAIL DOUBLES BACK INTO: the times it crosses itself
 *  with more than `shorterRunsAreOnePlace` of rail between the two
 *  crossing edges, counted over its flattened contours. A rail is one
 *  curve beside another and never a knot.
 *
 *  Neighbouring edges share a point and are not a crossing; on a closed
 *  ring the first and last edges are neighbours too. The length bound is
 *  the spacing the rail was sampled at: nearer than one sample, a join's
 *  own points and the samples beside it stand for the same place on the
 *  spine, and what they enclose is a hairline rather than a corner the
 *  walk turned inside out. */
int cornerLoops(const SkPath& rail, float shorterRunsAreOnePlace) {
  int found = 0;
  for (const Polyline& ring : flatten(rail)) {
    const size_t count = ring.points.size();
    const size_t edges = count < 2 ? 0 : (ring.closed ? count : count - 1);
    // Where each vertex sits along the rail, so a loop is measured in rail
    // length rather than in vertices — a join writes several points into
    // one place and a straight run writes one point per sample.
    std::vector<float> along(count, 0.0f);
    for (size_t i = 1; i < count; ++i)
      along[i] =
          along[i - 1] + glm::distance(ring.points[i - 1], ring.points[i]);
    for (size_t first = 0; first + 2 < edges; ++first)
      for (size_t second = first + 2; second < edges; ++second) {
        if (ring.closed && first == 0 && second == edges - 1) continue;
        if (along[second] - along[first] <= shorterRunsAreOnePlace) continue;
        const Polyline later{
            {ring.points[second], ring.points[(second + 1) % count]}};
        found += (int)edgeCrossings(later, ring.points[first],
                                    ring.points[(first + 1) % count])
                     .size();
      }
  }
  return found;
}

/** The spacing `profileOffset` walks a contour at, which is the rail's
 *  own resolution: a step of about two pixels, or an eighth of a short
 *  contour. */
float railSampleStep(const SkPath& spine) {
  const float len = Contour::of(spine).front().length();
  return len / (float)std::max(8, (int)std::ceil(len / 2.0f));
}

/** An open zigzag of four equal arms whose every interior angle is
 *  `interiorDegrees`, turning alternately so that for one side of travel
 *  each corner is in turn the inside and the outside of its turn. */
SkPath zigzagOfInteriorAngle(float interiorDegrees, float arm = 140.0f) {
  const float turn = 3.14159265f - interiorDegrees * 3.14159265f / 180.0f;
  SkPathBuilder b;
  SkPoint at{60.0f, 300.0f};
  float heading = 0.0f;
  b.moveTo(at);
  for (int k = 0; k < 4; ++k) {
    at = {at.fX + arm * std::cos(heading), at.fY + arm * std::sin(heading)};
    b.lineTo(at);
    heading += k % 2 == 0 ? turn : -turn;
  }
  return b.detach();
}

/** How near a rail comes to one of its spine's vertices. A rail stands
 *  the law's width off the spine everywhere, and a corner is the one
 *  place that is a property of the JOIN rather than of a sample: the
 *  join's own points are struck at the vertex and reach exactly that
 *  width away from it, so the nearest the rail comes to the vertex is
 *  the width itself. */
float nearestTo(const SkPath& rail, glm::vec2 vertex) {
  float nearest = 1e9f;
  for (const Polyline& ring : flatten(rail))
    for (const glm::vec2& point : ring.points)
      nearest = std::min(nearest, glm::distance(point, vertex));
  return nearest;
}

/** A regular polygon wound CLOCKWISE in Skia's y-down space, which is
 *  the winding that makes "inward" unambiguous. */
SkPath clockwisePolygon(int sides, float radius, float centre) {
  SkPathBuilder b;
  for (int k = 0; k < sides; ++k) {
    const float angle = 2.0f * 3.14159265f * (float)k / (float)sides;
    const SkPoint at{centre + radius * std::cos(angle),
                     centre + radius * std::sin(angle)};
    if (k == 0)
      b.moveTo(at);
    else
      b.lineTo(at);
  }
  b.close();
  return b.detach();
}
}  // namespace

TEST(Band, AVaryingRailJoinsAtTheRealVerticesInsteadOfLoopingAtACorner) {
  const SkPath hexagon = clockwisePolygon(6, 100.0f, 200.0f);
  ASSERT_GT(flatten(hexagon).front().signedArea(), 0.0f) << "clockwise";

  // The four rails the two formations under test cut, spelled as the laws
  // the band folds its formation into: positive across is LEFT of travel,
  // which on a clockwise path is outward, so the negative laws are the
  // inner rails — the side a turn bends toward and the side that loops.
  const Profile outward = Swell{};
  const Profile inward = Swell{-12.0f, -6.0f};
  const Profile halfOut = Swell{6.0f, 3.0f};
  const Profile halfIn = Swell{-6.0f, -3.0f};
  // A rail displaced along one sampled normal per point doubles back
  // wherever the turn is toward the offset side: the two offset edges
  // meet BEFORE the vertex's perpendicular foot, so the samples nearest
  // the corner have already overshot it. Joined at the real vertex it
  // does not.
  const float step = railSampleStep(hexagon);
  EXPECT_EQ(cornerLoops(profileOffset(hexagon, outward), step), 0) << "outward";
  EXPECT_EQ(cornerLoops(profileOffset(hexagon, inward), step), 0) << "inward";
  EXPECT_EQ(cornerLoops(profileOffset(hexagon, halfOut), step), 0)
      << "centered, outer rail";
  EXPECT_EQ(cornerLoops(profileOffset(hexagon, halfIn), step), 0)
      << "centered, inner rail";

  // A RIGHT ANGLE, which is the corner this repair is named after and
  // the sharpest one it closes: the miter reaches
  // `radius / tan(half the interior angle)` past the vertex, which at 90°
  // is exactly the radius — the width of the window of samples the join
  // stands for.
  SkPathBuilder box;
  box.addRect(SkRect::MakeXYWH(0, 0, 300, 200));
  const SkPath rectangle = box.detach();
  ASSERT_GT(flatten(rectangle).front().signedArea(), 0.0f) << "clockwise";
  EXPECT_EQ(
      cornerLoops(profileOffset(rectangle, inward), railSampleStep(rectangle)),
      0)
      << "inside every rectangle";

  // …and the bands built from them still occupy the side they name.
  const SkPath inwardBand = bandRegion(hexagon, Swell{}, Formation::Inner);
  const SkPath centeredBand = bandRegion(hexagon, Swell{}, Formation::Center);
  ASSERT_FALSE(inwardBand.isEmpty());
  ASSERT_FALSE(centeredBand.isEmpty());
  EXPECT_NEAR(inwardBand.getBounds().right(), 300.0f, 1.0f);
  EXPECT_GT(centeredBand.getBounds().right(), 300.0f);

  // …and the law still holds ALONG each edge: at every edge's midpoint the
  // rail stands the width the law says, on the side the frame says.
  const SkPath rail = profileOffset(hexagon, outward);
  const std::vector<Contour> spine = Contour::of(hexagon);
  ASSERT_EQ(spine.size(), 1u);
  const float len = spine.front().length();
  for (int edge = 0; edge < 6; ++edge) {
    const float distance = len * ((float)edge + 0.5f) / 6.0f;
    const auto at = spine.front().at(distance);
    ASSERT_TRUE(at.has_value());
    const float width = outward.acrossAt(distance / len, len);
    const glm::vec2 want{at->position.x + at->tangent.y * width,
                         at->position.y - at->tangent.x * width};
    float nearest = 1e9f;
    for (const Polyline& ring : flatten(rail))
      for (const glm::vec2& point : ring.points)
        nearest = std::min(nearest, glm::distance(point, want));
    EXPECT_LT(nearest, 1.5f) << "edge " << edge;
  }
}

TEST(Band, AnOpenVaryingRailJoinsItsCornersAndLeavesItsEndsAlone) {
  // An open zigzag: one corner turns each way, so for a given side one is
  // the inside of the turn and the other the outside — which separates
  // corner joining from anything a closed contour's seam does. The turns
  // are the hexagon's, near 120° of interior angle; the case below walks
  // the same shape from obtuse to sharply acute.
  SkPathBuilder b;
  b.moveTo(60, 300);
  b.lineTo(200, 220);
  b.lineTo(340, 300);
  b.lineTo(480, 220);
  const SkPath zigzag = b.detach();
  const Profile left = Swell{10.0f, 4.0f};
  const Profile right = Swell{-10.0f, 4.0f};
  const float step = railSampleStep(zigzag);
  EXPECT_EQ(cornerLoops(profileOffset(zigzag, left), step), 0)
      << "left of travel";
  EXPECT_EQ(cornerLoops(profileOffset(zigzag, right), step), 0) << "right";

  // The ends are where the spine's ends are, displaced by the law there:
  // a corner join never moved them.
  const std::vector<Contour> spine = Contour::of(zigzag);
  ASSERT_EQ(spine.size(), 1u);
  const float len = spine.front().length();
  const std::vector<Polyline> rail = flatten(profileOffset(zigzag, left));
  ASSERT_EQ(rail.size(), 1u);
  for (const float distance : {0.0f, len}) {
    const auto at = spine.front().at(distance);
    ASSERT_TRUE(at.has_value());
    const float width = left.acrossAt(distance / len, len);
    const glm::vec2 want{at->position.x + at->tangent.y * width,
                         at->position.y - at->tangent.x * width};
    const glm::vec2 end = distance == 0.0f ? rail.front().points.front()
                                           : rail.front().points.back();
    EXPECT_LT(glm::distance(end, want), 0.5f) << "at " << distance;
  }
}

TEST(Band, ARailStandsItsWholeWidthOffEveryCornerItTurnsAround) {
  // A clockwise rectangle, because its corners fall ON the walk's own
  // samples: the step divides its edges, and a measure answers a vertex
  // with the tangent of the piece that ENDS there. That is the
  // arrangement a corner search has to land on exactly — a corner placed
  // a fraction of a stride down the OUTGOING edge carries the join
  // struck at it that far off the vertex, and every point the join
  // writes stands that much nearer the spine than the law says.
  SkPathBuilder b;
  b.addRect(SkRect::MakeXYWH(0, 0, 300, 200));
  const SkPath spine = b.detach();
  ASSERT_GT(flatten(spine).front().signedArea(), 0.0f) << "clockwise";
  const std::vector<Contour> contour = Contour::of(spine);
  ASSERT_EQ(contour.size(), 1u);
  const float len = contour.front().length();
  // Positive across is LEFT of travel, which on a clockwise path is
  // outward, so every corner opens an ARC — the join no window of
  // swallowed samples stands in for, and the one a duplicated sample at
  // the vertex therefore survives at.
  const float corners[]{0.0f, 300.0f, 500.0f, 800.0f};
  for (const float width : {12.0f, 4.0f, 30.0f}) {
    const SkPath constant = parallel(spine, width, 2.0f);
    for (const float distance : corners) {
      const auto at = contour.front().at(distance);
      ASSERT_TRUE(at.has_value());
      EXPECT_GE(nearestTo(constant, at->position), width - 0.002f)
          << "constant " << width << " at " << distance;
    }
  }
  // …and the same of a rail a width LAW cuts, read at the corner's own
  // distance: a corner is one place and the width of that place is the
  // one its join is struck with.
  const Profile law = Swell{};
  const SkPath varying = profileOffset(spine, law);
  for (const float distance : corners) {
    const auto at = contour.front().at(distance);
    ASSERT_TRUE(at.has_value());
    const float width = law.acrossAt(distance / len, len);
    EXPECT_GE(nearestTo(varying, at->position), width - 0.002f)
        << "varying at " << distance;
  }

  // …AND THE RAIL NEVER CROSSES ITSELF, at any scale and on a spine
  // whose corners do NOT fall on the walk's samples. A join stands for
  // its vertex and the walk writes no sample at that vertex, so there
  // is not even the hairline a point written twice leaves — which is
  // what `0` rather than a sample step asks of the count here.
  for (const int sides : {3, 5, 6, 7})
    for (const float radius : {100.0f, 137.0f})
      for (const float width : {12.0f, -12.0f}) {
        const SkPath polygon = clockwisePolygon(sides, radius, 200.0f);
        EXPECT_EQ(cornerLoops(parallel(polygon, width, 2.0f), 0.0f), 0)
            << "constant " << width << " on a " << sides << "-gon of "
            << radius;
        EXPECT_EQ(
            cornerLoops(profileOffset(polygon, Swell{width, width * 0.5f}),
                        0.0f),
            0)
            << "varying " << width << " on a " << sides << "-gon of " << radius;
      }
}

TEST(Band, ACornerSharperThanARightAngleLeavesNoSpurEitherSide) {
  // The two offset edges of a turn INTO the offset fold across each
  // other `radius / tan(half the interior angle)` back from the vertex:
  // at a right angle exactly the offset, above one less, and BELOW one
  // further — so a window of the offset alone leaves the samples between
  // the two standing in the rail, each of them already past the fold,
  // and each closing a small loop toward the offset side.
  for (const float interior :
       {150.0f, 120.0f, 90.0f, 75.0f, 60.0f, 45.0f, 30.0f}) {
    const SkPath spine = zigzagOfInteriorAngle(interior);
    const float step = railSampleStep(spine);
    // Both sides: a zigzag turns each way, so one side's inside of a
    // turn is the other side's outside.
    for (const float width : {12.0f, -12.0f}) {
      EXPECT_EQ(cornerLoops(parallel(spine, width, step), step), 0)
          << "constant " << width << " at " << interior << "°";
      EXPECT_EQ(
          cornerLoops(profileOffset(spine, Swell{width, width * 0.35f}), step),
          0)
          << "varying " << width << " at " << interior << "°";
    }
  }
  // A CONSTANT rail holds far into the acute, where the fold reaches
  // several times the offset and no bound but the neighbouring corner
  // stops it. A law that VARIES is struck from the width at the vertex
  // and read again at the window's ends, and by this sharpness those
  // are two different widths — which is a separate matter from the
  // window, and the reason the sweep above stops where it does.
  for (const float interior : {25.0f, 20.0f, 15.0f, 10.0f}) {
    const SkPath spine = zigzagOfInteriorAngle(interior);
    const float step = railSampleStep(spine);
    for (const float width : {12.0f, -12.0f})
      EXPECT_EQ(cornerLoops(parallel(spine, width, step), step), 0)
          << "constant " << width << " at " << interior << "°";
  }
}

TEST(Band, AFoldReachesNoFurtherThanTheCornerBesideIt) {
  // A long edge, a 30° corner whose two offset edges fold 44.8 px back
  // from it against a 12 px law, a 25 px edge, a corner turning the
  // other way, and another long edge. A fold is between the two edges
  // ITS OWN vertex sits between: past the next vertex the contour has
  // turned away, and the rail there stands off an edge this corner
  // never met.
  const float turn = 150.0f * 3.14159265f / 180.0f;
  SkPathBuilder b;
  b.moveTo(60, 300);
  b.lineTo(200, 300);
  const SkPoint corner{200.0f + 25.0f * std::cos(turn),
                       300.0f + 25.0f * std::sin(turn)};
  b.lineTo(corner);
  b.lineTo(corner.fX + 140.0f, corner.fY);
  const SkPath spine = b.detach();
  const std::vector<Contour> contour = Contour::of(spine);
  ASSERT_EQ(contour.size(), 1u);

  // One side of travel turns into the first corner and the other into
  // the second, so each says what the OTHER corner's fold would have
  // swallowed: 44.8 px past the short edge, and 19.8 px back before it.
  const std::vector<std::pair<float, std::vector<float>>> beyond{
      {-12.0f, {170.0f, 178.0f, 184.0f}}, {12.0f, {124.0f, 130.0f, 138.0f}}};
  for (const auto& [across, distances] : beyond) {
    const SkPath rail = parallel(spine, across, 2.0f);
    EXPECT_EQ(cornerLoops(rail, 2.0f), 0) << "across " << across;
    for (const float distance : distances) {
      const auto at = contour.front().at(distance);
      ASSERT_TRUE(at.has_value());
      const glm::vec2 want{at->position.x + at->tangent.y * across,
                           at->position.y - at->tangent.x * across};
      EXPECT_LT(nearestTo(rail, want), 0.01f)
          << "across " << across << " at " << distance;
    }
  }

  // …and an open rail begins where its spine begins however sharp the
  // corner past its first edge: a fold cannot reach back along a
  // contour that has stopped.
  SkPathBuilder stub;
  stub.moveTo(60, 300);
  stub.lineTo(80, 300);
  stub.lineTo(80.0f + 140.0f * std::cos(turn),
              300.0f + 140.0f * std::sin(turn));
  const SkPath shortFirstEdge = stub.detach();
  const std::vector<Contour> stubContour = Contour::of(shortFirstEdge);
  ASSERT_EQ(stubContour.size(), 1u);
  for (const float across : {12.0f, -12.0f}) {
    const auto at = stubContour.front().at(0.0f);
    ASSERT_TRUE(at.has_value());
    const glm::vec2 want{at->position.x + at->tangent.y * across,
                         at->position.y - at->tangent.x * across};
    EXPECT_LT(nearestTo(parallel(shortFirstEdge, across, 2.0f), want), 0.01f)
        << "the start, across " << across;
  }
}

TEST(Band, AConstantProfileRidesParallelsCornerRepair) {
  SkPathBuilder b;
  b.addRect(SkRect::MakeXYWH(0, 0, 100, 60));
  const SkPath spine = b.detach();
  // Positive across is LEFT of travel, which on Skia's clockwise rect is
  // outside it: the rail's bounds grow by the offset on every side.
  const SkPath out = profileOffset(spine, profile::offset(6.0f));
  EXPECT_FALSE(out.isEmpty());
  EXPECT_LE(out.getBounds().left(), -5.0f);
  EXPECT_GE(out.getBounds().right(), 105.0f);
  // A zero profile is the boundary itself, handed back untouched.
  EXPECT_EQ(profileOffset(spine, profile::self()), spine);
}

TEST(Band, TheRegionIsBoundedByTheWidthAndEmptyWithoutOne) {
  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(200, 50);
  const SkPath spine = b.detach();
  const SkPath centred = bandRegion(spine, profile::offset(20.0f));
  ASSERT_FALSE(centred.isEmpty());
  // Centred: half the width each side of the spine.
  EXPECT_NEAR(centred.getBounds().top(), 40.0f, 1.0f);
  EXPECT_NEAR(centred.getBounds().bottom(), 60.0f, 1.0f);
  // Outer puts the whole width on one side.
  const SkPath outward =
      bandRegion(spine, profile::offset(20.0f), Formation::Outer);
  ASSERT_FALSE(outward.isEmpty());
  EXPECT_NEAR(outward.getBounds().height(), 20.0f, 1.0f);
  // A profile that is zero everywhere sweeps nothing.
  EXPECT_TRUE(bandRegion(spine, profile::self()).isEmpty());
}

// ---------------------------------------------------------------------------
// The band SWEPT rather than zipped.

/** A right angle, so a join has something to close. */
SkPath elbow() {
  SkPathBuilder b;
  b.moveTo(40, 40);
  b.lineTo(160, 40);
  b.lineTo(160, 160);
  return b.detach();
}

/** How many pixels of a 300 x 300 field @p path inks, aliased. */
int inked(const SkPath& path) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
  bm.eraseColor(SK_ColorBLACK);
  SkCanvas canvas(bm);
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setColor(SK_ColorWHITE);
  canvas.drawPath(path, paint);
  int on = 0;
  for (int y = 0; y < 300; ++y)
    for (int x = 0; x < 300; ++x)
      if (bm.getColor(x, y) == SK_ColorWHITE) ++on;
  return on;
}

TEST(Band, ASweptBandClosesItsTurnTheWayItsJoinSays) {
  const SweepWidth constant = [](const SweepStation&) { return 24.0f; };
  const SkPath spine = elbow();
  const SkPath mitre = sweptRegion(spine, constant, {.join = SweepJoin::Miter});
  const SkPath round = sweptRegion(spine, constant, {.join = SweepJoin::Round});
  const SkPath bevel = sweptRegion(spine, constant, {.join = SweepJoin::Bevel});
  ASSERT_FALSE(mitre.isEmpty());
  // The point reaches furthest, the chord least, the arc between them —
  // the same ordering a stroke's three joins have, because it is the same
  // decision. Nothing else about the band differs, so the counts differ by
  // the corner alone.
  const int m = inked(mitre), r = inked(round), b = inked(bevel);
  EXPECT_GT(m, r);
  EXPECT_GT(r, b);
  // …and none of them punches a hole on the INSIDE of the bend, which is
  // what a reversed piece under the winding fill would do.
  for (const SkPath& band : {mitre, round, bevel}) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
    bm.eraseColor(SK_ColorBLACK);
    SkCanvas canvas(bm);
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    canvas.drawPath(band, paint);
    EXPECT_EQ(bm.getColor(152, 48), SK_ColorWHITE) << "a hole inside the bend";
  }
  // A run with no turn has no join to make, so all three are one band.
  const SkPath straight = SkPath::Line({40, 40}, {200, 40});
  EXPECT_EQ(inked(sweptRegion(straight, constant, {.join = SweepJoin::Miter})),
            inked(sweptRegion(straight, constant, {.join = SweepJoin::Bevel})));
}

TEST(Band, ASweptWidthMayBeKeyedOnDirectionWhereAProfileCannot) {
  // The reason a band is swept at all: a pen NIB is widest where the spine
  // crosses it and thinnest along it, which is a function of the TANGENT —
  // no profile keyed on arc length can say it. The elbow's two legs run at
  // right angles, so one is fat and the other thin.
  const SweepWidth nib = [](const SweepStation& at) {
    const float a = std::atan2(at.tangent.y(), at.tangent.x());
    return 4.0f + 20.0f * std::abs(std::sin(a));
  };
  const SkPath band = sweptRegion(elbow(), nib);
  ASSERT_FALSE(band.isEmpty());
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
  bm.eraseColor(SK_ColorBLACK);
  SkCanvas canvas(bm);
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setColor(SK_ColorWHITE);
  canvas.drawPath(band, paint);
  int wide = 0, thin = 0;
  for (int y = 0; y < 300; ++y)
    if (bm.getColor(100, y) == SK_ColorWHITE) ++thin;  // across the flat leg
  for (int x = 0; x < 300; ++x)
    if (bm.getColor(x, 100) == SK_ColorWHITE) ++wide;  // across the upright
  EXPECT_LE(thin, 6);
  EXPECT_GE(wide, 20);

  // A law that answers nothing sweeps nothing, and a non-finite answer
  // pinches to the spine rather than deleting the whole mark.
  EXPECT_TRUE(sweptRegion(elbow(), {}).isEmpty());
  const SkPath pinched = sweptRegion(elbow(), [](const SweepStation& at) {
    return at.fraction < 0.5f ? 20.0f : std::nanf("");
  });
  EXPECT_FALSE(pinched.isEmpty());
  EXPECT_TRUE(pinched.isFinite());
}

}  // namespace
