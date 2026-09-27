/** @file
 * A 3D placement's matrix: the order its parts apply in.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/mesh/Transform.h>

#include <glm/vec4.hpp>

namespace {

using sigil::geometry::mesh::Transform;

void expectNear(glm::vec4 actual, glm::vec4 expected) {
  for (int component = 0; component < 4; ++component)
    EXPECT_NEAR(actual[component], expected[component], 1e-5f) << component;
}

TEST(MeshTransform, LeftAloneIsTheIdentity) {
  EXPECT_EQ(Transform{}.matrix(), glm::mat4(1.0f));
}

TEST(MeshTransform, ScalesThenTurnsThenMoves) {
  const Transform placed{.translate = {10, 0, 0},
                         .rotateDegrees = {0, 0, 90},
                         .scale = {2, 2, 2}};
  // (1, 0, 0) scaled to (2, 0, 0), turned a quarter about z to (0, 2, 0),
  // moved to (10, 2, 0).
  expectNear(placed.matrix() * glm::vec4(1, 0, 0, 1), {10, 2, 0, 1});
}

TEST(MeshTransform, TurnsAboutTheOrigin) {
  const Transform placed{.rotateDegrees = {0, 0, 180}, .origin = {1, 0, 0}};
  expectNear(placed.matrix() * glm::vec4(1, 0, 0, 1), {1, 0, 0, 1});
  expectNear(placed.matrix() * glm::vec4(0, 0, 0, 1), {2, 0, 0, 1});
}

TEST(MeshTransform, AZeroAxisTurnsNothing) {
  const Transform placed{.axis = {0, 0, 0}, .axisDegrees = 90};
  EXPECT_EQ(placed.matrix(), glm::mat4(1.0f));
}

}  // namespace
