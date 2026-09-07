/** @file
 * The multigrid dualised: N families of parallel lines becoming a tiling
 * of rhombs, the two Penrose shapes, the octagonal tiling's squares, the
 * rhombille, and the one set of corners they all share.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "sigilgeometry/path/Lattice.h"

using namespace sigil::geometry::path;

namespace {

constexpr double kPenroseOffset = 0.2;

double edgeLength(const MultigridTiling& t, const MultigridRhomb& r, int i) {
  const glm::dvec2 a = t.vertices[(size_t)r.corners[i]];
  const glm::dvec2 b = t.vertices[(size_t)r.corners[(i + 1) % 4]];
  return std::hypot(b.x - a.x, b.y - a.y);
}

/** The rhomb's acute angle in degrees, read off its two corner-to-corner
 *  edges rather than off the families that made it. */
double acuteDeg(const MultigridTiling& t, const MultigridRhomb& r) {
  const glm::dvec2 o = t.vertices[(size_t)r.corners[0]];
  const glm::dvec2 a = t.vertices[(size_t)r.corners[1]] - o;
  const glm::dvec2 b = t.vertices[(size_t)r.corners[3]] - o;
  const double c =
      (a.x * b.x + a.y * b.y) / (std::hypot(a.x, a.y) * std::hypot(b.x, b.y));
  const double deg =
      std::acos(std::clamp(c, -1.0, 1.0)) * 180.0 / std::numbers::pi;
  return std::min(deg, 180.0 - deg);
}

int countNear(const std::vector<double>& values, double want, double slack) {
  return (int)std::count_if(values.begin(), values.end(), [&](double v) {
    return std::abs(v - want) < slack;
  });
}

std::vector<double> acuteAngles(const MultigridTiling& t) {
  std::vector<double> out;
  out.reserve(t.rhombs.size());
  for (const MultigridRhomb& r : t.rhombs) out.push_back(acuteDeg(t, r));
  return out;
}

}  // namespace

TEST(Multigrid, FivefoldDualisesIntoTheTwoPenroseRhombsAndNothingElse) {
  const std::vector<MultigridFamily> families =
      multigridRing(5, kPenroseOffset);
  ASSERT_EQ(families.size(), 5u);
  const MultigridTiling t = multigrid(families, {.radius = 12.0});

  ASSERT_FALSE(t.rhombs.empty());
  // Every edge is one, whatever the crossing that made it: a rhomb's
  // sides are the two families' normals and those are unit.
  for (const MultigridRhomb& r : t.rhombs)
    for (int i = 0; i < 4; ++i) EXPECT_NEAR(edgeLength(t, r, i), 1.0, 1e-9);

  const std::vector<double> angles = acuteAngles(t);
  const int fat = countNear(angles, 72.0, 1e-6);
  const int thin = countNear(angles, 36.0, 1e-6);
  EXPECT_EQ(fat + thin, (int)t.rhombs.size());
  EXPECT_EQ((int)t.rhombs.size(), 605);
  EXPECT_EQ(fat, 370);
  EXPECT_EQ(thin, 235);
  // The two shapes stand in the golden ratio over a patch of any size,
  // which is the tiling's own statistic and not a property of the reach.
  EXPECT_NEAR((double)fat / (double)thin, std::numbers::phi, 0.05);

  // A rhomb is named by the two families that crossed, and the shape
  // follows only from how far apart in the ring they stand.
  for (const MultigridRhomb& r : t.rhombs) {
    const int apart = std::abs(r.families[0] - r.families[1]);
    const int step = std::min(apart, 5 - apart);
    EXPECT_NEAR(acuteDeg(t, r), step == 1 ? 72.0 : 36.0, 1e-6);
  }
}

TEST(Multigrid, FourfoldDualisesIntoTheOctagonalSquaresAndRhombs) {
  const MultigridTiling t =
      multigrid(multigridRing(4, kPenroseOffset), {.radius = 12.0});
  ASSERT_FALSE(t.rhombs.empty());

  const std::vector<double> angles = acuteAngles(t);
  const int squares = countNear(angles, 90.0, 1e-6);
  const int rhombs = countNear(angles, 45.0, 1e-6);
  EXPECT_EQ(squares + rhombs, (int)t.rhombs.size());
  EXPECT_EQ((int)t.rhombs.size(), 611);
  EXPECT_EQ(squares, 252);
  EXPECT_EQ(rhombs, 359);
  // Ammann-Beenker's own statistic: a square for every root-two rhombs.
  EXPECT_NEAR((double)rhombs / (double)squares, std::numbers::sqrt2, 0.03);

  for (const MultigridRhomb& r : t.rhombs) {
    const int apart = std::abs(r.families[0] - r.families[1]);
    EXPECT_NEAR(acuteDeg(t, r), apart == 2 ? 90.0 : 45.0, 1e-6);
    for (int i = 0; i < 4; ++i) EXPECT_NEAR(edgeLength(t, r, i), 1.0, 1e-9);
  }
}

TEST(Multigrid, ThreefoldDualisesIntoTheRhombille) {
  const MultigridTiling t =
      multigrid(multigridRing(3, kPenroseOffset), {.radius = 12.0});
  ASSERT_FALSE(t.rhombs.empty());
  // One shape at three turns: the cube-corner paving.
  for (const MultigridRhomb& r : t.rhombs)
    EXPECT_NEAR(acuteDeg(t, r), 60.0, 1e-6);
  EXPECT_EQ((int)t.rhombs.size(), 572);
}

