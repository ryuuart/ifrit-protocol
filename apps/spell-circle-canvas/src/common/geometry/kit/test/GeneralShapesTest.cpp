/** @file
 * The general shapes over a box and their stock values: a stock value is
 * the general form with its options fixed, so the two draw the same
 * outline; the modifiers every general shape carries; and the solids
 * that lift an outline flat or loft a run of sections.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>

#include <algorithm>
#include <array>
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
  const auto open =
      shapes::ellipse({.sweepDegrees = 90, .close = shapes::Close::Open})
          .outline({100, 100});
  const auto chord =
      shapes::ellipse({.sweepDegrees = 90, .close = shapes::Close::Chord})
          .outline({100, 100});
  const auto pie =
      shapes::ellipse({.sweepDegrees = 90, .close = shapes::Close::Pie})
          .outline({100, 100});
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
  const auto cut =
      shapes::polygon(6).cornered(10, {.shape = shapes::CornerShape::Bevel});
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
  const path::Rect kept =
      shapes::svg("M0 0 L10 0 L10 20 Z", true).outline(kBox).bounds();
  EXPECT_NEAR(kept.height(), 120.0f, 1e-3f);
  EXPECT_NEAR(kept.width(), 60.0f, 1e-3f);
}

TEST(GeneralShapes, AFlatFillAndALoftAreMeshes) {
  const mesh::Mesh flat = mesh::fill(shapes::annulus(0.5f).outline({100, 100}));
  ASSERT_FALSE(flat.indices.empty());
  for (const glm::vec3& p : flat.positions) EXPECT_EQ(p.z, 0.0f);
  const std::vector<glm::vec3> square{
      {-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1}};
  std::vector<glm::vec3> circle;
  for (int k = 0; k < 16; ++k) {
    const float a = 6.2831853f * (float)k / 16.0f;
    circle.push_back({std::cos(a), 2.0f, std::sin(a)});
  }
  const mesh::Mesh skin = mesh::loft({square, circle}, {.segmentsBetween = 3});
  // Five wall rings, plus an independent ring and hub for each cap.
  EXPECT_EQ(skin.positions.size(), 5u * 17u + 2u * 17u);
  EXPECT_EQ(skin.indices.size() % 3, 0u);
}

TEST(GeneralShapes, LoftCapsFollowTheSideWinding) {
  const std::vector<glm::vec3> square{
      {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, -1, -1}};
  for (bool reversed : {false, true}) {
    for (int between : {0, 2}) {
      for (bool caps : {false, true}) {
        std::vector<glm::vec3> first = square, last = square;
        if (reversed) {
          std::reverse(first.begin(), first.end());
          std::reverse(last.begin(), last.end());
        }
        for (auto& point : last) point.z = 1;
        const mesh::Mesh body = mesh::loft(
            {first, last}, {.segmentsBetween = between, .capEnds = caps});
        ASSERT_FALSE(body.indices.empty());
        const float winding = reversed ? -1.0f : 1.0f;
        size_t capTriangles = 0;
        for (size_t i = 0; i < body.indices.size(); i += 3) {
          const glm::vec3 a = body.positions[body.indices[i]];
          const glm::vec3 b = body.positions[body.indices[i + 1]];
          const glm::vec3 c = body.positions[body.indices[i + 2]];
          const glm::vec3 normal = glm::cross(b - a, c - a);
          EXPECT_GT(winding * glm::dot(normal, (a + b + c) / 3.0f), 0)
              << "triangle " << i / 3 << ", reversed " << reversed
              << ", intermediate rings " << between << ", caps " << caps;
          if (a.z == b.z && b.z == c.z) ++capTriangles;
        }
        EXPECT_EQ(capTriangles, caps ? 8u : 0u);
      }
    }
  }
}

TEST(GeneralShapes, ALoftRequiresARingInEverySection) {
  const std::vector<glm::vec3> square{
      {-1, -1, 0}, {-1, 1, 0}, {1, 1, 0}, {1, -1, 0}};
  EXPECT_TRUE(mesh::loft({}).positions.empty());
  EXPECT_TRUE(mesh::loft({square}).positions.empty());
  for (size_t count : {0u, 1u, 2u}) {
    const std::vector<glm::vec3> shortRing(square.begin(),
                                           square.begin() + count);
    for (bool closed : {false, true}) {
      for (size_t at : {0u, 1u, 2u}) {
        auto sections = std::vector<std::vector<glm::vec3>>{square, square};
        sections.insert(sections.begin() + at, shortRing);
        const mesh::Mesh result = mesh::loft(sections, {.closed = closed});
        EXPECT_TRUE(result.positions.empty());
        EXPECT_TRUE(result.indices.empty());
      }
    }
  }
}

TEST(GeneralShapes, LoftCapsKeepPlanarNormalsAndUsableUvs) {
  const std::array<glm::vec2, 4> corners{glm::vec2{-3, -1}, glm::vec2{-3, 1},
                                         glm::vec2{3, 1}, glm::vec2{3, -1}};
  const glm::vec3 origin{13, -7, 8};
  for (const glm::vec3 direction :
       {glm::vec3{0, 0, 1}, glm::vec3{2, 3, 5}, glm::vec3{0, 1, 0}}) {
    const glm::vec3 normal = glm::normalize(direction);
    const glm::vec3 up =
        std::abs(normal.y) > 0.9f ? glm::vec3{1, 0, 0} : glm::vec3{0, 1, 0};
    const glm::vec3 x = glm::normalize(glm::cross(up, normal));
    const glm::vec3 y = glm::cross(normal, x);
    for (const bool reversed : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "plane " << direction.x << ',' << direction.y << ','
                   << direction.z << ", reversed " << reversed);
      std::vector<glm::vec3> first, last;
      for (const glm::vec2 corner : corners) {
        const glm::vec3 inPlane = origin + x * corner.x + y * corner.y;
        first.push_back(inPlane - normal * 2.0f);
        last.push_back(inPlane + normal * 2.0f);
      }
      if (reversed) {
        std::reverse(first.begin(), first.end());
        std::reverse(last.begin(), last.end());
      }
      const mesh::Mesh walls =
          mesh::loft({first, last}, {.segmentsBetween = 1, .capEnds = false});
      const mesh::Mesh capped =
          mesh::loft({first, last}, {.segmentsBetween = 1, .capEnds = true});
      ASSERT_EQ(capped.normals.size(), capped.positions.size());
      ASSERT_EQ(capped.uvs.size(), capped.positions.size());
      ASSERT_EQ(capped.indices.size(), walls.indices.size() + 24);
      ASSERT_GE(capped.positions.size(), walls.positions.size());
      for (size_t i = 0; i < walls.positions.size(); ++i) {
        EXPECT_EQ(capped.positions[i], walls.positions[i]);
        EXPECT_EQ(capped.normals[i], walls.normals[i]);
        EXPECT_EQ(capped.uvs[i], walls.uvs[i]);
      }
      EXPECT_TRUE(std::equal(walls.indices.begin(), walls.indices.end(),
                             capped.indices.begin()));
      for (size_t end = 0; end < 2; ++end) {
        const float sign =
            (end == 0 ? -1.0f : 1.0f) * (reversed ? -1.0f : 1.0f);
        const glm::vec3 expectedNormal = normal * sign;
        float uvArea = 0;
        for (size_t triangle = 0; triangle < corners.size(); ++triangle) {
          const size_t at =
              walls.indices.size() + (end * corners.size() + triangle) * 3;
          for (size_t k = 0; k < 3; ++k) {
            const uint32_t index = capped.indices[at + k];
            EXPECT_GE(index, walls.positions.size());
            ASSERT_LT(index, capped.positions.size());
            EXPECT_NEAR(glm::dot(capped.normals[index], expectedNormal), 1,
                        1e-5f);
            const glm::vec2 uv = capped.uvs[index];
            EXPECT_TRUE(std::isfinite(uv.x) && std::isfinite(uv.y));
            EXPECT_GE(uv.x, -1e-5f);
            EXPECT_GE(uv.y, -1e-5f);
            EXPECT_LE(uv.x, 1 + 1e-5f);
            EXPECT_LE(uv.y, 1 + 1e-5f);
          }
          const glm::vec3 a = capped.positions[capped.indices[at]];
          const glm::vec3 b = capped.positions[capped.indices[at + 1]];
          const glm::vec3 c = capped.positions[capped.indices[at + 2]];
          EXPECT_GT(glm::dot(glm::cross(b - a, c - a), expectedNormal), 0);
          const glm::vec2 ab = capped.uvs[capped.indices[at + 1]] -
                               capped.uvs[capped.indices[at]];
          const glm::vec2 ac = capped.uvs[capped.indices[at + 2]] -
                               capped.uvs[capped.indices[at]];
          const float area = std::abs(ab.x * ac.y - ab.y * ac.x) * 0.5f;
          EXPECT_GT(area, 1e-5f);
          uvArea += area;
        }
        EXPECT_NEAR(uvArea, 1, 1e-5f);
      }
    }
  }
}

TEST(GeneralShapes, CollapsedLoftCapsKeepFiniteNormalsAndUvs) {
  for (const bool point : {false, true}) {
    SCOPED_TRACE(point);
    std::vector<glm::vec3> first{{-1, 0, -1}, {0, 0, -1}, {1, 0, -1}};
    if (point)
      for (glm::vec3& vertex : first) vertex.x = 0;
    std::vector<glm::vec3> last = first;
    for (glm::vec3& vertex : last) vertex.z = 1;
    const mesh::Mesh body = mesh::loft({first, last});
    ASSERT_FALSE(body.indices.empty());
    ASSERT_EQ(body.normals.size(), body.positions.size());
    ASSERT_EQ(body.uvs.size(), body.positions.size());
    for (const glm::vec3 normal : body.normals) {
      EXPECT_TRUE(std::isfinite(normal.x) && std::isfinite(normal.y) &&
                  std::isfinite(normal.z));
      EXPECT_NEAR(glm::length(normal), 1, 1e-5f);
    }
    for (const glm::vec2 uv : body.uvs) {
      EXPECT_TRUE(std::isfinite(uv.x) && std::isfinite(uv.y));
      EXPECT_GE(uv.x, 0);
      EXPECT_GE(uv.y, 0);
      EXPECT_LE(uv.x, 1);
      EXPECT_LE(uv.y, 1);
    }
  }
}

}  // namespace
