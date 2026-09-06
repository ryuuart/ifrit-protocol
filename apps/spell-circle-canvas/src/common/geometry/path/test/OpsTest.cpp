/** @file
 * The path operators — the booleans that combine two outlines, the offset
 * that grows and shrinks one, and the distorts that displace one without
 * moving the shape — with the numeric routines and the value noise under
 * them.
 */

#include <gtest/gtest.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkRect.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/pathops/SkPathOps.h>

#include <cmath>
#include <functional>
#include <glm/geometric.hpp>
#include <string>
#include <vector>

#include "sigilgeometry/path/Band.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Edges.h"
#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Ops.h"
#include "sigilgeometry/path/Segments.h"
#include "support/Paths.h"

using namespace sigil::geometry::path;
using sigil::geometry::test::rect;

namespace {

// ---------------------------------------------------------------------------
// The booleans. Two unit squares overlapping by half: each operator names
// one region of that pair, and the width alone cannot tell exclude from
// unite — they share an outline — so every row also probes the overlap.

struct Boolean {
  const char* name;
  SkPath (*combine)(const SkPath&, const SkPath&);
  float width;
  bool keepsTheOverlap;
};

class PathBoolean : public ::testing::TestWithParam<Boolean> {};

TEST_P(PathBoolean, NamesItsRegionOfTwoOverlappingSquares) {
  const SkPath a = rect(0, 0, 100, 100);
  const SkPath b = rect(50, 0, 100, 100);
  const SkPath combined = GetParam().combine(a, b);
  EXPECT_NEAR(combined.computeTightBounds().width(), GetParam().width, 1e-3);
  EXPECT_EQ(combined.contains(75, 50), GetParam().keepsTheOverlap);
}

INSTANTIATE_TEST_SUITE_P(
    PathOps, PathBoolean,
    ::testing::Values(Boolean{"Unite",
                              [](const SkPath& a, const SkPath& b) {
                                return ops::unite(a, b);
                              },
                              150.0f, true},
                      Boolean{"Subtract",
                              [](const SkPath& a, const SkPath& b) {
                                return ops::subtract(a, b);
                              },
                              50.0f, false},
                      Boolean{"Intersect",
                              [](const SkPath& a, const SkPath& b) {
                                return ops::intersect(a, b);
                              },
                              50.0f, true},
                      Boolean{"Exclude",
                              [](const SkPath& a, const SkPath& b) {
                                return ops::exclude(a, b);
                              },
                              150.0f, false}),
    [](const ::testing::TestParamInfo<Boolean>& info) {
      return std::string(info.param.name);
    });

TEST(PathOps, UnitingAListOfOutlinesReachesEveryOneOfThem) {
  const SkPath a = rect(0, 0, 100, 100);
  const SkPath b = rect(50, 0, 100, 100);
  EXPECT_TRUE(ops::unite({a, b, rect(140, 0, 100, 100)}).contains(200, 50));
}

// Offset distance is a radius, not a diameter: a positive amount grows the
// outline by that much on every side, a negative one eats into it, so a
// circle of radius 50 offset by 10 spans 120 across and by -15 spans 70.
TEST(PathOps, OffsetIsARadiusThatGrowsAndShrinksTheOutline) {
  const SkPath circle = SkPath::Circle(0, 0, 50);
  const SkRect grown = ops::offset(circle, 10).computeTightBounds();
  EXPECT_NEAR(grown.width(), 120, 1.5f);
  const SkRect shrunk = ops::offset(circle, -15).computeTightBounds();
  EXPECT_NEAR(shrunk.width(), 70, 1.5f);
}

// ---------------------------------------------------------------------------
// The fold. Five spellings of one offset stood in this library, each with
// one behaviour baked in; the general operator has to reproduce every one
// of them BYTE FOR BYTE, or a caller converted to it gets a different
// drawing. Each case below transcribes the arithmetic the spelling it
// replaces was written with and compares paths, not pictures.

/** A five-pointed star, so the fixtures include reflex corners rather
 *  than only convex ones. */
SkPath star() {
  SkPathBuilder b;
  for (int i = 0; i < 10; ++i) {
    const float angle = (float)i * kPi / 5.0f;
    const float radius = i % 2 == 0 ? 80.0f : 34.0f;
    const SkPoint at{100 + radius * std::cos(angle),
                     100 + radius * std::sin(angle)};
    if (i == 0)
      b.moveTo(at);
    else
      b.lineTo(at);
  }
  b.close();
  return b.detach();
}

/** `ops::offset` as it was: a round-joined, round-capped stroke
 *  expansion of twice the distance, united with the source to grow and
 *  subtracted from it to shrink. */
SkPath strokeExpandedOffset(const SkPath& path, float delta) {
  if (std::abs(delta) < 1e-3f) return path;
  SkPaint stroke;
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(std::abs(delta) * 2.0f);
  stroke.setStrokeJoin(SkPaint::kRound_Join);
  stroke.setStrokeCap(SkPaint::kRound_Cap);
  const SkPath expanded = skpathutils::FillPathWithPaint(path, stroke);
  return delta > 0 ? ops::unite(path, expanded)
                   : ops::simplify(ops::subtract(path, expanded));
}

/** `insetOutline` as it was: the same construction mitred, butt-capped
 *  and with the sign the other way round. */
SkPath mitredInset(const SkPath& outline, float px) {
  if (px == 0) return outline;
  SkPaint offset;
  offset.setStyle(SkPaint::kStroke_Style);
  offset.setStrokeWidth(std::abs(px) * 2.0f);
  offset.setStrokeJoin(SkPaint::kMiter_Join);
  const SkPath ring = skpathutils::FillPathWithPaint(outline, offset);
  SkPath result;
  if (Op(outline, ring,
         px > 0 ? SkPathOp::kDifference_SkPathOp : SkPathOp::kUnion_SkPathOp,
         &result))
    return result;
  return outline;
}

TEST(PathOffset, StraddlingTheSourceIsTheStrokeExpansionItReplaces) {
  for (const SkPath& source : {SkPath::Circle(0, 0, 50), star()})
    for (const float distance : {12.0f, -12.0f, 3.0f, -3.0f})
      EXPECT_TRUE(ops::offset(source, distance) ==
                  strokeExpandedOffset(source, distance))
          << "distance " << distance;
}

// The concentric frame is the same construction mitred, and it comes
// back through the self-intersection cleanup every shrink here goes
// through — the one place the two spellings it folds did not already
// agree, and the robust one of the two is what the operator keeps.
TEST(PathOffset, TheMitredButtJoinedOffsetIsTheConcentricFrameItReplaces) {
  for (const SkPath& source : {SkPath::Circle(0, 0, 50), star()}) {
    EXPECT_TRUE(
        ops::offset(source, -6.0f,
                    {.join = ops::Join::Miter, .cap = ops::Cap::Butt}) ==
        ops::simplify(mitredInset(source, 6.0f)));
    EXPECT_TRUE(
        ops::offset(source, 6.0f,
                    {.join = ops::Join::Miter, .cap = ops::Cap::Butt}) ==
        mitredInset(source, -6.0f));
  }
}

// Position 0 takes only the left rail and position 1 only the right, so
// each end of the dial is the sideways walk, exactly.
TEST(PathOffset, EitherEndOfThePositionDialIsTheParallelWalk) {
  const SkPath source = star();
  EXPECT_TRUE(ops::offset(source, 9.0f, {.position = 0, .step = 2.0f}) ==
              parallel(source, 9.0f, 2.0f));
  EXPECT_TRUE(ops::offset(source, 9.0f, {.position = 1, .step = 2.0f}) ==
              parallel(source, -9.0f, 2.0f));
}

TEST(PathOffset, ARectangleGrowsByTheDistanceOnEveryAxisForEveryJoin) {
  const SkPath source = rect(0, 0, 100, 60);
  for (const ops::Join join :
       {ops::Join::Round, ops::Join::Miter, ops::Join::Bevel}) {
    const SkRect grown =
        ops::offset(source, 10.0f, {.join = join}).computeTightBounds();
    EXPECT_NEAR(grown.width(), 120, 0.5f);
    EXPECT_NEAR(grown.height(), 80, 0.5f);
  }
}

// The band slid off centre is the mark itself rather than the grown
// area: at a quarter of the way across it lies wholly outside a
// clockwise rectangle and encloses none of it.
TEST(PathOffset, ABandToOneSideIsTheMarkAndNotTheGrownArea) {
  const SkPath source = rect(0, 0, 100, 60);
  const SkPath band = ops::offset(source, 10.0f, {.position = 0.1f});
  EXPECT_FALSE(band.contains(50, 30));
  EXPECT_TRUE(ops::offset(source, 10.0f).contains(50, 30));
}

TEST(PathOffset, KeepCompatibleMovesTheNodesAndTheAnswerStillPairs) {
  const SkPath source = rect(0, 0, 100, 60);
  const SkPath grown = ops::offset(source, 10.0f, {.keepCompatible = true});
  EXPECT_EQ(compatible(source, grown), Compatible::Yes);
  const SkRect bounds = grown.computeTightBounds();
  EXPECT_NEAR(bounds.width(), 120, 1e-3f);
  EXPECT_NEAR(bounds.height(), 80, 1e-3f);
}

TEST(PathOffset, KeepCompatibleBluntsANeedleRatherThanShootingOff) {
  SkPathBuilder needle;
  needle.moveTo(0, 0);
  needle.lineTo(200, 1);
  needle.lineTo(200, -1);
  needle.close();
  const SkPath source = needle.detach();
  const SkPath grown =
      ops::offset(source, 5.0f, {.miterLimit = 2.0f, .keepCompatible = true});
  EXPECT_EQ(compatible(source, grown), Compatible::Yes);
  // Two distances is the cap, so no node may travel further than that.
  const std::vector<SegmentContour> before = segments(source);
  const std::vector<SegmentContour> after = segments(grown);
  for (size_t i = 0; i < before[0].segments.size(); ++i)
    EXPECT_LE(glm::length(after[0].segments[i].start() -
                          before[0].segments[i].start()),
              10.0f + 1e-3f);
}

// ---------------------------------------------------------------------------
// Rounding, and the three dials the corner effect has no way to spell.

TEST(PathCorners, TheDefaultRoundingIsTheCornerEffectItReplaces) {
  SkPathBuilder dst;
  SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
  const SkPath source = rect(0, 0, 100, 60);
  sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(8.0f);
  ASSERT_TRUE(fx && fx->filterPath(&dst, source, &rec));
  EXPECT_TRUE(ops::roundCorners(source, 8.0f) == dst.detach());
}

TEST(PathCorners, RoundingCutsEveryCornerOfARectangle) {
  const SkPath source = rect(0, 0, 100, 60);
  const SkPath round = ops::roundCorners(source, 10.0f, {.minTurnDeg = 5.0f});
  const std::vector<SegmentContour> read = segments(round);
  ASSERT_EQ(read.size(), 1u);
  // Four arcs, four straight runs between them.
  int curves = 0;
  for (const Segment& piece : read[0].segments)
    if (piece.kind != SegmentKind::Line) ++curves;
  EXPECT_EQ(curves, 4);
  EXPECT_FALSE(round.contains(0.5f, 0.5f));  // the corner is gone
  EXPECT_TRUE(round.contains(50, 30));
}

// The visual correction holds every arc the same distance out from the
// vertex it replaced, whatever the corner's angle: a 45-degree corner
// and a right-angled one bulge by the same amount.
TEST(PathCorners, VisualCorrectionHoldsTheArcsStandOffConstant) {
  const auto standOff = [](float turnDeg, bool visual) {
    const float turn = turnDeg * kDegToRad;
    SkPathBuilder b;
    b.moveTo(-200, 0);
    b.lineTo(0, 0);
    b.lineTo(200 * std::cos(turn), 200 * std::sin(turn));
    const SkPath corner = ops::roundCorners(
        b.detach(), 20.0f, {.minTurnDeg = 1.0f, .visual = visual});
    // The arc's midpoint stands where the quadratic's middle is; how far
    // that is from the vertex is what the correction holds constant.
    const std::vector<SegmentContour> read = segments(corner);
    if (read.empty()) return 0.0f;
    for (const Segment& piece : read[0].segments)
      if (piece.kind == SegmentKind::Quad)
        return glm::length(
            (piece.points[0] + piece.points[1] * 2.0f + piece.points[2]) /
                4.0f -
            piece.points[1]);
    return 0.0f;
  };
  EXPECT_NEAR(standOff(90.0f, true), standOff(45.0f, true), 1e-2f);
  EXPECT_GT(standOff(90.0f, false), standOff(45.0f, false) * 1.4f);
}

TEST(PathCorners, ACornerShallowerThanTheThresholdIsLeftAlone) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(100, 0);
  b.lineTo(200, 6);  // a turn of about three degrees
  const SkPath source = b.detach();
  EXPECT_TRUE(ops::roundCorners(source, 10.0f, {.minTurnDeg = 20.0f}) ==
              source);
}

