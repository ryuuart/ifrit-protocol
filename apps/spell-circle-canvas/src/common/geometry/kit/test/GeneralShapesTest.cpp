/** @file
 * The general shapes over a box and their stock values: a stock value is
 * the general form with its options fixed, so the two draw the same
 * outline; the modifiers every general shape carries; and the solids
 * that lift an outline flat or loft a run of sections.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>

#include <cmath>
#include <vector>

using namespace sigil::geometry;

namespace {

constexpr glm::vec2 kBox{200, 120};

TEST(GeneralShapes, AStockValueIsTheGeneralFormWithItsOptionsFixed) {
  EXPECT_EQ(shapes::polygon(6, 15), shapes::radial(6, {.fromDegrees = 15}));
  EXPECT_EQ(shapes::star(5, 0.42f),
            shapes::radial(10, {.radii = {1.0f, 0.42f}}));
  EXPECT_EQ(shapes::circle(), shapes::ellipse());
  EXPECT_EQ(shapes::annulus(0.55f), shapes::ellipse({.inner = 0.55f}));
  EXPECT_EQ(shapes::sector(200, 250, 0.45f),
            shapes::ellipse({.fromDegrees = 200,
                             .sweepDegrees = 250,
                             .close = shapes::Close::Pie,
                             .inner = 0.45f}));
  EXPECT_EQ(shapes::squircle(4).outline(kBox),
            shapes::ellipse({.exponent = 4}).outline(kBox));
}

TEST(GeneralShapes, AnEllipseClosesItsSweepThreeWays) {
  const auto open = shapes::ellipse(
      {.sweepDegrees = 90, .close = shapes::Close::Open}).outline({100, 100});
  const auto chord = shapes::ellipse(
      {.sweepDegrees = 90, .close = shapes::Close::Chord}).outline({100, 100});
  const auto pie = shapes::ellipse(
      {.sweepDegrees = 90, .close = shapes::Close::Pie}).outline({100, 100});
  const float quarter = 3.14159265f * 50.0f / 2.0f;
  EXPECT_NEAR(open.length(), quarter, 0.2f);
  EXPECT_NEAR(chord.length(), quarter + 50.0f * std::sqrt(2.0f), 0.3f);
  EXPECT_NEAR(pie.length(), quarter + 100.0f, 0.3f);
  EXPECT_NEAR(pie.area(), 3.14159265f * 2500.0f / 4.0f, 10.0f);
}

TEST(GeneralShapes, ARingOfStudsIsOneOutline) {
  const auto studs = shapes::radial(
      12, {.connect = path::Connect::None,
           .marks = {path::Mark::shape(shapes::circle().outline({8, 8}))},
           .uniform = true});
  const path::Outline drawn = studs.outline({200, 200});
  EXPECT_NEAR(drawn.area(), 12 * 3.14159265f * 16.0f, 2.0f);
  EXPECT_EQ(studs.points({200, 200}).size(), 12u);
}

TEST(GeneralShapes, EveryGeneralShapeCarriesTheModifiers) {
  const auto soft = shapes::star(5, 0.42f).cornered(8);
  EXPECT_EQ(soft, shapes::cornered(shapes::star(5, 0.42f), 8));
  EXPECT_NE(soft.outline(kBox), shapes::star(5, 0.42f).outline(kBox));
  const auto cut = shapes::polygon(6).cornered(10, {.shape = shapes::CornerShape::Bevel});
  EXPECT_NE(cut.outline(kBox), soft.outline(kBox));
  // Drawn about a centre at a radius: the square of side 2r there.
  const path::Rect placed = shapes::circle().at({300, 40}, 20).bounds();
  EXPECT_NEAR(placed.centre().x, 300.0f, 1e-3f);
  EXPECT_NEAR(placed.width(), 40.0f, 1e-3f);
}

TEST(GeneralShapes, AFittedOutlineFillsTheBox) {
  const auto fitted = shapes::svg("M0 0 L10 0 L10 20 Z");
  const path::Rect stretched = fitted.outline(kBox).bounds();
  EXPECT_NEAR(stretched.width(), 200.0f, 1e-3f);
  EXPECT_NEAR(stretched.height(), 120.0f, 1e-3f);
  const path::Rect kept = shapes::svg("M0 0 L10 0 L10 20 Z", true)
                              .outline(kBox)
                              .bounds();
  EXPECT_NEAR(kept.height(), 120.0f, 1e-3f);
  EXPECT_NEAR(kept.width(), 60.0f, 1e-3f);
}

TEST(GeneralShapes, AFlatFillAndALoftAreMeshes) {
  const mesh::Mesh flat = mesh::fill(shapes::annulus(0.5f).outline({100, 100}));
  ASSERT_FALSE(flat.indices.empty());
  for (const glm::vec3& p : flat.positions) EXPECT_EQ(p.z, 0.0f);
  const std::vector<glm::vec3> square{{-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1}};
  std::vector<glm::vec3> circle;
  for (int k = 0; k < 16; ++k) {
    const float a = 6.2831853f * (float)k / 16.0f;
    circle.push_back({std::cos(a), 2.0f, std::sin(a)});
  }
  const mesh::Mesh skin = mesh::loft({square, circle}, {.segmentsBetween = 3});
  EXPECT_EQ(skin.positions.size(), 5u * 17u + 2u);  // five rings, two hubs
  EXPECT_EQ(skin.indices.size() % 3, 0u);
}

}  // namespace
