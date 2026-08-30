/** @file
 * Pop chains and their operators: the builder, every filter under a
 * mask, the selectors, the primitive and permutation classes, and the
 * sinks.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "sigilgeometry/mesh/codec/Encode.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using sigil::geometry::test::kCubeObj;

TEST(Pop, CookMeshFormsAModelFromAChain) {
  // A pop chain is a DESCRIPTION; cooking is what forms it. The output is a
  // plain Mesh, the same type the Skia painter and the GPU surface path
  // both take, so a chain needs no adapter to be drawn either way.
  // Cooking is also pure: the same chain cooks to the same model, and
  // editing the chain value is the only way to get a different one.
  pop::SplineScatter scatter;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    scatter.loop.emplace_back(200.0f * std::cos(a), 0, 200.0f * std::sin(a));
  }
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

TEST(Pop, SweptSinksBendWithTheChain) {
  // The chain's cooked POINTS are the path a sweep follows, so any operator
  // that moves points also bends every swept surface built from the chain.
  // The same description feeds a round profile and a flat one unchanged.
  pop::SplineScatter scatter;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    scatter.loop.emplace_back(300.0f * std::cos(a), 0, 300.0f * std::sin(a));
  }
  scatter.count = 96;
  scatter.head = 1;
  scatter.span = 1;
  pop::Chain chain = {scatter, pop::Noise{pop::Lane::P, 40, 0.01f, 5}};

  const Mesh tube = pop::cookSweep(chain, curve::profile::circle(10), true,
                                   {.segments = 200, .scale = 12});
  EXPECT_GT(tube.triangleCount(), 1000u);
  glm::vec3 lo, hi;
  tube.bounds(&lo, &hi);
  EXPECT_GT(hi.y - lo.y, 20.0f) << "noise must bend the sweep off-plane";
  // 600 across the scattered circle, plus the tube radius on each side;
  // the wide tolerance is the noise, which is free to push either way.
  EXPECT_NEAR(hi.x - lo.x, 624, 130);

  const Mesh ribbon =
      pop::cookSweep(chain, curve::profile::line(), true,
                     {.segments = 160,
                      .scale = 60,
                      .normals = curve::SweepOptions::Normals::Frame});
  EXPECT_GT(ribbon.triangleCount(), 200u);

  chain.emplace_back(pop::Math{pop::Lane::P, {1, 1, 1, 1}, {0, 900, 0, 0}});
  const Mesh lifted = pop::cookSweep(chain, curve::profile::circle(10), true,
                                     {.segments = 160, .scale = 12});
  glm::vec3 lo2, hi2;
  lifted.bounds(&lo2, &hi2);
  EXPECT_GT(lo2.y, hi.y + 400.0f) << "value edit re-forms the model high";
}

TEST(Pop, ArtistSpellingReadsLikeTouchDesigner) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(250.0f * std::cos(a), 0, 250.0f * std::sin(a));
  }
  // The builder spelling is one expression: an entry verb, the operators,
  // and a terminal verb that cooks. Every parameter has a default, so a
  // chain can be written without naming any of them.
  const Mesh wobble = pop::on(loop).count(64).noise(30).sweep(
      curve::profile::circle(8), true, {.segments = 160, .scale = 10});
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

// Smooth must undo what noise did to the local shape of the path, measured
// as the summed discrete second difference along the points — the quantity
// that shows up as kinks in anything swept along them. Halving it is a loose
// bar deliberately: the check is that smoothing acts on neighbours at all,
// not that it reaches a particular amount.
TEST(Pop, SmoothHealsNoiseKinks) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(250.0f * std::cos(a), 0, 250.0f * std::sin(a));
  }
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

TEST(Pop, SweepCarriesAnyProfileAlongTheChain) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(220.0f * std::cos(a), 0, 220.0f * std::sin(a));
  }
  SkPathBuilder starProfile;
  for (int i = 0; i < 10; ++i) {
    const float r = i % 2 == 0 ? 24.0f : 10.0f;
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    const SkPoint p = {r * std::cos(a), r * std::sin(a)};
    if (i == 0)
      starProfile.moveTo(p);
    else
      starProfile.lineTo(p);
  }
  starProfile.close();
  const Mesh swept = pop::on(loop).count(60).smooth(0.4f).sweep(
      curve::profile::fromPath(starProfile.detach()), true,
      {.segments = 120, .normals = curve::SweepOptions::Normals::Geometric});
  EXPECT_GT(swept.triangleCount(), 1500u);
  glm::vec3 lo, hi;
  swept.bounds(&lo, &hi);
  // The swept profile is carried in the frame's normal plane, so the star's
  // longest arm adds its radius on each side of the 220 scatter circle...
  EXPECT_NEAR(hi.x - lo.x, 2 * (220 + 24), 30);
  // ...and stands out of the loop's own plane rather than lying flat in it.
  EXPECT_GT(hi.y - lo.y, 20.0f);
}

TEST(Pop, ChainsComposeIntoEachOther) {
  // A chain is accepted anywhere a path is: chain A's cooked points become
  // chain B's path, so operators compose without a separate combinator.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(220.0f * std::cos(a), 0, 220.0f * std::sin(a));
  }
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
          .sweep(curve::profile::circle(8), true, {.segments = 160, .scale = 6})
          .triangleCount(),
      500u);
}

TEST(Pop, ChainsSeedFromFormedModels) {
  // A formed Mesh is also a valid seed: scattering ON a cooked surface and
  // forming again closes the loop, so a model can be built in stages
  // without any stage being a special case.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(200.0f * std::cos(a), 0, 200.0f * std::sin(a));
  }
  const Mesh cable = pop::on(loop).count(64).noise(20).sweep(
      curve::profile::circle(8), true, {.segments = 160, .scale = 9});
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
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(80.0f * std::cos(a), 0, 80.0f * std::sin(a));
  }
  const Mesh cubes = pop::on(loop).count(24).vary(0.4f).stamps(cube);
  EXPECT_EQ(cubes.triangleCount(), 24u * cube.triangleCount());
}

TEST(Pop, NamedAttributesFlowAndExport) {
  // Any NAME is an attribute: an operator that takes a lane takes a custom
  // one on equal terms with the built-in ones, so a lane can be created,
  // jittered and scaled by the same ops the standard lanes use, and it
  // exports under the name it was given.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(200.0f * std::cos(a), 0, 200.0f * std::sin(a));
  }
  const pop::Chain chain =
      pop::on(loop)
          .count(64)
          .fill("energy", {0.5f, 0, 0, 0})
          .op(pop::Jitter{"energy", 0.25f, 5})
          .op(pop::Math{"energy", {2, 1, 1, 1}, {0, 0, 0, 0}});
  const Cloud cooked = pop::cook(chain);
  const std::vector<glm::vec4>* energy = cooked.colorIf("energy");
  ASSERT_TRUE(energy) << "customs must export under their own name";
  float lo = 1e9f, hi = -1e9f;
  for (const glm::vec4& e : *energy) {
    lo = std::min(lo, e.r);
    hi = std::max(hi, e.r);
  }
  EXPECT_GT(hi, lo + 0.1f);  // jittered, not constant
  // Jitter is bounded by its amplitude, so the lane stays in
  // 0.5 +/- 0.25 before the Math op doubles it into 0.5 .. 1.5.
  EXPECT_GT(lo, 0.4f);
  EXPECT_LT(hi, 1.6f);
}

TEST(Pop, RampByDrivesOneAttributeFromAnotherThroughATable) {
  // rampBy is a value curve, not a palette: one lane's component indexes a
  // table of stops over an EXPLICIT domain, and the result is written to
  // another lane. With three stops the interesting behaviour is between
  // them, so the checks sample the interpolation and the clamping, not just
  // the stop values.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(200.0f * std::cos(a), 0, 200.0f * std::sin(a));
  }
  const glm::vec4 lowStop{0, 0, 1, 1};
  const glm::vec4 midStop{0, 1, 0, 1};
  const glm::vec4 highStop{1, 0, 0, 1};

  // Height is authored by hand so every sample point is known: P.y is
  // set outright, then read back through the table over [-100, 100].
  const auto colorAtHeight = [&](float y) {
    const pop::Chain chain =
        pop::on(loop)
            .count(4)
            .fill(pop::Lane::P, {0, y, 0, 0})
            .rampBy(pop::Lane::P, 1, {lowStop, midStop, highStop}, -100, 100);
    const Cloud cooked = pop::cook(chain);
    const std::vector<glm::vec4>* tint = cooked.colorIf("tint");
    EXPECT_TRUE(tint);
    return tint ? (*tint)[0] : glm::vec4{0, 0, 0, 0};
  };

  // The stops themselves.
  EXPECT_NEAR(colorAtHeight(-100).b, lowStop.b, 1e-5f);
  EXPECT_NEAR(colorAtHeight(0).g, midStop.g, 1e-5f);
  EXPECT_NEAR(colorAtHeight(100).r, highStop.r, 1e-5f);
  // BETWEEN two stops: a quarter of the way up the domain is halfway from
  // the low stop to the mid stop, with nothing of the far stop mixed in. A
  // nearest-stop lookup would return a stop colour here instead.
  const glm::vec4 quarter = colorAtHeight(-50);
  EXPECT_NEAR(quarter.b, 0.5f, 1e-5f);
  EXPECT_NEAR(quarter.g, 0.5f, 1e-5f);
  EXPECT_NEAR(quarter.r, 0.0f, 1e-5f);
  const glm::vec4 threeQuarters = colorAtHeight(50);
  EXPECT_NEAR(threeQuarters.g, 0.5f, 1e-5f);
  EXPECT_NEAR(threeQuarters.r, 0.5f, 1e-5f);
  // ...and the domain CLAMPS at both ends rather than extrapolating.
  EXPECT_NEAR(colorAtHeight(-1e4f).b, lowStop.b, 1e-5f);
  EXPECT_NEAR(colorAtHeight(1e4f).r, highStop.r, 1e-5f);
  EXPECT_NEAR(colorAtHeight(1e4f).g, 0.0f, 1e-5f);

  // Custom lanes work at BOTH ends: read a named lane, write a named lane,
  // and the destination is created if it does not exist yet. The stops are
  // ordinary values, not colours, so a lookup can drive any quantity.
  const pop::Chain custom =
      pop::on(loop)
          .count(16)
          .fill("energy", {0.25f, 0, 0, 0})
          .rampBy("energy", 0, {{10, 0, 0, 0}, {20, 0, 0, 0}}, 0, 1, "heat");
  const Cloud cooked = pop::cook(custom);
  const std::vector<glm::vec4>* heat = cooked.colorIf("heat");
  ASSERT_TRUE(heat) << "a lookup must create the lane it writes";
  EXPECT_NEAR((*heat)[0].r, 12.5f, 1e-4f);
}

TEST(Pop, OrderPutsTheWholePointInDrawOrder) {
  // Sorting is a PERMUTATION of whole points. Two things must hold: the key
  // really orders them, and every lane travels with its own point — a sort
  // that moved positions alone would shear a cloud's colours and sizes onto
  // the wrong points, which nothing downstream could detect.
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    loop.emplace_back(180.0f * std::cos(a), 40.0f * std::sin(3 * a),
                      180.0f * std::sin(a));
  }
  const auto describe = [&](bool sorted, bool descending) {
    pop::Builder b = pop::on(loop);
    b.count(200).seed(3).spread(20).vary(0.4f).fade({0, 0, 0, 1}, {1, 1, 1, 1});
    if (sorted) b.order({0, 1, 0}, descending);
    return (pop::Chain)b;
  };

  const Cloud plain = pop::cook(describe(false, false));
  const Cloud rising = pop::cook(describe(true, false));
  ASSERT_EQ(plain.size(), 200u);
  ASSERT_EQ(rising.size(), 200u);

  // 1. The order is real, and it is not the order it started in.
  for (size_t i = 1; i < rising.size(); ++i)
    EXPECT_LE(rising.positions[i - 1].y, rising.positions[i].y);
  size_t moved = 0;
  for (size_t i = 0; i < plain.size(); ++i)
    if (plain.positions[i] != rising.positions[i]) ++moved;
  EXPECT_GT(moved, 100u) << "the sort must actually reorder";

  // 2. Coherence: each sorted point's t/size/tint are the ones its
  // ORIGINAL position carried. Found by matching position, so the
  // check knows nothing about the permutation itself.
  const std::vector<float>* plainT = plain.scalarIf("t");
  const std::vector<float>* risingT = rising.scalarIf("t");
  const std::vector<float>* plainSize = plain.scalarIf("size");
  const std::vector<float>* risingSize = rising.scalarIf("size");
  const std::vector<glm::vec4>* plainTint = plain.colorIf("tint");
  const std::vector<glm::vec4>* risingTint = rising.colorIf("tint");
  ASSERT_TRUE(plainT && risingT && plainSize && risingSize && plainTint &&
              risingTint);
  size_t matched = 0;
  for (size_t i = 0; i < rising.size(); ++i)
    for (size_t j = 0; j < plain.size(); ++j)
      if (rising.positions[i] == plain.positions[j]) {
        EXPECT_FLOAT_EQ((*risingT)[i], (*plainT)[j]);
        EXPECT_FLOAT_EQ((*risingSize)[i], (*plainSize)[j]);
        EXPECT_FLOAT_EQ((*risingTint)[i].r, (*plainTint)[j].r);
        ++matched;
        break;
      }
  EXPECT_EQ(matched, rising.size()) << "every point must survive";

  // 3. Descending is exactly the ascending permutation reversed. The keys
  // here are distinct, so there are no ties to make that ambiguous. This is
  // the painter-order spelling: farthest first.
  const Cloud falling = pop::cook(describe(true, true));
  ASSERT_EQ(falling.size(), rising.size());
  for (size_t i = 0; i < falling.size(); ++i)
    EXPECT_EQ(falling.positions[i], rising.positions[rising.size() - 1 - i]);

  // 4. Point order IS the swept path, so sorting the same points forms a
  // genuinely different cable. Sorting is therefore an authoring operation
  // with geometric consequences, not just a draw-order adjustment.
  const Mesh unsorted =
      pop::cookSweep(describe(false, false), curve::profile::circle(), false,
                     {.segments = 160, .scale = 4, .caps = true});
  const Mesh threaded =
      pop::cookSweep(describe(true, false), curve::profile::circle(), false,
                     {.segments = 160, .scale = 4, .caps = true});
  ASSERT_EQ(unsorted.positions.size(), threaded.positions.size());
  float drift = 0;
  for (size_t i = 0; i < unsorted.positions.size(); ++i)
    drift += glm::length(unsorted.positions[i] - threaded.positions[i]);
  EXPECT_GT(drift, 1000.0f);
}

TEST(Pop, SharedPcgHashKeepsBothConsumersBitStable) {
  // One hash definition feeds both the pop operators and the point
  // generators, and the GPU kernels reimplement the same function. These
  // goldens are the exact bit patterns it produces: a single-bit change to
  // the hash would desync the CPU results from the GPU ones with no other
  // symptom, since both would still look like plausible randomness.
  //
  // The values are readable straight out of the definition. Jitter with
  // amplitude 0.5 on a zeroed lane is hash1(i * 3 + seed) - 0.5 with
  // nothing else mixed in...
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(100.0f * std::cos(a), 0, 100.0f * std::sin(a));
  }
  const Cloud cooked = pop::cook(pop::on(loop)
                                     .count(6)
                                     .fill("h", {0, 0, 0, 0})
                                     .op(pop::Jitter{"h", 0.5f, 0}));
  const std::vector<glm::vec4>* h = cooked.colorIf("h");
  ASSERT_TRUE(h);
  ASSERT_EQ(h->size(), 6u);
  const float popGolden[6] = {0.231199384f,  -0.441547632f, -0.184789419f,
                              0.0919363499f, 0.274898827f,  0.0884094238f};
  for (size_t i = 0; i < 6; ++i)
    EXPECT_NEAR((*h)[i].x, popGolden[i], 1e-7f) << "pop hash lane " << i;

  // ...and a scatter into the unit box is the raw 0..1 stream in order,
  // three draws per point.
  const Cloud box = points::scatterBox({0, 0, 0}, {1, 1, 1}, 4, /*seed=*/7);
  ASSERT_EQ(box.positions.size(), 4u);
  const float pointsGolden[12] = {0.985658824f,  0.420034766f,  0.98710376f,
                                  0.480089128f,  0.151413783f,  0.589045703f,
                                  0.263890147f,  0.0428663865f, 0.913271964f,
                                  0.0273712128f, 0.317990124f,  0.787527025f};
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(box.positions[i].x, pointsGolden[i * 3], 1e-7f);
    EXPECT_NEAR(box.positions[i].y, pointsGolden[i * 3 + 1], 1e-7f);
    EXPECT_NEAR(box.positions[i].z, pointsGolden[i * 3 + 2], 1e-7f);
  }
}

