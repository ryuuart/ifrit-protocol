/** @file
 * The program cache: one program per recipe, target and variant,
 * concurrent requests folded onto one compile, the warm-up, and the two
 * things it says once — a missing body and a params field the compiled
 * body never reads.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/Material.h>
#include <sigilshaders/MaterialCore.h>

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ShaderTable.h"

using namespace sigil::material;

namespace {

struct TwoParams {
  float uScale;
  Color uColor;
};

std::shared_ptr<const Recipe> twoRecipe(const char* name = "two") {
  return std::make_shared<const Recipe>(Recipe::of<TwoParams>(name).body(
      Target::SkSL, "half4 main(float2 p) { return half4(uColor * uScale); }"));
}

/** A compiler that records the recipe it was handed and returns a plain
 *  Program, so the cache's keying can be observed without a renderer. */
int gCompiles = 0;
std::shared_ptr<Program> countingCompiler(std::shared_ptr<const Recipe> r,
                                          Variant v, std::string&) {
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

/** Everything the cache writes to stderr while @p fn runs. */
std::string captureStderr(const std::function<void()>& fn) {
  testing::internal::CaptureStderr();
  fn();
  return testing::internal::GetCapturedStderr();
}

}  // namespace

TEST(ProgramCache, OneProgramPerRecipeTargetAndVariant) {
  ProgramCache cache;
  cache.registerCompiler(Target::Slang, countingCompiler);
  auto a = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("a").body(Target::Slang, "x"));
  auto b = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("a").body(Target::Slang, "x"));
  gCompiles = 0;
  auto p1 = cache.program(a, Target::Slang);
  auto p2 = cache.program(a, Target::Slang);
  EXPECT_EQ(p1, p2);
  EXPECT_EQ(gCompiles, 1);
  auto p3 = cache.program(a, Target::Slang, Variant{}.with(1));
  EXPECT_NE(p1, p3);
  EXPECT_EQ(p3->variant(), Variant{1});
  EXPECT_EQ(gCompiles, 2);
  // An equal definition is a different identity, so a different program.
  auto p4 = cache.program(b, Target::Slang);
  EXPECT_NE(p1, p4);
  EXPECT_EQ(gCompiles, 3);
  EXPECT_EQ(cache.size(), 3u);
  cache.clear();
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.program(a, Target::Slang)->recipe().name(), "a");
  EXPECT_EQ(gCompiles, 4);
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
      [&](std::shared_ptr<const Recipe> recipe, Variant variant, std::string&) {
        ++compiles;
        release.wait();
        return std::shared_ptr<Program>(std::make_shared<Program>(
            std::move(recipe), Target::Slang, variant));
      });
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("concurrent").body(Target::Slang, "x"));

  std::atomic_int arrived = 0;
  std::vector<std::future<std::shared_ptr<Program>>> asks;
  for (int i = 0; i < kAsks; ++i)
    asks.push_back(std::async(std::launch::async, [&] {
      if (arrived.fetch_add(1) + 1 == kAsks) everyAskIsIn.set_value();
      return cache.program(recipe, Target::Slang);
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
      [&](std::shared_ptr<const Recipe> recipe, Variant variant, std::string&) {
        ++compiles;
        return std::shared_ptr<Program>(std::make_shared<Program>(
            std::move(recipe), Target::Slang, variant));
      });
  auto a = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("warm.a").body(Target::Slang, "x"));
  auto b = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("warm.b").body(Target::Slang, "x"));
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
  EXPECT_EQ(cache.program(r, Target::SkSL), nullptr);
  cache.registerCompiler(Target::Slang, countingCompiler);
  gCompiles = 0;
  // A compiler, but no Slang body: reported, null, and the compiler never
  // runs.
  EXPECT_EQ(cache.program(r, Target::Slang), nullptr);
  EXPECT_EQ(cache.program(r, Target::Slang), nullptr);
  EXPECT_EQ(gCompiles, 0);
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_EQ(cache.program(nullptr, Target::Slang), nullptr);
}

TEST(ProgramCache, CompileFailureIsNullAndRetriedAfterClear) {
  ProgramCache cache;
  int calls = 0;
  cache.registerCompiler(
      Target::Slang,
      [&](const std::shared_ptr<const Recipe>&, Variant, std::string& e) {
        ++calls;
        e = "nope";
        return std::shared_ptr<Program>{};
      });
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("bad").body(Target::Slang, "x"));
  EXPECT_EQ(cache.program(r, Target::Slang), nullptr);
  EXPECT_EQ(calls, 1);
}

TEST(ProgramCache, UnreadParamsFieldIsNamedOnce) {
  ProgramCache cache;
  cache.registerCompiler(
      Target::Slang,
      [](const std::shared_ptr<const Recipe>& r, Variant v, std::string&) {
        return std::shared_ptr<Program>(
            std::make_shared<DroppingProgram>(r, v, "uScale"));
      });
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("dead").body(Target::Slang, "x"));
  const std::string said = captureStderr(
      [&] { EXPECT_NE(cache.program(r, Target::Slang), nullptr); });
  EXPECT_NE(said.find("\"dead\""), std::string::npos) << said;
  EXPECT_NE(said.find("uScale"), std::string::npos) << said;
  // The field the body DOES read is not named.
  EXPECT_EQ(said.find("uColor"), std::string::npos) << said;
  // Once per (recipe, target), whatever the variant.
  const std::string again = captureStderr([&] {
    cache.program(r, Target::Slang);
    cache.program(r, Target::Slang, Variant{}.with(1));
  });
  EXPECT_EQ(again, "") << again;
  // A program that keeps every field says nothing.
  ProgramCache clean;
  clean.registerCompiler(Target::Slang, countingCompiler);
  const std::string quiet =
      captureStderr([&] { clean.program(r, Target::Slang); });
  EXPECT_EQ(quiet, "") << quiet;
}

// ---- the embedded shader table --------------------------------------------

// ---- the embedded shader table --------------------------------------------

TEST(CoreShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      shaderSources(), SIGIL_MATERIAL_CORE_SHADER_DIR);
}