TEST(Multigrid, EveryCornerTwoRhombsShareIsOneVertex) {
  const MultigridTiling t =
      multigrid(multigridRing(5, kPenroseOffset), {.radius = 6.0});
  ASSERT_FALSE(t.rhombs.empty());

  for (const MultigridRhomb& r : t.rhombs) {
    for (int i = 0; i < 4; ++i) {
      ASSERT_GE(r.corners[i], 0);
      ASSERT_LT((size_t)r.corners[i], t.vertices.size());
      for (int j = i + 1; j < 4; ++j) EXPECT_NE(r.corners[i], r.corners[j]);
    }
  }
  // The weld is complete and it welds nothing it should not: no two
  // vertices stand near each other, and the count is the one a
  // brute-force weld over every corner reaches.
  for (size_t i = 0; i < t.vertices.size(); ++i)
    for (size_t j = i + 1; j < t.vertices.size(); ++j)
      EXPECT_GT(std::hypot(t.vertices[i].x - t.vertices[j].x,
                           t.vertices[i].y - t.vertices[j].y),
                1e-6);

  std::vector<glm::dvec2> brute;
  for (const MultigridRhomb& r : t.rhombs) {
    for (int i = 0; i < 4; ++i) {
      const glm::dvec2 p = t.vertices[(size_t)r.corners[i]];
      const bool seen =
          std::any_of(brute.begin(), brute.end(), [&](glm::dvec2 q) {
            return std::hypot(q.x - p.x, q.y - p.y) < 1e-6;
          });
      if (!seen) brute.push_back(p);
    }
  }
  EXPECT_EQ(brute.size(), t.vertices.size());
  // Four corners a rhomb, and far fewer vertices than that: the sharing
  // is the whole point of one deduplicated set.
  EXPECT_LT(t.vertices.size(), t.rhombs.size() * 2);
}

TEST(Multigrid, OneOffsetInEveryFamilyIsExactlyFivefoldAboutTheOrigin) {
  const MultigridTiling t =
      multigrid(multigridRing(5, kPenroseOffset), {.radius = 8.0});
  ASSERT_FALSE(t.rhombs.empty());

  // Turned by a fifth of a turn the tiling stands on itself. It is the
  // claim the double solve is FOR: the counting ceilings that place each
  // rhomb read a crossing that lands on a grid line exactly, and a
  // rounded grid would put a rhomb a whole edge away from its mirror.
  const double turn = 2.0 * std::numbers::pi / 5.0;
  const double c = std::cos(turn), s = std::sin(turn);
  int matched = 0, inside = 0;
  for (const glm::dvec2& v : t.vertices) {
    // Only what is comfortably inside can have its image kept too.
    if (std::hypot(v.x, v.y) > 6.0) continue;
    ++inside;
    const glm::dvec2 turned{v.x * c - v.y * s, v.x * s + v.y * c};
    matched +=
        std::any_of(t.vertices.begin(), t.vertices.end(), [&](glm::dvec2 q) {
          return std::hypot(q.x - turned.x, q.y - turned.y) < 1e-9;
        });
  }
  EXPECT_GT(inside, 100);
  EXPECT_EQ(matched, inside);
}

TEST(Multigrid, TheReachIsTheOnlyThingThatDecidesHowFarItRuns) {
  const std::vector<MultigridFamily> families =
      multigridRing(5, kPenroseOffset);
  const MultigridTiling near = multigrid(families, {.radius = 5.0});
  const MultigridTiling far = multigrid(families, {.radius = 9.0});
  EXPECT_LT(near.rhombs.size(), far.rhombs.size());

  // Every rhomb of the smaller patch is a rhomb of the larger, named the
  // same way: the reach trims, it does not renumber.
  auto identity = [](const MultigridRhomb& r) {
    return std::array<int, 4>{r.families[0], r.families[1], r.lines[0],
                              r.lines[1]};
  };
  std::vector<std::array<int, 4>> wide;
  for (const MultigridRhomb& r : far.rhombs) wide.push_back(identity(r));
  std::sort(wide.begin(), wide.end());
  for (const MultigridRhomb& r : near.rhombs)
    EXPECT_TRUE(std::binary_search(wide.begin(), wide.end(), identity(r)));

  // Nothing comes back that stands wholly outside the reach.
  for (const MultigridRhomb& r : near.rhombs) {
    bool any = false;
    for (int i = 0; i < 4; ++i) {
      const glm::dvec2 p = near.vertices[(size_t)r.corners[i]];
      any = any || std::hypot(p.x, p.y) <= 5.0 + 1e-9;
    }
    EXPECT_TRUE(any);
  }
}

TEST(Multigrid, FamiliesThatCannotCrossDualiseIntoNothing) {
  EXPECT_TRUE(multigrid({}, {.radius = 4.0}).rhombs.empty());
  const MultigridFamily one{.normal = 0.4};
  EXPECT_TRUE(multigrid({&one, 1}, {.radius = 4.0}).rhombs.empty());
  // Two families facing the same way are one family of lines twice over,
  // and a pair of parallel lines bounds no rhomb.
  const MultigridFamily parallel[2] = {{.normal = 0.4, .offset = 0.1},
                                       {.normal = 0.4, .offset = 0.6}};
  EXPECT_TRUE(multigrid(parallel, {.radius = 4.0}).rhombs.empty());
}

TEST(Multigrid, TheBoundOnRhombsIsTheOneThingThatStopsIt) {
  const MultigridTiling t = multigrid(multigridRing(5, kPenroseOffset),
                                      {.radius = 12.0, .maxRhombs = 50});
  EXPECT_EQ(t.rhombs.size(), 50u);
}