TEST(Pop, AtlasTexHintsRemapStampUvs) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(150.0f * std::cos(a), 0, 150.0f * std::sin(a));
  }
  const pop::Chain chain = pop::on(loop).count(40).atlas(2, 2);
  const Mesh stamped = pop::cookMesh(chain, mesh::quad(8, 8));
  // atlas(2, 2) divides the texture into a 2x2 grid and assigns each point
  // one cell, remapping its stamp's uvs into that cell. So every stamp's
  // uvs span exactly half the range in each axis and sit wholly inside one
  // cell — a stamp straddling a cell edge would sample two sprites at once.
  const size_t stampVerts = mesh::quad(8, 8).vertexCount();
  int cellsSeen[4] = {0, 0, 0, 0};
  for (size_t p = 0; p < 40; ++p) {
    float uMin = 2, uMax = -1, vMin = 2, vMax = -1;
    for (size_t v = 0; v < stampVerts; ++v) {
      const glm::vec2 uv = stamped.uvs[p * stampVerts + v];
      uMin = std::min(uMin, uv.x);
      uMax = std::max(uMax, uv.x);
      vMin = std::min(vMin, uv.y);
      vMax = std::max(vMax, uv.y);
    }
    EXPECT_NEAR(uMax - uMin, 0.5f, 1e-4f);
    EXPECT_NEAR(vMax - vMin, 0.5f, 1e-4f);
    cellsSeen[(uMin > 0.25f ? 1 : 0) + (vMin > 0.25f ? 2 : 0)]++;
  }
  // Every one of the 40 points fell into one of the four cells.
  EXPECT_GT(cellsSeen[0] + cellsSeen[1] + cellsSeen[2] + cellsSeen[3], 39);
  int distinct = 0;
  for (int c : cellsSeen) distinct += c > 0 ? 1 : 0;
  EXPECT_GE(distinct, 3) << "the hash should spread across cells";
}

