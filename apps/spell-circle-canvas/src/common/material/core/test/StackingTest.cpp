/** @file
 * What `over()` stacks — the three operands as the result's children and
 * the walk back down — and the bank that folds seeds into buckets.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/Material.h>

#include <memory>
#include <string>

using namespace sigil::material;

namespace {

struct TwoParams {
  float uScale;
  Color uColor;
};

struct SeededParams {
  float seed;
  Color uColor;
};

std::shared_ptr<const Recipe> seededRecipe(const char* name = "seeded") {
  return std::make_shared<const Recipe>(Recipe::of<SeededParams>(name).body(
      Target::SkSL, "half4 main(float2 p) { return half4(uColor * seed); }"));
}

std::shared_ptr<const Recipe> twoRecipe(const char* name = "two") {
  return std::make_shared<const Recipe>(Recipe::of<TwoParams>(name).body(
      Target::SkSL, "half4 main(float2 p) { return half4(uColor * uScale); }"));
}

/** THE THREE OPERANDS every stacking case below is built from. */
struct Operands {
  std::shared_ptr<const Recipe> recipe = twoRecipe();
  Material base{recipe, TwoParams{1, {1, 0, 0, 1}}};
  Material top{recipe, TwoParams{1, {0, 0, 1, 1}}};
  Material mask{recipe, TwoParams{0.5f, {1, 1, 1, 1}}};
};

}  // namespace

TEST(Stacking, TheOperandsAreTheStacksChildren) {
  const Operands o;
  const Material stack = over(o.base, o.top, o.mask);
  // The operands are the result's children, so every query answers over
  // the whole stack.
  EXPECT_EQ(stack.children().size(), 3u);
  ASSERT_NE(stack.child("base"), nullptr);
  EXPECT_EQ(*stack.child("base"), o.base);
  EXPECT_EQ(*stack.child("top"), o.top);
  EXPECT_EQ(*stack.child("mask"), o.mask);
  EXPECT_EQ(stack, over(o.base, o.top, o.mask));
}

TEST(Stacking, ADifferentBlendIsADifferentRecipeAndSoADifferentMaterial) {
  const Operands o;
  EXPECT_FALSE(over(o.base, o.top, o.mask) ==
               over(o.base, o.top, o.mask, Blend::Add));
  EXPECT_NE(overRecipe(Blend::Mix), overRecipe(Blend::Multiply));
  EXPECT_EQ(name(Blend::Multiply), "multiply");
}

TEST(Stacking, UnderWalksOneStepDownSoRepeatingItReachesTheBottom) {
  const Operands o;
  EXPECT_EQ(stackDepth(o.base), 0);
  EXPECT_EQ(*under(o.base), o.base);
  const Material stack = over(o.base, o.top, o.mask);
  EXPECT_EQ(stackDepth(stack), 1);
  EXPECT_EQ(*under(stack), o.base);
  const Material deeper = over(stack, o.top, o.mask, Blend::Add);
  EXPECT_EQ(stackDepth(deeper), 2);
  EXPECT_EQ(*under(deeper), stack);
  EXPECT_EQ(*under(*under(deeper)), o.base);
}

// ---- the bank ---------------------------------------------------------------

TEST(Bank, FoldsSeedsIntoBucketsAndKeysOnTheRecipeAndParams) {
  Bank bank(24);
  SeededParams p{0.0f, {0, 0, 0, 1}};
  const std::shared_ptr<const Recipe> recipe = seededRecipe();
  const Material& first = bank.get(recipe, p, 5);
  // The bucket IS the seed the recipe reads, and pieces in one bucket are
  // one instance.
  EXPECT_FLOAT_EQ(first.get<float>("seed"), 5.0f);
  EXPECT_EQ(&bank.get(recipe, p, 5 + 24), &first);
  EXPECT_NE(&bank.get(recipe, p, 6), &first);
  for (uint32_t seed = 0; seed < 1000; ++seed) (void)bank.get(recipe, p, seed);
  EXPECT_EQ(bank.size(), 24u);
  // A seed the caller left in the params does not reach the key.
  p.seed = 99;
  EXPECT_EQ(&bank.get(recipe, p, 5), &first);
  // The params' bytes are the rest of the key, so another tone is another
  // species and another recipe another row.
  p.uColor = {1, 0, 0, 1};
  EXPECT_NE(&bank.get(recipe, p, 5), &first);
  EXPECT_EQ(bank.size(), 25u);
  (void)bank.get(seededRecipe("other"), p, 5);
  EXPECT_EQ(bank.size(), 26u);
  bank.clear();
  EXPECT_EQ(bank.size(), 0u);
}

TEST(Bank, TheMakerRunsOncePerBucketAndItsAnswerIsWhatIsBanked) {
  Bank bank(4);
  const std::shared_ptr<const Recipe> recipe = twoRecipe();
  int made = 0;
  for (uint32_t seed = 0; seed < 40; ++seed)
    (void)bank.get(recipe, TwoParams{}, seed, [&](uint32_t bucket) {
      ++made;
      Material m(recipe);
      m.set("uScale", (float)bucket * 7);
      return m;
    });
  EXPECT_EQ(made, 4);
  EXPECT_EQ(bank.size(), 4u);
  EXPECT_FLOAT_EQ(bank.get(recipe, TwoParams{}, 9,
                           [&](uint32_t) { return Material(recipe); })
                      .get<float>("uScale"),
                  7.0f);
}
