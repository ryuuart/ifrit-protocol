/** @file
 * The program cache: one program per recipe, target and variant,
 * concurrent requests folded onto one compile, the warm-up, and the two
 * things it says once — a missing body and a parameter field the compiled
 * body never reads.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Target.h>
#include <sigilshaders/MaterialCore.h>

#include <algorithm>
#include <atomic>
#include <barrier>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "ShaderTable.h"
#include "support/Recipes.h"

using namespace sigil::material;

namespace {

/** The program @p cache answers, or null where it refused: the shape the
 *  cases below compare programs in. */
std::shared_ptr<Program> pointerOf(ProgramCache& cache,
                                   std::shared_ptr<const Recipe> recipe,
                                   Target target, Variant variant = {}) {
  return cache.program(std::move(recipe), target, variant).value_or(nullptr);
}

/** A compiler that records the recipe it was handed and returns a plain
 *  Program, so the cache's keying can be observed without a renderer. */
int gCompiles = 0;
std::shared_ptr<Program> countingCompiler(std::shared_ptr<const Recipe> r,
                                          Variant v) {
  ++gCompiles;
  return std::make_shared<Program>(std::move(r), Target::Slang, v);
}

/** A program that has lost one uniform, standing in for a shader compiler
 *  that discards what its body never reads. */
class DroppingProgram : public Program {
 public:
  DroppingProgram(std::shared_ptr<const Recipe> r, Variant v, std::string drop)
      : Program(std::move(r), Target::Slang, v), m_drop(std::move(drop)) {}
  bool keeps(std::string_view name) const override { return name != m_drop; }

 private:
  std::string m_drop;
};

class ThrowingReflectionProgram : public Program {
 public:
  ThrowingReflectionProgram(std::shared_ptr<const Recipe> recipe,
                            Variant variant, std::function<void()> before = {})
      : Program(std::move(recipe), Target::Slang, variant),
        m_before(std::move(before)) {}
  bool keeps(std::string_view) const override {
    if (m_before) m_before();
    throw std::runtime_error("uniform reflection failed");
  }

 private:
  std::function<void()> m_before;
};

}  // namespace

TEST(ProgramCache, OneProgramPerRecipeTargetAndVariant) {
  ProgramCache cache;
  cache.registerCompiler(Target::Slang, countingCompiler);
  auto a = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("a").body(Target::Slang, "x"));
  auto b = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("a").body(Target::Slang, "x"));
  gCompiles = 0;
  auto p1 = pointerOf(cache, a, Target::Slang);
  auto p2 = pointerOf(cache, a, Target::Slang);
  EXPECT_EQ(p1, p2);
  EXPECT_EQ(gCompiles, 1);
  auto p3 = pointerOf(cache, a, Target::Slang, Variant{}.with(1));
  EXPECT_NE(p1, p3);
  EXPECT_EQ(p3->variant(), Variant{1});
  EXPECT_EQ(gCompiles, 2);
  // An equal definition is a different identity, so a different program.
  auto p4 = pointerOf(cache, b, Target::Slang);
  EXPECT_NE(p1, p4);
  EXPECT_EQ(gCompiles, 3);
  EXPECT_EQ(cache.size(), 3u);
  cache.clear();
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(pointerOf(cache, a, Target::Slang)->recipe().name(), "a");
  EXPECT_EQ(gCompiles, 4);
}

TEST(ProgramCache, ListsTheProgramsItHoldsForATarget) {
  ProgramCache cache;
  cache.registerCompiler(Target::Slang, countingCompiler);
  EXPECT_TRUE(cache.programs(Target::Slang).empty());
  auto a = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("a").body(Target::Slang, "x"));
  auto b = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("b").body(Target::Slang, "y"));
  auto first = pointerOf(cache, a, Target::Slang);
  auto second = pointerOf(cache, b, Target::Slang);
  auto listed = cache.programs(Target::Slang);
  ASSERT_EQ(listed.size(), 2u);
  EXPECT_TRUE(std::ranges::find(listed, first) != listed.end());
  EXPECT_TRUE(std::ranges::find(listed, second) != listed.end());
  EXPECT_TRUE(cache.programs(Target::SkSL).empty());
  cache.clear();
  EXPECT_TRUE(cache.programs(Target::Slang).empty());
}