// promote() moves a POINT lane onto the PRIMITIVES a point becomes: every
// triangle of a point's stamp carries that point's value. It is only
// meaningful where the output geometry can be traced back to a point, which
// is instancing — the swept sinks resample the chain into new geometry that
// no longer corresponds to points one for one, so they promote nothing.
TEST(Pop, PromoteCarriesPointLanesOntoPrimitives) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(180.0f * std::cos(a), 0, 180.0f * std::sin(a));
  }
  const int kPoints = 24;
  const Mesh stamp = mesh::quad(6, 6);
  const pop::Chain chain = pop::on(loop)
                               .count(kPoints)
                               .fade({1, 0, 0, 1}, {0, 0, 1, 1})
                               .vary(0.5f)
                               .promote(pop::Lane::Color)
                               .promote("Id", "Id")
                               .promote(pop::Lane::Scale, "size");

  // Cooking to a Cloud is unaffected: points have no primitives, so a
  // promote in the chain is simply inert there.
  const Cloud cooked = pop::cook(chain);
  ASSERT_EQ(cooked.size(), (size_t)kPoints);

  const Mesh model = pop::cookMesh(chain, stamp);
  const size_t perStamp = stamp.triangleCount();
  ASSERT_EQ(model.triangleCount(), (size_t)kPoints * perStamp);

  const std::vector<glm::vec4>* color = model.primIf("Color");
  const std::vector<glm::vec4>* id = model.primIf("Id");
  const std::vector<glm::vec4>* size = model.primIf("size");
  ASSERT_TRUE(color && id && size);
  ASSERT_EQ(color->size(), model.triangleCount());

  const std::vector<glm::vec4>* tint = cooked.colorIf("tint");
  const std::vector<float>* sizes = cooked.scalarIf("size");
  ASSERT_TRUE(tint && sizes);
  for (size_t p = 0; p < (size_t)kPoints; ++p)
    for (size_t k = 0; k < perStamp; ++k) {
      const size_t tri = p * perStamp + k;
      // Every triangle of a stamp carries its owning POINT's values...
      EXPECT_EQ((*color)[tri], (*tint)[p]);
      EXPECT_FLOAT_EQ((*size)[tri].x, (*sizes)[p]);
      // ..."Id" is the owning point's index, so every triangle of one stamp
      // reports the same id and different stamps report different ones —
      // this is what lets a shader address a whole instance.
      EXPECT_FLOAT_EQ((*id)[tri].x, (float)p);
    }
  // The source lane really did vary along the chain, so the equalities
  // above are not comparing a constant against itself.
  EXPECT_GT((*color)[model.triangleCount() - 1].b, (*color)[0].b + 0.5f);

  EXPECT_TRUE(pop::cookSweep(chain, curve::profile::circle(), false,
                             {.segments = 160, .scale = 4, .caps = true})
                  .prims.empty());
}

