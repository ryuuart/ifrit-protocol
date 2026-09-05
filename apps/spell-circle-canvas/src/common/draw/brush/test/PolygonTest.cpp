/** @file
 * The stored polygon: its edges are its vertices.
 */

#include <gtest/gtest.h>
#include <sigildraw/brush/Polygon.h>

#include <vector>

namespace {

namespace brush = sigil::draw::brush;

TEST(Polygon, IntersectsAndTranslates) {
  const brush::Polygon polygon({{10, 10}, {50, 10}, {50, 40}, {10, 40}});
  const std::vector<SkPoint> hits = polygon.intersect({{0, 25}, {80, 25}});
  ASSERT_EQ(hits.size(), 2u);
  EXPECT_EQ(hits[0], SkPoint::Make(10, 25));
  EXPECT_EQ(hits[1], SkPoint::Make(50, 25));
  const brush::Polygon moved = polygon.translated(4, -3);
  EXPECT_EQ(moved.vertices.front(), SkPoint::Make(14, 7));
  EXPECT_FALSE(moved.empty());
  EXPECT_TRUE(brush::Polygon({{0, 0}, {1, 1}}).empty());
}

TEST(Polygon, AVertexAddedLaterIsAnEdgeAtOnce) {
  brush::Polygon polygon({{10, 10}, {50, 10}, {50, 40}});
  EXPECT_EQ(polygon.intersect({{0, 35}, {80, 35}}).size(), 2u);
  polygon.vertices.push_back({10, 40});
  const std::vector<SkPoint> hits = polygon.intersect({{0, 35}, {80, 35}});
  ASSERT_EQ(hits.size(), 2u);
  EXPECT_EQ(hits[0], SkPoint::Make(10, 35));
  EXPECT_EQ(hits[1], SkPoint::Make(50, 35));
}

}  // namespace