TEST(ProgramCache, ConcurrentRequestsShareOneInFlightCompile) {
  constexpr int kAsks = 8;
  ProgramCache cache;
  std::atomic_int compiles = 0;

  // THE COMPILE IS HELD OPEN until every ask has reached the cache, so
  // what is being asked is whether the cache folds requests that arrive
  // while one is in flight — with no wall clock deciding what "while"
  // means.
  std::promise<void> everyAskIsIn;
  const std::shared_future<void> release = everyAskIsIn.get_future().share();
  cache.registerCompiler(
      Target::Slang,
      [&](std::shared_ptr<const Recipe> recipe, Variant variant) {
        ++compiles;
        release.wait();
        return std::shared_ptr<Program>(std::make_shared<Program>(
            std::move(recipe), Target::Slang, variant));
      });
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("concurrent").body(Target::Slang, "x"));

  std::atomic_int arrived = 0;
  std::vector<std::future<std::shared_ptr<Program>>> asks;
  for (int i = 0; i < kAsks; ++i)
    asks.push_back(std::async(std::launch::async, [&] {
      if (arrived.fetch_add(1) + 1 == kAsks) everyAskIsIn.set_value();
      return pointerOf(cache, recipe, Target::Slang);
    }));
  const std::shared_ptr<Program> first = asks.front().get();
  ASSERT_NE(first, nullptr);
  for (size_t i = 1; i < asks.size(); ++i) EXPECT_EQ(asks[i].get(), first);
  EXPECT_EQ(compiles.load(), 1);
}

TEST(ProgramCache, WarmupFoldsDuplicateKeysAndPopulatesTheCache) {
  ProgramCache cache;
  std::atomic_int compiles = 0;
  cache.registerCompiler(
      Target::Slang,
      [&](std::shared_ptr<const Recipe> recipe, Variant variant) {
        ++compiles;
        return std::shared_ptr<Program>(std::make_shared<Program>(
            std::move(recipe), Target::Slang, variant));
      });
  auto a = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("warm.a").body(Target::Slang, "x"));
  auto b = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("warm.b").body(Target::Slang, "x"));
  const WarmupRequest requests[] = {
      {a, Target::Slang, {}},
      {b, Target::Slang, {}},
      {a, Target::Slang, {}},
  };
  const WarmupResult result = cache.warmup(requests);
  EXPECT_EQ(result.requested, 3u);
  EXPECT_EQ(result.unique, 2u);
  EXPECT_EQ(result.ready, 2u);
  EXPECT_EQ(compiles.load(), 2);
  EXPECT_EQ(cache.size(), 2u);
}

TEST(ProgramCache, MissingBodyAndMissingCompilerReturnNull) {
  ProgramCache cache;
  auto r = twoRecipe();
  // No compiler for SkSL in this fresh cache.
  EXPECT_EQ(pointerOf(cache, r, Target::SkSL), nullptr);
  cache.registerCompiler(Target::Slang, countingCompiler);
  gCompiles = 0;
  // A compiler, but no Slang body: reported, null, and the compiler never
  // runs.
  EXPECT_EQ(pointerOf(cache, r, Target::Slang), nullptr);
  EXPECT_EQ(pointerOf(cache, r, Target::Slang), nullptr);
  EXPECT_EQ(gCompiles, 0);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(pointerOf(cache, nullptr, Target::Slang), nullptr);
}

TEST(ProgramCache, CompileFailureIsNullAndRetriedAfterClear) {
  ProgramCache cache;
  int calls = 0;
  cache.registerCompiler(
      Target::Slang,
      [&](const std::shared_ptr<const Recipe>&,
          Variant) -> std::expected<std::shared_ptr<Program>, sigil::core::Error> {
        ++calls;
        return std::unexpected(sigil::core::Error{"nope"});
      });
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("bad").body(Target::Slang, "x"));
  EXPECT_EQ(pointerOf(cache, r, Target::Slang), nullptr);
  EXPECT_EQ(calls, 1);
  cache.clear();
  const auto refused = cache.program(r, Target::Slang);
  ASSERT_FALSE(refused);
  EXPECT_EQ(refused.error().message, "nope");
}

