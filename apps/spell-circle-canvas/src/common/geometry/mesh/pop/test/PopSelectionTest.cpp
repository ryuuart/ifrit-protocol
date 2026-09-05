/** @file
 * Naming a subset and acting on it: a selector writes a lane, a mask
 * scopes the filter after it, delete removes what the selector named, and
 * order permutes WHOLE points — every lane travelling with the point that
 * carried it.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "support/Loops.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;
using sigil::geometry::mesh::pop::test::flatRing;


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

TEST(Pop, DeleteRemovesThePointsASelectionNames) {
  const std::vector<glm::vec3> loop = flatRing(12, 200);
  const auto selected = [&](int count) {
    return pop::on(loop).count(count).select("east", {200, 0, 0}, 120);
  };
  const Cloud whole = pop::cook(selected(400));
  const Cloud dropped = pop::cook(pop::Chain(selected(400).drop("east")));
  const Cloud kept = pop::cook(pop::Chain(selected(400).keep("east")));

  const std::vector<glm::vec4>* east = whole.colorIf("east");
  ASSERT_TRUE(east);
  size_t named = 0;
  for (size_t i = 0; i < whole.size(); ++i)
    if ((*east)[i].x >= 0.5f) ++named;
  ASSERT_GT(named, 40u);
  ASSERT_LT(named, whole.size());

  EXPECT_EQ(kept.size(), named);
  EXPECT_EQ(dropped.size(), whole.size() - named);
  // Every lane travels with its point: the survivors are the same
  // positions the whole cook produced, in the same order.
  size_t at = 0;
  for (size_t i = 0; i < whole.size(); ++i) {
    if ((*east)[i].x < 0.5f) continue;
    ASSERT_LT(at, kept.size());
    EXPECT_EQ(kept.positions[at], whole.positions[i]);
    ++at;
  }
  // A mask nothing names deletes nothing, which is what keeps an
  // operator from emptying a set by omission.
  EXPECT_EQ(pop::cook(pop::Chain(selected(400).drop(""))).size(), whole.size());
}

namespace {

/** The same chain, with the sort switched on or off and its direction
 *  named — so what changes between two cooks is the permutation alone. */
pop::Chain scattered(bool sorted, bool descending) {
  const std::vector<glm::vec3> ring = flatRing(10, 180.0f);
  pop::Builder b = pop::on(ring);
  b.count(200).seed(3).spread(20).vary(0.4f).fade({0, 0, 0, 1}, {1, 1, 1, 1});
  if (sorted) b.order({0, 1, 0}, descending);
  return (pop::Chain)b;
}

}  // namespace

TEST(Pop, OrderReallyReordersByTheKeyItIsGiven) {
  const Cloud plain = pop::cook(scattered(false, false));
  const Cloud rising = pop::cook(scattered(true, false));
  ASSERT_EQ(plain.size(), 200u);
  ASSERT_EQ(rising.size(), 200u);
  for (size_t i = 1; i < rising.size(); ++i)
    EXPECT_LE(rising.positions[i - 1].y, rising.positions[i].y);
  size_t moved = 0;
  for (size_t i = 0; i < plain.size(); ++i)
    if (plain.positions[i] != rising.positions[i]) ++moved;
  EXPECT_GT(moved, 100u) << "the sort must actually reorder";
}

TEST(Pop, OrderPermutesWholePointsSoEveryLaneTravelsWithItsOwn) {
  // A sort that moved positions alone would shear a cloud's colours and
  // sizes onto the wrong points, which nothing downstream could detect.
  // Each sorted point is found by matching its position, so the check
  // knows nothing about the permutation itself.
  const Cloud plain = pop::cook(scattered(false, false));
  const Cloud rising = pop::cook(scattered(true, false));
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
}

TEST(Pop, DescendingIsTheAscendingPermutationReversed) {
  // The keys here are distinct, so there are no ties to make that
  // ambiguous. This is the painter-order spelling: farthest first.
  const Cloud rising = pop::cook(scattered(true, false));
  const Cloud falling = pop::cook(scattered(true, true));
  ASSERT_EQ(falling.size(), rising.size());
  for (size_t i = 0; i < falling.size(); ++i)
    EXPECT_EQ(falling.positions[i], rising.positions[rising.size() - 1 - i]);
}

TEST(Pop, PointOrderIsTheSweptPathSoSortingFormsADifferentCable) {
  // Sorting is an authoring operation with geometric consequences, not
  // only a draw-order adjustment: the swept sink threads the points in the
  // order it finds them.
  const Mesh unsorted =
      pop::cookSweep(scattered(false, false), pop::profile::circle(), false,
                     {.segments = 160, .scale = 4, .caps = true});
  const Mesh threaded =
      pop::cookSweep(scattered(true, false), pop::profile::circle(), false,
                     {.segments = 160, .scale = 4, .caps = true});
  ASSERT_EQ(unsorted.positions.size(), threaded.positions.size());
  float drift = 0;
  for (size_t i = 0; i < unsorted.positions.size(); ++i)
    drift += glm::length(unsorted.positions[i] - threaded.positions[i]);
  EXPECT_GT(drift, 1000.0f);
}