namespace {

std::vector<glm::vec3> flatRing(int n, float radius) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < n; ++i) {
    const float a = (float)i / (float)n * 2.0f * (float)M_PI;
    loop.emplace_back(radius * std::cos(a), 0, radius * std::sin(a));
  }
  return loop;
}

}  // namespace

TEST(Pop, GroupWritesASelectionAndMasksTheNextFilter) {
  // A ring in the xz plane; a sphere selector around +x picks the points
  // on that side and nowhere else; a masked Math then lifts ONLY those,
  // and an unmasked point stays on the floor. Feather grades the edge.
  const std::vector<glm::vec3> loop = flatRing(12, 200);
  const pop::Chain chain = pop::on(loop)
                               .count(400)
                               .select("east", {200, 0, 0}, 120)
                               .move({0, 50, 0})
                               .masked("east");
  const Cloud cooked = pop::cook(chain);
  const std::vector<glm::vec4>* east = cooked.colorIf("east");
  ASSERT_TRUE(east);
  int lifted = 0, grounded = 0;
  for (size_t i = 0; i < cooked.size(); ++i) {
    const float sel = (*east)[i].x;
    EXPECT_TRUE(sel == 0.0f || sel == 1.0f) << "hard edge selects 0/1";
    if (sel == 1.0f) {
      EXPECT_NEAR(cooked.positions[i].y, 50.0f, 1e-3f);
      EXPECT_GT(cooked.positions[i].x, 80.0f);
      ++lifted;
    } else {
      EXPECT_NEAR(cooked.positions[i].y, 0.0f, 1e-3f);
      ++grounded;
    }
  }
  EXPECT_GT(lifted, 40);
  EXPECT_GT(grounded, 200);

  // Feathered: values in between exist, and the blend is proportional.
  const pop::Chain soft = pop::on(loop)
                              .count(400)
                              .select("east", {200, 0, 0}, 160, 0.6f)
                              .move({0, 50, 0})
                              .masked("east");
  const Cloud softCooked = pop::cook(soft);
  const std::vector<glm::vec4>* softEast = softCooked.colorIf("east");
  ASSERT_TRUE(softEast);
  int partial = 0;
  for (size_t i = 0; i < softCooked.size(); ++i) {
    const float sel = (*softEast)[i].x;
    EXPECT_NEAR(softCooked.positions[i].y, 50.0f * sel, 1e-3f);
    if (sel > 0.05f && sel < 0.95f) ++partial;
  }
  EXPECT_GT(partial, 10) << "the feather band must grade";

  // Combine: a second box selector UNIONS the west side in.
  pop::Chain both = chain;
  both.insert(both.begin() + 2, pop::Select{"east",
                                            pop::Select::Shape::Box,
                                            {-200, 0, 0},
                                            {120, 400, 120},
                                            0,
                                            false,
                                            pop::Select::Combine::Union});
  const Cloud unioned = pop::cook(both);
  int liftedBoth = 0;
  for (const glm::vec3& p : unioned.positions) liftedBoth += p.y > 25.0f;
  EXPECT_GT(liftedBoth, lifted + 40);

  // A mask naming a lane nothing wrote selects nobody.
  const Cloud nobody =
      pop::cook(pop::on(loop).count(50).move({0, 50, 0}).masked("ghost"));
  for (const glm::vec3& p : nobody.positions) EXPECT_NEAR(p.y, 0.0f, 1e-4f);
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

TEST(Pop, MixBlendsCopiesAndFadesByALane) {
  const std::vector<glm::vec3> loop = flatRing(8, 100);
  const pop::Chain chain = pop::on(loop)
                               .count(40)
                               .fill("a", {1, 0, 0, 1})
                               .fill("b", {0, 0, 1, 1})
                               .mix("a", "b", "half", 0.5f)
                               .copy("a", "again")
                               .mixBy("a", "b", "byT", "T");
  const Cloud cooked = pop::cook(chain);
  const std::vector<glm::vec4>* half = cooked.colorIf("half");
  const std::vector<glm::vec4>* again = cooked.colorIf("again");
  const std::vector<glm::vec4>* byT = cooked.colorIf("byT");
  const std::vector<float>* t = cooked.scalarIf("t");
  ASSERT_TRUE(half && again && byT && t);
  for (size_t i = 0; i < 40; ++i) {
    EXPECT_NEAR((*half)[i].r, 0.5f, 1e-5f);
    EXPECT_NEAR((*half)[i].b, 0.5f, 1e-5f);
    EXPECT_NEAR((*again)[i].r, 1.0f, 1e-5f);
    EXPECT_NEAR((*byT)[i].b, (*t)[i], 1e-5f);
    EXPECT_NEAR((*byT)[i].r, 1.0f - (*t)[i], 1e-5f);
  }
}

TEST(Pop, PointSetSeedsAChainFromAnExistingCloudLanesAndAll) {
  // A cloud with the conventional lanes and a custom one (a Houdini
  // group, say) enters a chain as-is: positions become P, "size" Scale,
  // "tint" Color, "normal" Dir, and "top" a custom attribute — usable
  // straight away as a mask. Filters then run over it like any chain.
  Cloud given;
  for (int i = 0; i < 40; ++i) {
    given.positions.emplace_back((float)i * 10, i % 2 ? 100.0f : 0.0f, 0);
  }
  std::vector<float>& size = given.scalar("size", 1);
  std::vector<glm::vec4>& tint = given.color("tint");
  std::vector<glm::vec3>& normal = given.vector("normal");
  std::vector<float>& top = given.scalar("top");
  for (int i = 0; i < 40; ++i) {
    size[(size_t)i] = 2.0f + (float)(i % 3);
    tint[(size_t)i] = {1, 0, 0, 1};
    normal[(size_t)i] = {0, 1, 0};
    top[(size_t)i] = i % 2 ? 1.0f : 0.0f;
  }
  const pop::Chain chain = pop::on(given)
                               .move({0, 0, 50})
                               .masked("top")
                               .peak(5)
                               .fade({0, 1, 0, 1}, {0, 1, 0, 1});
  const Cloud cooked = pop::cook(chain);
  ASSERT_EQ(cooked.size(), 40u);
  const std::vector<float>* outSize = cooked.scalarIf("size");
  const std::vector<glm::vec3>* dir = cooked.vectorIf("dir");
  const std::vector<glm::vec4>* outTint = cooked.colorIf("tint");
  const std::vector<glm::vec4>* outTop = cooked.colorIf("top");
  ASSERT_TRUE(outSize && dir && outTint && outTop);
  for (size_t i = 0; i < 40; ++i) {
    // The mask came in with the cloud: only odd points moved in z.
    EXPECT_NEAR(cooked.positions[i].z, (i % 2 ? 50.0f : 0.0f), 1e-4f) << i;
    // Peak rides Dir, seeded from "normal": +5 in y for everyone.
    EXPECT_NEAR(cooked.positions[i].y, (i % 2 ? 105.0f : 5.0f), 1e-4f) << i;
    EXPECT_FLOAT_EQ((*outSize)[i], 2.0f + (float)(i % 3));
    EXPECT_FLOAT_EQ((*outTint)[i].g, 1.0f);  // recoloured by the fade
    EXPECT_FLOAT_EQ((*outTop)[i].x, i % 2 ? 1.0f : 0.0f);
    EXPECT_FLOAT_EQ((*dir)[i].y, 1.0f);
  }
  // count() and window() are inert on a point set: the count is the
  // cloud's.
  EXPECT_EQ(pop::cook(pop::on(given).count(5).window(0.5f, 0.5f)).size(), 40u);
  // The layout the GPU executor uploads is the same function.
  std::map<std::string, std::vector<glm::vec4>, std::less<>> lanes;
  pop::seedAttrs(given, lanes);
  EXPECT_EQ(lanes.count("P"), 1u);
  EXPECT_EQ(lanes.count("Scale"), 1u);
  EXPECT_EQ(lanes.count("top"), 1u);
  EXPECT_EQ(lanes.count("size"), 0u);
  const std::vector<std::string> customs = pop::seedCustomNames(given);
  ASSERT_EQ(customs.size(), 1u);
  EXPECT_EQ(customs[0], "top");
}

TEST(Pop, FieldsAreAddressableByName) {
  // The dial door: any operator's numeric field by its own name, vector
  // components dotted, enums and bools as numbers; a name the operator
  // lacks is refused and leaves it untouched.
  pop::Op twist = pop::Deform{};
  EXPECT_TRUE(pop::setField(twist, "amount", 45.0f));
  EXPECT_TRUE(pop::setField(twist, "origin.x", 12.0f));
  EXPECT_TRUE(pop::setField(twist, "kind", (float)pop::Deform::Kind::Bend));
  EXPECT_FALSE(pop::setField(twist, "wibble", 1.0f));
  const auto& d = std::get<pop::Deform>(twist);
  EXPECT_FLOAT_EQ(d.amount, 45.0f);
  EXPECT_FLOAT_EQ(d.origin.x, 12.0f);
  EXPECT_EQ(d.kind, pop::Deform::Kind::Bend);
  const std::optional<float> amount = pop::getField(twist, "amount");
  ASSERT_TRUE(amount.has_value());
  EXPECT_FLOAT_EQ(*amount, 45.0f);
  const std::optional<float> kind = pop::getField(twist, "kind");
  ASSERT_TRUE(kind.has_value());
  EXPECT_FLOAT_EQ(*kind, 2.0f);
  EXPECT_FALSE(pop::getField(twist, "mask"));  // a string, not a dial

  pop::Op group = pop::Select{};
  EXPECT_TRUE(pop::setField(group, "center.y", 80.0f));
  EXPECT_TRUE(pop::setField(group, "invert", 1.0f));
  EXPECT_TRUE(
      pop::setField(group, "combine", (float)pop::Select::Combine::Union));
  const auto& g = std::get<pop::Select>(group);
  EXPECT_FLOAT_EQ(g.center.y, 80.0f);
  EXPECT_TRUE(g.invert);
  EXPECT_EQ(g.combine, pop::Select::Combine::Union);

  pop::Op ramp = pop::Ramp{};
  EXPECT_TRUE(pop::setField(ramp, "to.g", 0.25f));  // colour spelling
  EXPECT_FLOAT_EQ(std::get<pop::Ramp>(ramp).to.y, 0.25f);
  const std::optional<float> toY = pop::getField(ramp, "to.y");
  ASSERT_TRUE(toY.has_value());
  EXPECT_FLOAT_EQ(*toY, 0.25f);

  pop::Op scatter = pop::SplineScatter{};
  EXPECT_TRUE(pop::setField(scatter, "count", 250.7f));  // int truncates
  EXPECT_EQ(std::get<pop::SplineScatter>(scatter).count, 250);
  EXPECT_TRUE(pop::setField(scatter, "seed", 9.0f));
  EXPECT_EQ(std::get<pop::SplineScatter>(scatter).seed, 9u);

  // Operators without dials say no to everything.
  pop::Op promote = pop::Promote{};
  EXPECT_FALSE(pop::setField(promote, "to", 1.0f));
  pop::Op given = pop::PointSet{};
  EXPECT_FALSE(pop::getField(given, "count"));
}

// The swept sink cooks the chain's points into a Catmull-Rom path and
// hands that, with the profile, to curve::sweep. Below it is written
// longhand — the profile flattened off an SkPath, wrapped back onto its
// first point, rings formed in place and normals averaged from the
// triangles — so the forwarding can be held against it. The topology
// and every uv agree exactly. The positions agree to within one
// rounding step, and the direction of that step is deliberate: the
// primitive assembles the profile's offset before adding the frame's
// position, where the longhand adds the position first and so spends a
// bit of the offset's precision on a spine far from the origin.
namespace {

Mesh referenceSweep(const pop::Chain& chain, const SkPath& profile, bool closed,
                    int segments) {
  curve::Spline3 spine;
  spine.points = pop::cook(chain).positions;
  spine.closed = closed;
  if (spine.points.size() < 2) return {};
  const std::vector<path::Polyline> contours = path::flatten(profile, 0.4f);
  if (contours.empty() || contours[0].points.size() < 3) return {};
  const std::vector<glm::vec2>& ring = contours[0].points;
  const std::vector<curve::Frame3> rail =
      curve::frames(spine, std::max(segments, 2), {0, 1, 0});

  Mesh out;
  const uint32_t n = (uint32_t)ring.size();
  for (const curve::Frame3& f : rail)
    for (uint32_t i = 0; i < n; ++i) {
      const glm::vec2 p = ring[i];
      out.positions.push_back(f.position + f.binormal * p.x - f.normal * p.y);
      out.uvs.emplace_back((float)i / (float)n, f.t);
    }
  for (uint32_t s = 0; s + 1 < (uint32_t)rail.size(); ++s)
    for (uint32_t i = 0; i < n; ++i) {
      const uint32_t j = (i + 1) % n;
      const uint32_t a = s * n + i, b = s * n + j;
      const uint32_t c = (s + 1) * n + i, d = (s + 1) * n + j;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }
  out.computeNormals();
  return out;
}

SkPath starProfile() {
  SkPathBuilder b;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    const float r = (i % 2 == 0) ? 24.0f : 10.0f;
    const SkPoint p = {std::cos(a) * r, std::sin(a) * r};
    if (i == 0)
      b.moveTo(p);
    else
      b.lineTo(p);
  }
  b.close();
  return b.detach();
}

}  // namespace