// A star's reflex corners stay sharp while its points round, which is
// what a selection with no corners named is asked for.
TEST(PathCorners, OutwardOnlyLeavesTheReflexCornersAlone) {
  const SkPath source = star();
  const SkPath rounded = ops::roundCorners(source, 8.0f, {.outwardOnly = true});
  const std::vector<SegmentContour> read = segments(rounded);
  ASSERT_EQ(read.size(), 1u);
  int curves = 0;
  for (const Segment& piece : read[0].segments)
    if (piece.kind != SegmentKind::Line) ++curves;
  EXPECT_EQ(curves, 5);
}

// ---------------------------------------------------------------------------
// The distorts. Each is shape-preserving in the large: it displaces the
// outline but must not translate the shape or run away in size, and how
// far it may reach is its own amplitude budget. A Roughen of amplitude 6
// can move a point at most 6 outward, so the width cannot exceed the
// diameter plus twice that.

struct Distort {
  const char* name;
  std::function<SkPath(const SkPath&)> apply;
  float reach;  // how far outward this distort's dials allow a point to go
};

class PathDistort : public ::testing::TestWithParam<Distort> {};

TEST_P(PathDistort, DisplacesTheOutlineWithoutMovingOrGrowingTheShape) {
  const SkPath base = SkPath::Circle(100, 100, 60);
  const SkPath distorted = GetParam().apply(base);
  ASSERT_FALSE(distorted.isEmpty());
  const SkRect bounds = distorted.computeTightBounds();
  EXPECT_LT(std::abs(bounds.centerX() - 100), 4);
  EXPECT_LT(std::abs(bounds.centerY() - 100), 4);
  EXPECT_LT(bounds.width(), 120 + 2 * GetParam().reach + 2);
}

