/** @file
 * Where marks land inside a shape: the stride that spaces them along a
 * run however the run arrives in pieces, the scanline lattice cut to an
 * even-odd interior, and the polygon inset that moves every edge by one
 * distance.
 */

#include <gtest/gtest.h>
#include <include/core/SkRect.h>

#include <cmath>
#include <glm/geometric.hpp>
#include <vector>

#include "sigilgeometry/path/Edges.h"
#include "sigilgeometry/path/Lattice.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Stride.h"

using namespace sigil::geometry::path;

namespace {

// ---------------------------------------------------------------------------
// Stride

TEST(Stride, LandsOneSpacingApartHoweverThePiecesArrive) {
  // The same total length walked in one piece and in four lands in the
  // same places: that is the whole of what the carried debt buys.
  std::vector<float> whole;
  Stride one;
  one.advance(40.0f, 3.0f,
              [&](Stride::Step step) { whole.push_back(step.distance); });

  std::vector<float> pieces;
  Stride many;
  for (int i = 0; i < 4; ++i)
    many.advance(10.0f, 3.0f,
                 [&](Stride::Step step) { pieces.push_back(step.distance); });

  ASSERT_EQ(whole.size(), pieces.size());
  for (size_t i = 0; i < whole.size(); ++i)
    EXPECT_NEAR(whole[i], pieces[i], 1e-3f);
  EXPECT_FLOAT_EQ(whole.front(), 3.0f);
  EXPECT_FLOAT_EQ(one.travelled(), 40.0f);
  EXPECT_FLOAT_EQ(many.travelled(), 40.0f);

  // The fraction addresses the piece just handed over, not the walk.
  std::vector<float> fractions;
  Stride third;
  third.advance(10.0f, 4.0f,
                [&](Stride::Step step) { fractions.push_back(step.fraction); });
  ASSERT_EQ(fractions.size(), 2u);
  EXPECT_FLOAT_EQ(fractions[0], 0.4f);
  EXPECT_FLOAT_EQ(fractions[1], 0.8f);

  // A piece of no length moves nothing; a restart owes nothing.
  Stride idle;
  int landings = 0;
  idle.advance(0.0f, 1.0f, [&](Stride::Step) { ++landings; });
  EXPECT_EQ(landings, 0);
  one.restart();
  EXPECT_FLOAT_EQ(one.travelled(), 0.0f);
  EXPECT_FLOAT_EQ(one.pending(), 0.0f);
}

// ---------------------------------------------------------------------------
// Lattice

TEST(Lattice, MarksLieInsideTheEvenOddInteriorAndSkipTheHole) {
  Polyline outer;
  outer.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
  Polyline hole;
  hole.points = {{40, 40}, {60, 40}, {60, 60}, {40, 60}};
  const std::vector<Polyline> rings = {outer, hole};

  const std::vector<LatticeMark> marks =
      lattice(rings, {.spacing = 10.0f, .angle = 0.0f});
  ASSERT_FALSE(marks.empty());
  for (const LatticeMark& mark : marks) {
    // A horizontal lattice: both ends on one scanline, and the middle of
    // every mark inside the interior it was cut to.
    EXPECT_NEAR(mark.from.y, mark.to.y, 1e-3f);
    EXPECT_TRUE(containsEvenOdd(rings, (mark.from + mark.to) * 0.5f));
  }
  // The lines that meet the hole are cut in two, so the middle of the
  // square carries more marks than it has scanlines.
  int throughTheHole = 0;
  for (const LatticeMark& mark : marks)
    if (mark.from.y > 40.0f && mark.from.y < 60.0f) ++throughTheHole;
  EXPECT_EQ(throughTheHole, 4);
}

TEST(Lattice, TheAngleTurnsTheLinesAndTheTaperOpensTheGaps) {
  Polyline square;
  square.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
  const std::vector<Polyline> rings = {square};

  const std::vector<LatticeMark> upright =
      lattice(rings, {.spacing = 10.0f, .angle = kPi / 2});
  ASSERT_FALSE(upright.empty());
  for (const LatticeMark& mark : upright)
    EXPECT_NEAR(mark.from.x, mark.to.x, 1e-3f);

  const std::vector<LatticeMark> even =
      lattice(rings, {.spacing = 10.0f, .angle = 0.0f});
  const std::vector<LatticeMark> opening =
      lattice(rings, {.spacing = 10.0f, .angle = 0.0f, .taper = 1.4f});
  EXPECT_LT(opening.size(), even.size());

  // The cap is a bound and not a preference.
  EXPECT_LE((int)lattice(rings, {.spacing = 0.01f, .angle = 0.0f,
                                 .taper = 1.0f, .maxLines = 12})
                .size(),
            12);
  // No rings with an area, no marks.
  Polyline thin;
  thin.points = {{0, 0}, {10, 0}};
  EXPECT_TRUE(lattice(std::vector<Polyline>{thin}, {.spacing = 1.0f}).empty());
}

// ---------------------------------------------------------------------------
// Edges

TEST(Edges, InsetPolygonShrinksASquareByTheDistanceWhicheverWayItWinds) {
  const std::vector<glm::vec2> square = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
  const std::vector<glm::vec2> in = insetPolygon(square, 10);
  ASSERT_EQ(in.size(), 4u);
  const glm::vec2 expected[] = {{10, 10}, {90, 10}, {90, 90}, {10, 90}};
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(in[i].x, expected[i].x, 1e-4f) << i;
    EXPECT_NEAR(in[i].y, expected[i].y, 1e-4f) << i;
  }
  // The same polygon wound the other way insets to the same corners,
  // each still answering to the vertex it came from.
  const std::vector<glm::vec2> reversed(square.rbegin(), square.rend());
  const std::vector<glm::vec2> other = insetPolygon(reversed, 10);
  ASSERT_EQ(other.size(), 4u);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(other[i].x, in[3 - i].x, 1e-4f) << i;
    EXPECT_NEAR(other[i].y, in[3 - i].y, 1e-4f) << i;
  }
  // A negative distance grows.
  const std::vector<glm::vec2> out = insetPolygon(square, -10);
  EXPECT_NEAR(out[0].x, -10, 1e-4f);
  EXPECT_NEAR(out[2].y, 110, 1e-4f);
}

