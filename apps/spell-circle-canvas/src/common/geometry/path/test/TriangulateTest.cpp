/** @file
 * The triangulation on point sets whose answer is known by hand, the dual
 * cells it builds, and the outline at a tightness — the convex hull at no
 * bound, a crescent's own shape below one.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <numbers>
#include <set>
#include <vector>

#include "sigilgeometry/path/Hull.h"
#include "sigilgeometry/path/Triangulate.h"

using namespace sigil::geometry::path;

namespace {

/** The four corners of a unit square, which two triangles cover and no
 *  other pair of triangles can. */
const std::vector<glm::vec2> kSquare = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};

std::set<std::pair<float, float>> pointSet(const Polyline& ring) {
  std::set<std::pair<float, float>> found;
  for (const glm::vec2 point : ring.points) found.insert({point.x, point.y});
  return found;
}

}  // namespace

// ---------------------------------------------------------------------------
// Delaunay

TEST(Delaunay, ASquareIsTwoTriangles) {
  const Triangulation mesh = delaunay(kSquare);
  EXPECT_EQ(mesh.points.size(), 4u);
  ASSERT_EQ(mesh.triangles.size(), 2u);
  // Every corner is used, and the two triangles share exactly one edge —
  // which is what makes them a cover of the square rather than an overlap.
  std::set<uint32_t> used;
  for (const glm::uvec3 triangle : mesh.triangles)
    for (int corner = 0; corner < 3; ++corner) used.insert(triangle[corner]);
  EXPECT_EQ(used.size(), 4u);

  int shared = 0;
  for (const glm::uvec3 across : mesh.neighbours)
    for (int edge = 0; edge < 3; ++edge)
      if (across[edge] != Triangulation::noNeighbour) ++shared;
  EXPECT_EQ(shared, 2);
}

TEST(Delaunay, NoPointFallsInsideAnyCircumcircle) {
  // The defining property, checked directly: that is what makes a
  // triangulation Delaunay rather than merely a triangulation.
  std::vector<glm::vec2> points;
  for (int i = 0; i < 60; ++i) {
    const float angle = 0.7f * (float)i;
    points.push_back({120.0f * std::cos(angle) + 3.0f * (float)i,
                      140.0f * std::sin(angle) - 2.0f * (float)i});
  }
  const Triangulation mesh = delaunay(points);
  ASSERT_GT(mesh.triangles.size(), 10u);
  for (size_t t = 0; t < mesh.triangles.size(); ++t) {
    const glm::vec2 centre = mesh.circumcentre(t);
    const float radius = mesh.circumradius(t);
    const glm::uvec3 triangle = mesh.triangles[t];
    for (uint32_t i = 0; i < (uint32_t)mesh.points.size(); ++i) {
      if (i == triangle.x || i == triangle.y || i == triangle.z) continue;
      EXPECT_GE(glm::length(mesh.points[i] - centre), radius * 0.999f)
          << "point " << i << " inside triangle " << t;
    }
  }
}

TEST(Delaunay, TheCircumcentreOfARightTriangleIsItsHypotenuseMidpoint) {
  const Triangulation mesh =
      delaunay(std::vector<glm::vec2>{{0, 0}, {100, 0}, {0, 100}});
  ASSERT_EQ(mesh.triangles.size(), 1u);
  const glm::vec2 centre = mesh.circumcentre(0);
  EXPECT_NEAR(centre.x, 50.0f, 1e-2f);
  EXPECT_NEAR(centre.y, 50.0f, 1e-2f);
  EXPECT_NEAR(mesh.circumradius(0), std::sqrt(5000.0f), 1e-2f);
}

TEST(Delaunay, DuplicatePointsAreOnePoint) {
  std::vector<glm::vec2> doubled = kSquare;
  doubled.insert(doubled.end(), kSquare.begin(), kSquare.end());
  const Triangulation mesh = delaunay(doubled);
  EXPECT_EQ(mesh.points.size(), 4u);
  EXPECT_EQ(mesh.triangles.size(), 2u);
}

TEST(Delaunay, TooFewOrCollinearPointsBoundNothing) {
  EXPECT_TRUE(delaunay(std::vector<glm::vec2>{}).triangles.empty());
  EXPECT_TRUE(
      delaunay(std::vector<glm::vec2>{{0, 0}, {1, 1}}).triangles.empty());
  EXPECT_TRUE(delaunay(std::vector<glm::vec2>{{0, 0}, {1, 0}, {2, 0}, {3, 0}})
                  .triangles.empty());
}

TEST(Delaunay, AdjacencyIsSharedEdges) {
  const Triangulation mesh = delaunay(kSquare);
  for (uint32_t i = 0; i < 4; ++i) {
    const std::vector<uint32_t> near = mesh.adjacent(i);
    // A corner of a square shares an edge with the two beside it, and with
    // the opposite one only if the diagonal was drawn there.
    EXPECT_GE(near.size(), 2u);
    EXPECT_LE(near.size(), 3u);
    EXPECT_EQ(std::find(near.begin(), near.end(), i), near.end());
  }
}

// ---------------------------------------------------------------------------
// Voronoi