TEST(ProgramCache, ReflectionExceptionLeavesTheKeyRetryable) {
  ProgramCache cache;
  int calls = 0;
  cache.registerCompiler(
      Target::Slang,
      [&](std::shared_ptr<const Recipe> recipe, Variant variant) -> std::shared_ptr<Program> {
        if (++calls == 1)
          return std::make_shared<ThrowingReflectionProgram>(std::move(recipe),
                                                             variant);
        return std::make_shared<Program>(std::move(recipe), Target::Slang,
                                         variant);
      });
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("reflection-retry").body(Target::Slang, "x"));
  EXPECT_THROW((void)cache.program(recipe, Target::Slang), std::runtime_error);
  EXPECT_EQ(cache.size(), 0u);
  const auto repaired = pointerOf(cache, recipe, Target::Slang);
  ASSERT_TRUE(repaired);
  EXPECT_EQ(pointerOf(cache, recipe, Target::Slang), repaired);
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(cache.size(), 1u);
}

TEST(ProgramCache, ReflectionFailureSettlesOverlappingRequests) {
  constexpr size_t kRequests = 4;
  ProgramCache cache;
  std::atomic_size_t calls = 0;
  std::promise<void> reflectionStarted;
  auto started = reflectionStarted.get_future();
  std::promise<void> releaseReflection;
  const auto release = releaseReflection.get_future().share();
  cache.registerCompiler(
      Target::Slang,
      [&](std::shared_ptr<const Recipe> recipe, Variant variant) -> std::shared_ptr<Program> {
        if (calls.fetch_add(1) == 0)
          return std::make_shared<ThrowingReflectionProgram>(
              std::move(recipe), variant, [&] {
                reflectionStarted.set_value();
                release.wait();
              });
        return std::make_shared<Program>(std::move(recipe), Target::Slang,
                                         variant);
      });
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("reflection-overlap").body(Target::Slang, "x"));
  const auto request = [&] {
    try {
      const auto compiled = pointerOf(cache, recipe, Target::Slang);
      return compiled && compiled->recipe().name() == recipe->name() ? 0 : 2;
    } catch (const std::runtime_error& error) {
      return std::string_view(error.what()) == "uniform reflection failed" ? 1
                                                                           : 2;
    } catch (...) {
      return 2;
    }
  };
  auto owner = std::async(std::launch::async, request);
  started.wait();
  std::barrier callers(static_cast<std::ptrdiff_t>(kRequests + 1));
  std::vector<std::future<int>> requests;
  for (size_t i = 0; i < kRequests; ++i)
    requests.push_back(std::async(std::launch::async, [&] {
      callers.arrive_and_wait();
      return request();
    }));
  callers.arrive_and_wait();
  releaseReflection.set_value();
  EXPECT_EQ(owner.get(), 1);
  // A caller joins the failed flight or starts the successful retry.
  // Neither path may retain an abandoned promise or lose the exception.
  for (auto& request : requests) EXPECT_LE(request.get(), 1);
  const auto compiled = pointerOf(cache, recipe, Target::Slang);
  ASSERT_TRUE(compiled);
  EXPECT_EQ(pointerOf(cache, recipe, Target::Slang), compiled);
  EXPECT_EQ(calls.load(), 2u);
  EXPECT_EQ(cache.size(), 1u);
}

TEST(ProgramCache, UnreadParametersFieldIsNamedOnce) {
  ProgramCache cache;
  cache.registerCompiler(
      Target::Slang,
      [](const std::shared_ptr<const Recipe>& r, Variant v) {
        return std::shared_ptr<Program>(
            std::make_shared<DroppingProgram>(r, v, "uScale"));
      });
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("dead").body(Target::Slang, "x"));
  const std::string said = captureStandardError(
      [&] { EXPECT_NE(pointerOf(cache, r, Target::Slang), nullptr); });
  EXPECT_NE(said.find("\"dead\""), std::string::npos) << said;
  EXPECT_NE(said.find("uScale"), std::string::npos) << said;
  // The field the body DOES read is not named.
  EXPECT_EQ(said.find("uColor"), std::string::npos) << said;
  // Once per (recipe, target), whatever the variant.
  const std::string again = captureStandardError([&] {
    pointerOf(cache, r, Target::Slang);
    pointerOf(cache, r, Target::Slang, Variant{}.with(1));
  });
  EXPECT_EQ(again, "") << again;
  // A program that keeps every field says nothing.
  ProgramCache clean;
  clean.registerCompiler(Target::Slang, countingCompiler);
  const std::string quiet =
      captureStandardError([&] { pointerOf(clean, r, Target::Slang); });
  EXPECT_EQ(quiet, "") << quiet;
}

// ---- the embedded shader table --------------------------------------------

TEST(CoreShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      shaderSources(), SIGIL_MATERIAL_CORE_SHADER_DIR);
}