TEST(Edges, InsetPolygonMitresASharpCornerUntilTheLimitBluntsIt) {
  // A thin rhomb: 36° at the two ends, 144° at the two sides. The mitre
  // at a 36° corner is d / sin(18°) — over three distances — while the
  // 144° corner's is barely more than one.
  constexpr float kHalfTip = 18.0f * kPi / 180.0f;
  const float length = 100.0f;
  const float half = length * std::tan(kHalfTip);
  const std::vector<glm::vec2> rhomb = {
      {-length, 0}, {0, -half}, {length, 0}, {0, half}};
  const float d = 4.0f;
  const std::vector<glm::vec2> mitred = insetPolygon(rhomb, d, 100.0f);
  ASSERT_EQ(mitred.size(), 4u);
  EXPECT_NEAR(mitred[0].x, -length + d / std::sin(kHalfTip), 1e-3f);
  EXPECT_NEAR(mitred[0].y, 0.0f, 1e-4f);
  EXPECT_NEAR(mitred[2].x, length - d / std::sin(kHalfTip), 1e-3f);
  // Every moved edge stands exactly d inside the edge it came from: the
  // distance from a moved vertex to the source edge's line is d.
  for (size_t i = 0; i < 4; ++i) {
    const glm::vec2 a = rhomb[i], b = rhomb[(i + 1) % 4];
    const glm::vec2 edge = b - a;
    const glm::vec2 normal = glm::normalize(glm::vec2{-edge.y, edge.x});
    EXPECT_NEAR(std::abs(glm::dot(mitred[i] - a, normal)), d, 1e-3f) << i;
    EXPECT_NEAR(std::abs(glm::dot(mitred[(i + 1) % 4] - a, normal)), d, 1e-3f)
        << i;
  }
  // Capped at two distances the tips stop at 2d along the axis, the
  // vertex count stands, and the wide corners — inside the cap — are
  // exactly where they were.
  const std::vector<glm::vec2> capped = insetPolygon(rhomb, d, 2.0f);
  ASSERT_EQ(capped.size(), 4u);
  EXPECT_NEAR(capped[0].x, -length + 2 * d, 1e-3f);
  EXPECT_NEAR(capped[0].y, 0.0f, 1e-4f);
  EXPECT_NEAR(capped[1].x, mitred[1].x, 1e-4f);
  EXPECT_NEAR(capped[1].y, mitred[1].y, 1e-4f);
}

TEST(Edges, InsetPolygonMovesAReflexCornerTheWayItsEdgesSayAndLeavesTooFewAlone) {
  // An L. Its reflex corner is where the bar meets the column, and the
  // inset L's reflex corner is the meeting of the two moved edges — back
  // toward the outer corner, not toward the interior of either arm.
  const std::vector<glm::vec2> ell = {{0, 0},   {100, 0},  {100, 50},
                                      {50, 50}, {50, 100}, {0, 100}};
  const std::vector<glm::vec2> in = insetPolygon(ell, 10);
  ASSERT_EQ(in.size(), 6u);
  EXPECT_NEAR(in[3].x, 40, 1e-4f);
  EXPECT_NEAR(in[3].y, 40, 1e-4f);
  EXPECT_NEAR(in[0].x, 10, 1e-4f);
  EXPECT_NEAR(in[2].x, 90, 1e-4f);
  EXPECT_NEAR(in[2].y, 40, 1e-4f);
  EXPECT_NEAR(in[4].y, 90, 1e-4f);

  const std::vector<glm::vec2> two = {{0, 0}, {5, 5}};
  const std::vector<glm::vec2> same = insetPolygon(two, 3);
  ASSERT_EQ(same.size(), 2u);
  EXPECT_EQ(same[1], glm::vec2(5, 5));
}

}  // namespace