INSTANTIATE_TEST_SUITE_P(
    PathOps, PathDistort,
    ::testing::Values(
        Distort{"Roughen",
                [](const SkPath& p) { return ops::Roughen{6, 8, 42}.apply(p); },
                6.0f},
        Distort{"Twirl",
                [](const SkPath& p) { return ops::Twirl{90}.apply(p); }, 0.0f},
        Distort{"Zigzag",
                [](const SkPath& p) { return ops::Zigzag{4, 20}.apply(p); },
                4.0f},
        Distort{"AChainOfTwo",
                [](const SkPath& p) {
                  return ops::chain({ops::offsetBy(6), ops::Zigzag{4, 20}})(p);
                },
                10.0f}),
    [](const ::testing::TestParamInfo<Distort>& info) {
      return std::string(info.param.name);
    });

TEST(PathOps, BloatPushesOutwardAndNeverInward) {
  const SkPath base = SkPath::Circle(100, 100, 60);
  const SkRect bloated =
      ops::PuckerBloat{0.8f}.apply(base).computeTightBounds();
  EXPECT_GT(bloated.width(), 118);
}

// ---------------------------------------------------------------------------
// Numeric

TEST(Numeric, BisectReturnsTheFarSideOfTheTransition) {
  const float at = bisect(0.0f, 1.0f, [](float x) { return x < 0.3f; }, 20);
  EXPECT_GE(at, 0.3f);
  EXPECT_NEAR(at, 0.3f, 1e-5f);
}

