#include <gtest/gtest.h>

#include "sigilgeometry/Easel.h"

using namespace sigil::geometry;

// An easel::Shape is a recipe, not a path: it stores the operations and cooks
// them on demand. Two properties follow, and both are what let a caller hold
// one and hand copies around — cooking is pure, and copies are independent.
TEST(Easel, ShapeRecipeCooksNonDestructively) {
  const easel::Shape recipe =
      easel::shape(easel::dot(50)).bloat(0.4f).offset(10);
  const SkPath once = recipe.path();
  const SkPath twice = recipe.path();
  EXPECT_EQ(once.countPoints(), twice.countPoints());
  EXPECT_GT(once.computeTightBounds().width(), 115);  // grew by ~offset
  // Extending a copy must not reach back into the original recipe.
  easel::Shape copy = recipe;
  copy.twirl(90);
  EXPECT_GT(copy.path().countPoints(), 0);
  EXPECT_EQ(recipe.path().countPoints(), once.countPoints());
}

TEST(Easel, BlendReadsLikeIllustrator) {
  const std::vector<blend::Step> steps =
      easel::blend(easel::star(5, 60), easel::dot(50))
          .colors({1, 0, 0, 1}, {0, 0, 1, 1})
          .steps(7)
          .between({0, 0}, {400, 0})
          .cook();
  // steps(n) counts INTERMEDIATES, as it does in a drawing program's blend
  // dialog: the two keys are always present on top of it.
  EXPECT_EQ(steps.size(), 9u);  // 2 keys + 7
  EXPECT_NEAR(steps.back().path.computeTightBounds().centerX(), 400, 2);
}

TEST(Easel, WireAndParticlesCook) {
  const easel::Wire arc = easel::wire({{-100, 0, 0}, {0, 80, 0}, {100, 0, 0}});
  EXPECT_GT(arc.tube(8).triangleCount(), 0u);
  EXPECT_EQ(arc.beads(12).size(), 12u);

  const Cloud sparks = easel::particles()
                           .on(arc)
                           .count(50)
                           .drift(10)
                           .ramp({1, 0, 0, 1}, {0, 0, 1, 1})
                           .cook();
  EXPECT_EQ(sparks.size(), 50u);
  ASSERT_TRUE(sparks.colorIf("tint"));
  EXPECT_NEAR(sparks.colorIf("tint")->front().r, 1, 1e-3);
  EXPECT_NEAR(sparks.colorIf("tint")->back().b, 1, 1e-3);
}
