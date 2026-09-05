/** @file
 * A pop chain is a DESCRIPTION and cooking is what forms it: the same
 * chain cooks to the same model, editing the value is the only way to get
 * a different one, the fluent spelling holds nothing the chain does not,
 * a chain is accepted anywhere a path is, and a formed or imported model
 * seeds one like any other.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "support/GeometrySupport.h"
#include "support/Loops.h"
#include <sigilgeometry/kit/Sections.h>

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;
using sigil::geometry::mesh::pop::test::flatRing;

using sigil::geometry::test::kCubeObj;

TEST(Pop, CookMeshFormsAModelFromAChain) {
  // A pop chain is a DESCRIPTION; cooking is what forms it. The output is a
  // plain Mesh, the same type the Skia painter and the GPU surface path
  // both take, so a chain needs no adapter to be drawn either way.
  // Cooking is also pure: the same chain cooks to the same model, and
  // editing the chain value is the only way to get a different one.
  pop::SplineScatter scatter;
  scatter.loop = flatRing(8, 200.0);
  scatter.count = 500;
  scatter.head = 1;
  scatter.span = 1;
  scatter.radius = 12;
  pop::Chain chain = {scatter, pop::Vary{pop::Lane::Scale, 1.0f, 0.4f, 3},
                      pop::Ramp{pop::Lane::Color, {1, 0, 0, 1}, {0, 0, 1, 1}}};
  const Mesh stamp = mesh::quad(6, 6);
  const Mesh model = pop::cookMesh(chain, stamp);
  EXPECT_EQ(model.vertexCount(), 500u * stamp.vertexCount());
  EXPECT_EQ(model.triangleCount(), 500u * stamp.triangleCount());
  ASSERT_EQ(model.colors.size(), model.vertexCount());  // tint baked
  const Mesh again = pop::cookMesh(chain, stamp);
  ASSERT_EQ(again.positions.size(), model.positions.size());
  EXPECT_EQ(again.positions[123].x, model.positions[123].x);
  chain.emplace_back(pop::Math{pop::Lane::P, {1, 1, 1, 1}, {0, 500, 0, 0}});
  const Mesh lifted = pop::cookMesh(chain, stamp);
  EXPECT_GT(lifted.positions[123].y, model.positions[123].y + 400.0f);
}

TEST(Pop, TheFluentSpellingIsTheChainValueInOneExpression) {
  const std::vector<glm::vec3> loop = flatRing(8, 250.0);
  // The builder spelling is one expression: an entry verb, the operators,
  // and a terminal verb that cooks. Every parameter has a default, so a
  // chain can be written without naming any of them.
  const Mesh wobble = pop::on(loop).count(64).noise(30).sweep(
      sections::circle(8), true, {.segments = 160, .scale = 10});
  EXPECT_GT(wobble.triangleCount(), 500u);

  // The builder holds nothing the chain does not: it converts to a
  // pop::Chain whose operators are still ordinary values, so a chain built
  // fluently can still be taken apart and edited afterwards.
  pop::Chain c = pop::on(loop).count(10).spread(5).vary(0.4f).fade(
      {1, 0, 0, 1}, {0, 0, 1, 1});
  EXPECT_EQ(c.size(), 3u);  // scatter + vary + ramp
  std::get<pop::SplineScatter>(c.front()).count = 20;
  EXPECT_EQ(pop::cook(c).size(), 20u);
}

TEST(Pop, ChainsComposeIntoEachOther) {
  // A chain is accepted anywhere a path is: chain A's cooked points become
  // chain B's path, so operators compose without a separate combinator.
  const std::vector<glm::vec3> loop = flatRing(8, 220.0);
  const pop::Chain spine = pop::on(loop).count(48).noise(26).smooth(0.5f);
  const Cloud beads = pop::on(spine).count(300).spread(6).cloud();
  EXPECT_EQ(beads.size(), 300u);
  // The beads inherit A's off-plane noise, which proves they were scattered
  // along the composed path and not along the flat circle it started from.
  float yMin = 1e9f, yMax = -1e9f;
  for (const glm::vec3& p : beads.positions) {
    yMin = std::min(yMin, p.y);
    yMax = std::max(yMax, p.y);
  }
  EXPECT_GT(yMax - yMin, 12.0f);
  // And any sink still applies to the composition.
  EXPECT_GT(
      pop::on(spine)
          .count(80)
          .sweep(sections::circle(8), true, {.segments = 160, .scale = 6})
          .triangleCount(),
      500u);
}

TEST(Pop, ChainsSeedFromFormedModels) {
  // A formed Mesh is also a valid seed: scattering ON a cooked surface and
  // forming again closes the loop, so a model can be built in stages
  // without any stage being a special case.
  const std::vector<glm::vec3> loop = flatRing(8, 200.0);
  const Mesh cable = pop::on(loop).count(64).noise(20).sweep(
      sections::circle(8), true, {.segments = 160, .scale = 9});
  const Cloud dust = pop::on(cable, 500).cloud();
  EXPECT_EQ(dust.size(), 500u);
  glm::vec3 mLo, mHi, dLo, dHi;
  cable.bounds(&mLo, &mHi);
  Mesh asPoints;
  asPoints.positions = dust.positions;
  asPoints.bounds(&dLo, &dHi);
  EXPECT_GE(dLo.x, mLo.x - 1);
  EXPECT_LE(dHi.x, mHi.x + 1);  // dust lives on the cable
  EXPECT_GT(pop::on(cable, 300).stamps(mesh::quad(4, 4)).triangleCount(), 500u);
}

TEST(Pop, ImportedModelsJoinTheSystem) {
  // An imported model is an ordinary Mesh, so it can both SEED a chain (be
  // scattered on) and serve as a STAMP (be instanced along one). Nothing in
  // either path distinguishes a loaded mesh from a generated one.
  const std::string obj = kCubeObj;
  auto model = codec::decode::model(obj.data(), obj.size(), "cube.obj");
  ASSERT_TRUE(model.has_value());
  const Mesh cube = model->merged();

  // Scatter on the imported surface...
  const Cloud dust = pop::on(cube, 300).cloud();
  EXPECT_EQ(dust.size(), 300u);
  glm::vec3 lo, hi;
  Mesh asPoints;
  asPoints.positions = dust.positions;
  asPoints.bounds(&lo, &hi);
  EXPECT_GE(lo.x, -1.01f);
  EXPECT_LE(hi.x, 1.01f);  // points live on the unit cube

  // ...and use the imported model AS the stamp along a chain.
  const std::vector<glm::vec3> loop = flatRing(8, 80.0);
  const Mesh cubes = pop::on(loop).count(24).vary(0.4f).stamps(cube);
  EXPECT_EQ(cubes.triangleCount(), 24u * cube.triangleCount());
}