TEST(Numeric, WrapIsPeriodicAndNonNegative) {
  EXPECT_FLOAT_EQ(wrap(5, 4), 1);
  EXPECT_FLOAT_EQ(wrap(-1, 4), 3);
  EXPECT_FLOAT_EQ(wrap(4, 4), 0);
}

// ---------------------------------------------------------------------------
// Value noise, which is this leaf's own: the seeded mixers it stands on
// live one library down and are held to their own promises there.

TEST(PathNoise, ValueNoiseIsBoundedAndMovesSmoothlyWithItsInput) {
  float prev = valueNoise({0.5f, 0.5f, 0.5f}, 1);
  for (int i = 1; i <= 100; ++i) {
    const float v = valueNoise({0.5f + i * 0.01f, 0.5f, 0.5f}, 1);
    EXPECT_GE(v, -1.0f);
    EXPECT_LE(v, 1.0f);
    EXPECT_LT(std::abs(v - prev), 0.1f);  // 0.01 steps never jump
    prev = v;
  }
}

// ---------------------------------------------------------------------------
// THE FILL TYPE THROUGH A REBUILDING OPERATOR. Every operator that walks
// a path and writes a new one has to carry the source's fill rule: an
// even-odd donut whose answer comes back winding fills its hole solid,
// and a glyph's counters go with it. One case per operator, all over the
// same square donut — two nested rectangles wound the same way, which is
// a ring under even-odd and a solid slab under winding.

