/** @file
 * Direction fields: the seam, the stock values, and the two ways
 * geometry goes through one.
 */

#include <gtest/gtest.h>
#include <sigildraw/Constants.h>
#include <sigildraw/brush/Field.h>
#include <sigildraw/brush/Fields.h>

#include <vector>

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;

TEST(Fields, TraceFollowsAnyCallableDirectionValue) {
  const brush::Wave horizontal{.direction = 0.0f, .amplitude = 0.0f};
  const brush::Stroke path =
      brush::trace(SkPoint::Make(2, 3), 12.0f, 2.0f, 0.0f, horizontal);
  ASSERT_EQ(path.size(), 7u);
  EXPECT_NEAR(path.back().position.fX, 14.0f, 1e-5f);
  EXPECT_NEAR(path.back().position.fY, 3.0f, 1e-5f);

  // A quarter turn clockwise on the y-down canvas heads down the page.
  const brush::Stroke down = brush::trace(
      SkPoint::Make(0, 0), 10.0f, 5.0f, 0.0f, [](SkPoint, float) {
        return HALF_PI;
      });
  EXPECT_NEAR(down.back().position.fY, 10.0f, 1e-5f);
}

TEST(Fields, StockValuesAreValuesAndTheVortexTurnsClockwise) {
  const brush::Vortex clockwise{.center = {0, 0}};
  EXPECT_NEAR(clockwise({10, 0}, 0), HALF_PI, 1e-6f);
  const brush::Vortex anticlockwise{.center = {0, 0}, .direction = -1.0f};
  EXPECT_NEAR(anticlockwise({10, 0}, 0), -HALF_PI, 1e-6f);
  const brush::Curl a(42), b(42);
  EXPECT_EQ(a, b);
  EXPECT_FLOAT_EQ(a({14, 28}, 0.5f), b({14, 28}, 0.5f));
}

TEST(Fields, TheStockCatalogueHoldsSevenNames) {
  const std::vector<std::pair<std::string, brush::Direction>> stock =
      brush::stockFields();
  ASSERT_EQ(stock.size(), 7u);
  for (const auto& [name, field] : stock) EXPECT_TRUE(field) << name;
}

TEST(Fields, WarpClosesItsPath) {
  const std::vector<SkPoint> polygon = {{30, 20}, {94, 28}, {86, 78}, {24, 70}};
  const brush::Wave field{.direction = 0.0f, .amplitude = 0.0f};
  const brush::Stroke warped = brush::warp(polygon, 7.0f, 5.0f, 0.0f, field);
  ASSERT_GT(warped.size(), polygon.size());
  EXPECT_EQ(warped.front(), warped.back());
  EXPECT_NEAR(warped.front().position.fX, polygon.front().fX + 5.0f, 1e-5f);
}

}  // namespace