TEST(Voronoi, TwoPointsSplitTheBoundsDownTheBisector) {
  const std::vector<Polyline> cells =
      voronoi(std::vector<glm::vec2>{{-50, 0}, {50, 0}},
              SkRect::MakeLTRB(-100, -100, 100, 100));
  ASSERT_EQ(cells.size(), 2u);
  EXPECT_NEAR(cells[0].signedArea(), 20000.0f, 1.0f);
  EXPECT_NEAR(cells[1].signedArea(), 20000.0f, 1.0f);
  for (const glm::vec2 point : cells[0].points)
    EXPECT_LE(point.x, 0.0f + 1e-3f);
  for (const glm::vec2 point : cells[1].points)
    EXPECT_GE(point.x, 0.0f - 1e-3f);
}

TEST(Voronoi, EveryCellHoldsItsOwnPointAndTheCellsCoverTheBounds) {
  std::vector<glm::vec2> points;
  for (int i = 0; i < 40; ++i)
    points.push_back({(float)((i * 37) % 200), (float)((i * 91) % 200)});
  const SkRect box = SkRect::MakeLTRB(-20, -20, 220, 220);
  const std::vector<Polyline> cells = voronoi(points, box);
  ASSERT_EQ(cells.size(), points.size());

  float total = 0;
  for (size_t i = 0; i < cells.size(); ++i) {
    ASSERT_GE(cells[i].points.size(), 3u) << "cell " << i;
    EXPECT_TRUE(cells[i].contains(points[i])) << "cell " << i;
    total += std::abs(cells[i].signedArea());
  }
  // The cells tile the bounds: they meet edge to edge and leave nothing
  // over, so their areas sum to the box's.
  EXPECT_NEAR(total, box.width() * box.height(), box.width() * 0.01f);
}

TEST(Voronoi, ASinglePointOwnsEverything) {
  const std::vector<Polyline> cells =
      voronoi(std::vector<glm::vec2>{{5, 5}}, SkRect::MakeLTRB(0, 0, 10, 20));
  ASSERT_EQ(cells.size(), 1u);
  EXPECT_NEAR(std::abs(cells[0].signedArea()), 200.0f, 1e-3f);
}

TEST(Voronoi, EmptyBoundsAnswerEmptyCells) {
  const std::vector<Polyline> cells =
      voronoi(kSquare, SkRect::MakeLTRB(0, 0, 0, 0));
  ASSERT_EQ(cells.size(), 4u);
  for (const Polyline& cell : cells) EXPECT_TRUE(cell.points.empty());
}

// ---------------------------------------------------------------------------
// Hull

TEST(Hull, AtNoBoundItIsTheConvexHull) {
  std::vector<glm::vec2> points = kSquare;
  // Points strictly inside cannot be on a convex hull however many there
  // are.
  for (int i = 1; i < 10; ++i) points.push_back({(float)i * 9, (float)i * 7});
  const std::vector<Polyline> rings = hull(points);
  ASSERT_EQ(rings.size(), 1u);
  EXPECT_TRUE(rings[0].closed);
  EXPECT_EQ(rings[0].points.size(), 4u);
  EXPECT_EQ(pointSet(rings[0]), (std::set<std::pair<float, float>>{
                                    {0, 0}, {100, 0}, {100, 100}, {0, 100}}));
  EXPECT_NEAR(std::abs(rings[0].signedArea()), 10000.0f, 1e-2f);
}

TEST(Hull, PointsOnTheEdgesAreNotCorners) {
  const std::vector<Polyline> rings = hull(std::vector<glm::vec2>{
      {0, 0}, {50, 0}, {100, 0}, {100, 50}, {100, 100}, {0, 100}});
  ASSERT_EQ(rings.size(), 1u);
  EXPECT_EQ(rings[0].points.size(), 4u);
}

TEST(Hull, ABoundLetsTheOutlineReachIntoAConcavity) {
  // A ring of points with a quarter of it missing: the convex answer
  // spans the gap, and a bound smaller than the gap does not.
  std::vector<glm::vec2> crescent;
  for (int i = 0; i < 90; ++i) {
    const float angle = 2.0f * std::numbers::pi_v<float> * (float)i / 120.0f;
    for (float radius : {160.0f, 200.0f})
      crescent.push_back({radius * std::cos(angle), radius * std::sin(angle)});
  }
  const float convex = std::abs(hull(crescent).front().signedArea());
  float tight = 0;
  for (const Polyline& ring : hull(crescent, 40.0f))
    tight += std::abs(ring.signedArea());
  EXPECT_GT(tight, 0.0f);
  EXPECT_LT(tight, convex * 0.7f);
}

TEST(Hull, ABoundBelowTheSpacingLeavesNothingStanding) {
  std::vector<glm::vec2> spread;
  for (int i = 0; i < 40; ++i)
    spread.push_back({(float)(i % 8) * 50, (float)(i / 8) * 50});
  EXPECT_TRUE(hull(spread, 1.0f).empty());
}

TEST(Hull, TooFewOrCollinearPointsEncloseNothing) {
  EXPECT_TRUE(hull(std::vector<glm::vec2>{{0, 0}, {1, 1}}).empty());
  EXPECT_TRUE(
      hull(std::vector<glm::vec2>{{0, 0}, {1, 0}, {2, 0}, {3, 0}}).empty());
}
