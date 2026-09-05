/** @file
 * The lanes a chain carries: any NAME is an attribute, a table drives one
 * lane from another over an explicit domain, mixes and copies write new
 * ones, promote moves a point lane onto the primitives a point becomes, an
 * existing cloud enters a chain with every lane it already had, and the
 * seeded mixer under all of it is reproducible.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "support/Loops.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;
using sigil::geometry::mesh::pop::test::flatRing;


TEST(Pop, NamedAttributesFlowAndExport) {
  // Any NAME is an attribute: an operator that takes a lane takes a custom
  // one on equal terms with the built-in ones, so a lane can be created,
  // jittered and scaled by the same ops the standard lanes use, and it
  // exports under the name it was given.
  const std::vector<glm::vec3> loop = flatRing(8, 200.0);
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
  const std::vector<glm::vec3> loop = flatRing(8, 200.0);
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

TEST(Pop, TheSeededMixerIsReproducibleAndSeedSensitive) {
  // One mixer definition feeds both the pop operators and the point
  // generators, and the device kernels dispatch the same function. What a
  // caller may rely on is that a seed names a stream: the same chain and
  // the same seed cook the same lane every time, in this process and the
  // next, and a different seed cooks a different one. That two executors
  // of that definition agree BIT FOR BIT is the device tier's claim, and
  // it is made against the host rather than against a written-down number.
  const std::vector<glm::vec3> loop = flatRing(8, 100.0f);
  const auto jittered = [&](uint32_t seed) {
    const Cloud cooked = pop::cook(
        pop::on(loop).count(6).fill("h", {0, 0, 0, 0}).op(pop::Jitter{"h", 0.5f, seed}));
    const std::vector<glm::vec4>* h = cooked.colorIf("h");
    std::vector<float> out;
    if (h)
      for (const glm::vec4& v : *h) out.push_back(v.x);
    return out;
  };
  const std::vector<float> first = jittered(0);
  ASSERT_EQ(first.size(), 6u);
  EXPECT_EQ(first, jittered(0));
  EXPECT_NE(first, jittered(1));
  // Jitter of amplitude 0.5 on a zeroed lane stays inside that amplitude,
  // which is what makes the dial a bound rather than a scale factor.
  for (float v : first) {
    EXPECT_GE(v, -0.5f);
    EXPECT_LE(v, 0.5f);
  }

  // …and a scatter into the unit box deals from the same stream: the same
  // seed is the same box of points, a different seed a different one, and
  // every draw is inside the box it was told to fill.
  const Cloud box = points::scatterBox({0, 0, 0}, {1, 1, 1}, 4, /*seed=*/7);
  ASSERT_EQ(box.positions.size(), 4u);
  EXPECT_EQ(box.positions,
            points::scatterBox({0, 0, 0}, {1, 1, 1}, 4, 7).positions);
  EXPECT_NE(box.positions,
            points::scatterBox({0, 0, 0}, {1, 1, 1}, 4, 8).positions);
  for (const glm::vec3& p : box.positions) {
    EXPECT_GE(std::min({p.x, p.y, p.z}), 0.0f);
    EXPECT_LE(std::max({p.x, p.y, p.z}), 1.0f);
  }
}

// promote() moves a POINT lane onto the PRIMITIVES a point becomes: every
// triangle of a point's stamp carries that point's value. It is only
// meaningful where the output geometry can be traced back to a point, which
// is instancing — the swept sinks resample the chain into new geometry that
// no longer corresponds to points one for one, so they promote nothing.
TEST(Pop, PromoteCarriesPointLanesOntoPrimitives) {
  const std::vector<glm::vec3> loop = flatRing(8, 180.0);
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

  EXPECT_TRUE(pop::cookSweep(chain, pop::profile::circle(), false,
                             {.segments = 160, .scale = 4, .caps = true})
                  .prims.empty());
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
  pop::Lanes lanes;
  pop::seedAttrs(given, lanes);
  EXPECT_EQ(lanes.count("P"), 1u);
  EXPECT_EQ(lanes.count("Scale"), 1u);
  EXPECT_EQ(lanes.count("top"), 1u);
  EXPECT_EQ(lanes.count("size"), 0u);
  const std::vector<std::string> customs = pop::seedCustomNames(given);
  ASSERT_EQ(customs.size(), 1u);
  EXPECT_EQ(customs[0], "top");
}