TEST(Pop, SweptSinkForwardsToTheSweptPrimitive) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI;
    loop.emplace_back(220.0f * std::cos(a), 0, 220.0f * std::sin(a));
  }
  const pop::Chain chain = pop::on(loop).count(60).noise(18).smooth(0.4f);

  for (const bool closed : {false, true}) {
    const Mesh made = pop::cookSweep(
        chain, curve::profile::fromPath(starProfile()), closed,
        {.segments = 120, .normals = curve::SweepOptions::Normals::Geometric});
    const Mesh want = referenceSweep(chain, starProfile(), closed, 120);
    ASSERT_EQ(made.positions.size(), want.positions.size());
    ASSERT_EQ(made.indices, want.indices);
    for (size_t i = 0; i < want.positions.size(); ++i) {
      // A float carrying a coordinate of a few hundred steps by about
      // 3e-5; the bound is two of those, and the normals are unit.
      EXPECT_LT(glm::length(made.positions[i] - want.positions[i]), 1e-4f)
          << "position " << i;
      EXPECT_LT(glm::length(made.normals[i] - want.normals[i]), 1e-4f)
          << "normal " << i;
      EXPECT_EQ(made.uvs[i], want.uvs[i]) << "uv " << i;
    }
  }

  // The sink is the chain's only geometric commitment: the same chain,
  // a different profile, and the model changes without the description
  // being touched.
  const Mesh cable = pop::cookSweep(chain, curve::profile::circle(10), true,
                                    {.segments = 120, .scale = 9});
  EXPECT_EQ(cable.vertexCount(), 120u * 11u);
  EXPECT_EQ(cable.normals.size(), cable.vertexCount());
  // A chain too short to be a path forms nothing rather than a
  // degenerate mesh.
  EXPECT_TRUE(pop::cookSweep(pop::Chain{}, curve::profile::circle(), false)
                  .positions.empty());
}
