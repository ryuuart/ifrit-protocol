/** @file
 * The filters that move points: smoothing undoes what noise did to the
 * local shape, a matrix moves P and turns Dir with it, peak slides every
 * point along its own direction, the deformers twist, taper and bend about
 * an axis, and normal units a direction lane and gives it one sense.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "support/Loops.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;
using sigil::geometry::mesh::pop::test::flatRing;


// Smooth must undo what noise did to the local shape of the path, measured
// as the summed discrete second difference along the points — the quantity
// that shows up as kinks in anything swept along them. Halving it is a loose
// bar deliberately: the check is that smoothing acts on neighbours at all,
// not that it reaches a particular amount.
TEST(Pop, SmoothHealsNoiseKinks) {
  const std::vector<glm::vec3> loop = flatRing(8, 250.0);
  const auto jaggedness = [](const Cloud& cloud) {
    double sum = 0;
    for (size_t i = 1; i + 1 < cloud.size(); ++i)
      sum += glm::length(cloud.positions[i - 1] - cloud.positions[i] * 2.0f +
                         cloud.positions[i + 1]);
    return sum;
  };
  const double rough =
      jaggedness(pop::cook(pop::on(loop).count(80).noise(30).chain()));
  const double healed = jaggedness(
      pop::cook(pop::on(loop).count(80).noise(30).smooth(0.6f, 3).chain()));
  EXPECT_LT(healed, rough * 0.5) << rough << " -> " << healed;
}

// NORMAL makes a direction lane a unit direction and gives every one of
// them the same sense. A ring's outward is what stands its stamps up the
// same way all the way round.
TEST(Pop, NormalUnitsADirectionLaneAndGivesItOneSense) {
  const std::vector<glm::vec3> loop = flatRing(12, 200);
  const Cloud cooked =
      pop::cook(pop::Chain(pop::on(loop)
                               .count(120)
                               .fill(pop::Lane::Dir, {3, 0, 0, 0})
                               .normal(1.0f, {0, 0, 0})));
  const std::vector<glm::vec3>* dir = cooked.vectorIf("dir");
  ASSERT_TRUE(dir);
  for (size_t i = 0; i < cooked.size(); ++i) {
    EXPECT_NEAR(glm::length((*dir)[i]), 1.0f, 1e-5f) << "point " << i;
    // +1 turns each direction away from the centre, so a point on the
    // -x side of the ring must carry -x and not the +x it was filled
    // with.
    EXPECT_GE(glm::dot((*dir)[i], glm::normalize(cooked.positions[i])), 0.0f)
        << "point " << i;
  }
  // A zero sense leaves the sense alone and only units the lane.
  const Cloud plain = pop::cook(pop::Chain(
      pop::on(loop).count(120).fill(pop::Lane::Dir, {3, 0, 0, 0}).normal()));
  const std::vector<glm::vec3>* plainDir = plain.vectorIf("dir");
  ASSERT_TRUE(plainDir);
  for (const glm::vec3& d : *plainDir) EXPECT_EQ(d, glm::vec3(1, 0, 0));
}

TEST(Pop, TransformAndPeakMovePointsAlongTheirFrame) {
  const std::vector<glm::vec3> loop = flatRing(12, 100);
  // A pure translation on P is Math's move; a rotation is not, and Dir
  // follows through orient() renormalized.
  const glm::mat4 turn = camera::place({0, 30, 0}, 90);
  const Cloud a = pop::cook(pop::on(loop).count(60).affine(turn));
  const Cloud b = pop::cook(pop::on(loop).count(60));
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    const glm::vec4 expected = turn * glm::vec4(b.positions[i], 1.0f);
    EXPECT_NEAR(a.positions[i].x, expected.x, 1e-3f);
    EXPECT_NEAR(a.positions[i].y, expected.y, 1e-3f);
    EXPECT_NEAR(a.positions[i].z, expected.z, 1e-3f);
  }
  const Cloud oriented =
      pop::cook(pop::on(loop).count(60).orient(camera::place({}, 90)));
  const std::vector<glm::vec3>* dirA = oriented.vectorIf("dir");
  const std::vector<glm::vec3>* dirB = b.vectorIf("dir");
  ASSERT_TRUE(dirA && dirB);
  for (size_t i = 0; i < 60; ++i) {
    EXPECT_NEAR(glm::length((*dirA)[i]), 1.0f, 1e-4f);
    // Yaw by 90 about +Y: (x, y, z) -> (z, y, -x).
    EXPECT_NEAR((*dirA)[i].x, (*dirB)[i].z, 1e-3f);
    EXPECT_NEAR((*dirA)[i].z, -(*dirB)[i].x, 1e-3f);
  }

  // Peak: on a loop scatter Dir is the tangent, so peaking slides every
  // point along the ring by the same distance — the radius holds.
  const Cloud peaked = pop::cook(pop::on(loop).count(60).peak(25));
  for (size_t i = 0; i < 60; ++i) {
    const float moved = glm::length(peaked.positions[i] - b.positions[i]);
    EXPECT_NEAR(moved, 25.0f, 1e-3f);
  }
  // Peak along a custom zero lane moves nothing.
  const Cloud still = pop::cook(pop::on(loop).count(60).peak(25, "nowhere"));
  for (size_t i = 0; i < 60; ++i)
    EXPECT_NEAR(glm::length(still.positions[i] - b.positions[i]), 0.0f, 1e-4f);
}

TEST(Pop, DeformersTwistTaperAndBend) {
  // A vertical column: points along y from 0 to 200, all at x = 50.
  std::vector<glm::vec3> loop = {
      {50, 0, 0}, {50, 200, 0}, {50, 200, 1}, {50, 0, 1}};
  const auto column = [&] {
    return pop::on(loop).count(200).window(0.5f, 0.5f);
  };
  const Cloud base = pop::cook(column());

  // Twist 180 degrees over 0..200: a point at the top lands at x = -50.
  const Cloud twisted = pop::cook(column().twist(180, {0, 1, 0}, 0, 200));
  for (size_t i = 0; i < 200; ++i) {
    const glm::vec3& p0 = base.positions[i];
    const glm::vec3& p1 = twisted.positions[i];
    EXPECT_NEAR(p1.y, p0.y, 1e-3f);
    EXPECT_NEAR(glm::length(glm::vec2{p1.x, p1.z}),
                glm::length(glm::vec2{p0.x, p0.z}), 1e-3f)
        << "twist preserves the radius";
    const float u = std::clamp(p0.y / 200.0f, 0.0f, 1.0f);
    const float ang = (float)M_PI * u;
    // Rodrigues about +Y: x' = x cos + z sin.
    EXPECT_NEAR(p1.x, p0.x * std::cos(ang) + p0.z * std::sin(ang), 1e-2f);
  }

  // Taper to 0.2 at the top: the radius shrinks linearly.
  const Cloud tapered = pop::cook(column().taper(0.2f, {0, 1, 0}, 0, 200));
  for (size_t i = 0; i < 200; ++i) {
    const glm::vec3& p0 = base.positions[i];
    const glm::vec3& p1 = tapered.positions[i];
    const float u = std::clamp(p0.y / 200.0f, 0.0f, 1.0f);
    EXPECT_NEAR(p1.x, p0.x * (1.0f + (0.2f - 1.0f) * u), 1e-2f);
    EXPECT_NEAR(p1.y, p0.y, 1e-3f);
  }

  // Bend 90 degrees toward +x over 0..200: the band's centreline
  // becomes a quarter circle of radius 200 * 2 / pi; the top of the
  // column ends up pointing along +x, at height R and x = R + offset
  // adjustment. Arc length is preserved for the x = 0 fibre.
  const std::vector<glm::vec3> spine = {
      {0, 0, 0}, {0, 200, 0}, {0, 200, 1}, {0, 0, 1}};
  const Cloud bent = pop::cook(pop::on(spine)
                                   .count(200)
                                   .window(0.5f, 0.5f)
                                   .bend(90, {0, 1, 0}, {1, 0, 0}, 0, 200));
  const Cloud spineBase =
      pop::cook(pop::on(spine).count(200).window(0.5f, 0.5f));
  const float R = 200.0f / ((float)M_PI * 0.5f);
  for (size_t i = 0; i < 200; ++i) {
    const glm::vec3& p0 = spineBase.positions[i];
    const glm::vec3& p1 = bent.positions[i];
    // The spline overshoots its control points a little at both ends;
    // points outside the band ride the end tangents rigidly, so only
    // the band itself is on the arc.
    if (p0.y < 0.0f || p0.y > 200.0f) continue;
    const float theta = p0.y / R;
    EXPECT_NEAR(p1.y, R * std::sin(theta), 1e-2f);
    EXPECT_NEAR(p1.x, R - R * std::cos(theta), 1e-2f);
    // Distance from the arc centre (x = R, y = 0) is R everywhere.
    EXPECT_NEAR(std::hypot(p1.x - R, p1.y), R, 1e-2f);
  }
  // Amount 0 is the identity.
  const Cloud unbent = pop::cook(
      pop::on(spine).count(200).window(0.5f, 0.5f).bend(0, {0, 1, 0}));
  for (size_t i = 0; i < 200; ++i)
    EXPECT_NEAR(glm::length(unbent.positions[i] - spineBase.positions[i]), 0.0f,
                1e-4f);
}
