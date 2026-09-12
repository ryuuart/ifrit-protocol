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

struct TwoParameters {
  float uScale;
  Color uColor;
};

struct SeededParameters {
  float seed;
  Color uColor;
};

std::shared_ptr<const Recipe> seededRecipe(const char* name = "seeded") {
  return std::make_shared<const Recipe>(Recipe::of<SeededParameters>(name).body(
      Target::SkSL, "half4 main(float2 p) { return half4(uColor * seed); }"));
}

std::shared_ptr<const Recipe> twoRecipe(const char* name = "two") {
  return std::make_shared<const Recipe>(Recipe::of<TwoParameters>(name).body(
      Target::SkSL, "half4 main(float2 p) { return half4(uColor * uScale); }"));
}

/** A stand-in Slang compiler, so `over()` builds the COMPOSED recipe.
 *  A stack is composed only where a compiler that cannot reach a child
 *  material is installed; one built without it carries the plain
 *  three-slot recipe, which asks the composition cache nothing. */
std::shared_ptr<Program> slangStandIn(std::shared_ptr<const Recipe> recipe,
                                      Variant variant, std::string&) {
  return std::make_shared<Program>(std::move(recipe), Target::Slang, variant);
}

/** A recipe whose Slang body spells @p mark, so the text of a
 *  composition says which definitions were inlined into it. */
std::shared_ptr<const Recipe> markedRecipe(const char* name, const char* mark) {
  return std::make_shared<const Recipe>(Recipe::of<TwoParameters>(name).body(
      Target::Slang, std::string("float4 surface(float2 p) { return ") + mark +
                         "(uColor * uScale); }"));
}

Material marked(const std::shared_ptr<const Recipe>& recipe) {
  return Material(recipe, TwoParameters{1, {1, 1, 1, 1}});
}

/** THE THREE OPERANDS every stacking case below is built from. */
struct Operands {
  std::shared_ptr<const Recipe> recipe = twoRecipe();
  Material base{recipe, TwoParameters{1, {1, 0, 0, 1}}};
  Material top{recipe, TwoParameters{1, {0, 0, 1, 1}}};
  Material mask{recipe, TwoParameters{0.5f, {1, 1, 1, 1}}};
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

TEST(Stacking, TheCompositionIsHeldUnderItsOperandsAndNotUnderTheirAddresses) {
  registerCompiler(Target::Slang, slangStandIn);
  const std::shared_ptr<const Recipe> base = markedRecipe("stack.key.a", "aye");
  const std::shared_ptr<const Recipe> mask = markedRecipe("stack.key.m", "em");
  std::shared_ptr<const Recipe> b = markedRecipe("stack.key.b", "bee");

  // The stack itself is let go of, because a stack keeps its operands as
  // its children: while one stands, so does every definition under it,
  // and the question below is what happens when none does. The
  // composition is kept, and it holds nothing of its operands but their
  // inlined text.
  std::shared_ptr<const Recipe> composedWithB;
  {
    const Material withB = over(marked(base), marked(b), marked(mask));
    composedWithB = withB.recipePointer();
  }
  const std::string* bodyB = composedWithB->body(Target::Slang);
  ASSERT_NE(bodyB, nullptr);
  EXPECT_NE(bodyB->find("bee"), std::string::npos);

  // THE HAZARD: a definition freed and a second allocated where it stood
  // would inherit the first's composition, and the stack would be drawn
  // with a body its own operand never wrote. It cannot happen, because
  // the cache HOLDS the three definitions it composed: with the stack
  // gone and the caller's own reference dropped, the definition is
  // standing still, so no later recipe can be built at its address.
  const std::weak_ptr<const Recipe> watch = b;
  const Recipe* stood = b.get();
  b.reset();
  ASSERT_FALSE(watch.expired());

  const std::shared_ptr<const Recipe> c = markedRecipe("stack.key.c", "cee");
  EXPECT_NE(c.get(), stood);

  // A fresh operand is a fresh key, and what comes back is the
  // composition of THIS stack's three bodies.
  const Material withC = over(marked(base), marked(c), marked(mask));
  EXPECT_NE(withC.recipePointer(), composedWithB);
  const std::string* bodyC = withC.recipe().body(Target::Slang);
  ASSERT_NE(bodyC, nullptr);
  EXPECT_NE(bodyC->find("cee"), std::string::npos);
  EXPECT_EQ(bodyC->find("bee"), std::string::npos);
}

TEST(Stacking, OneCompositionServesEveryStackOverTheSameThreeDefinitions) {
  registerCompiler(Target::Slang, slangStandIn);
  const std::shared_ptr<const Recipe> base = markedRecipe("stack.one.a", "aye");
  const std::shared_ptr<const Recipe> top = markedRecipe("stack.one.t", "tee");
  const std::shared_ptr<const Recipe> mask = markedRecipe("stack.one.m", "em");

  // While the definitions live, two stacks over them are one definition,
  // one program and one pipeline — the whole reason the composition is
  // cached rather than written per call.
  const Material first = over(marked(base), marked(top), marked(mask));
  const Material again = over(marked(base), marked(top), marked(mask));
  EXPECT_EQ(first.recipePointer(), again.recipePointer());

  // The blend and each operand are all in the key.
  EXPECT_NE(
      over(marked(base), marked(top), marked(mask), Blend::Add).recipePointer(),
      first.recipePointer());
  const std::shared_ptr<const Recipe> other = markedRecipe("stack.one.o", "oh");
  EXPECT_NE(over(marked(base), marked(other), marked(mask)).recipePointer(),
            first.recipePointer());
  EXPECT_EQ(over(marked(base), marked(top), marked(mask)).recipePointer(),
            first.recipePointer());
}

// ---- the bank ---------------------------------------------------------------

TEST(Bank, FoldsSeedsIntoBucketsAndKeysOnTheRecipeAndParameters) {
  Bank bank(24);
  SeededParameters p{0.0f, {0, 0, 0, 1}};
  const std::shared_ptr<const Recipe> recipe = seededRecipe();
  const Material& first = bank.get(recipe, p, 5);
  // The bucket IS the seed the recipe reads, and pieces in one bucket are
  // one instance.
  EXPECT_FLOAT_EQ(first.get<float>("seed"), 5.0f);
  EXPECT_EQ(&bank.get(recipe, p, 5 + 24), &first);
  EXPECT_NE(&bank.get(recipe, p, 6), &first);
  for (uint32_t seed = 0; seed < 1000; ++seed) (void)bank.get(recipe, p, seed);
  EXPECT_EQ(bank.size(), 24u);
  // A seed the caller left in the parameters does not reach the key.
  p.seed = 99;
  EXPECT_EQ(&bank.get(recipe, p, 5), &first);
  // The parameters' bytes are the rest of the key, so another tone is another
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
    (void)bank.get(recipe, TwoParameters{}, seed, [&](uint32_t bucket) {
      ++made;
      Material m(recipe);
      m.set("uScale", (float)bucket * 7);
      return m;
    });
  EXPECT_EQ(made, 4);
  EXPECT_EQ(bank.size(), 4u);
  EXPECT_FLOAT_EQ(bank.get(recipe, TwoParameters{}, 9,
                           [&](uint32_t) { return Material(recipe); })
                      .get<float>("uScale"),
                  7.0f);
}