SkPath evenOddDonut() {
  SkPathBuilder b(SkPathFillType::kEvenOdd);
  b.addRect(SkRect::MakeLTRB(0, 0, 100, 100));
  b.addRect(SkRect::MakeLTRB(30, 30, 70, 70));
  return b.detach();
}

struct RebuildCase {
  std::string name;
  std::function<SkPath(const SkPath&)> apply;
};

class PathFillType : public testing::TestWithParam<RebuildCase> {};

TEST_P(PathFillType, AnEvenOddDonutKeepsItsHole) {
  const SkPath out = GetParam().apply(evenOddDonut());
  EXPECT_EQ(out.getFillType(), SkPathFillType::kEvenOdd);
  EXPECT_FALSE(out.contains(50, 50)) << "the hole filled in";
}

INSTANTIATE_TEST_SUITE_P(
    RebuildingOperators, PathFillType,
    testing::Values(
        RebuildCase{"roundCorners",
                    [](const SkPath& p) { return ops::roundCorners(p, 6); }},
        RebuildCase{"selectedCorners",
                    [](const SkPath& p) {
                      return ops::roundCorners(p, 6, {.minTurnDeg = 10});
                    }},
        RebuildCase{"chamferCorners",
                    [](const SkPath& p) { return ops::chamferCorners(p, 5); }},
        RebuildCase{
            "displaceSquare",
            [](const SkPath& p) { return ops::displaceSquare(p, 3, 20); }},
        RebuildCase{"roughen",
                    [](const SkPath& p) {
                      return ops::Roughen{.amplitude = 2, .segmentPx = 5}.apply(
                          p);
                    }},
        RebuildCase{
            "zigzag",
            [](const SkPath& p) {
              return ops::Zigzag{.amplitude = 2, .wavelengthPx = 20}.apply(p);
            }},
        RebuildCase{"puckerBloat",
                    [](const SkPath& p) {
                      return ops::PuckerBloat{.amount = 0.2f}.apply(p);
                    }},
        RebuildCase{"twirl",
                    [](const SkPath& p) {
                      return ops::Twirl{.angleDeg = 10}.apply(p);
                    }},
        RebuildCase{"edges",
                    [](const SkPath& p) { return edges(p, Edge::All); }},
        RebuildCase{"parallel", [](const SkPath& p) { return parallel(p, 3); }},
        RebuildCase{"displace",
                    [](const SkPath& p) { return displace(p, 3, 20, false); }},
        RebuildCase{"profileOffset",
                    [](const SkPath& p) {
                      return profileOffset(p, profile::taper(4, 8));
                    }},
        RebuildCase{
            "bandRegion",
            [](const SkPath& p) { return bandRegion(p, profile::offset(6)); }}),
    [](const testing::TestParamInfo<RebuildCase>& info) {
      return info.param.name;
    });

// The rule the operators carry is the one that decides the picture: the
// same donut wound rather than even-odd IS a solid slab, and stays one.
TEST(PathFillType, TheSameDonutWoundStaysSolid) {
  SkPath winding = evenOddDonut();
  winding.setFillType(SkPathFillType::kWinding);
  const SkPath out = ops::roundCorners(winding, 6);
  EXPECT_EQ(out.getFillType(), SkPathFillType::kWinding);
  EXPECT_TRUE(out.contains(50, 50));
}

}  // namespace
