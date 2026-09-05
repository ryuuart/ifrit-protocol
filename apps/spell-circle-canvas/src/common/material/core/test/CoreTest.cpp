/** @file
 * The core value model: params reflection names, counts and declarations
 * per target; recipe identity against definition equality; the program
 * cache's keys, its once-reported missing body and the params field it
 * names once when the compiled body never reads it; material equality,
 * bindings, children and tiers; what `over()` stacks; and UniformBlock
 * revisioning. The colour leaf's own claims are material_test's.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/Material.h>
#include <sigilshaders/MaterialCore.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "ShaderTable.h"

using namespace sigil::material;

namespace {

struct TwoParams {
  float uScale;
  Color uColor;
};

struct EveryKind {
  float f;
  glm::vec2 v2;
  glm::vec4 v4;
  std::array<float, 3> arr;
  Color c;
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

TEST(Params, ReflectionNamesEveryFieldInDeclarationOrderAndFindsThemByName) {
  EXPECT_EQ(fieldCount<TwoParams>(), 2u);
  EXPECT_EQ(fieldCount<EveryKind>(), 5u);
  EXPECT_EQ((fieldName<0, TwoParams>()), "uScale");
  EXPECT_EQ((fieldName<1, TwoParams>()), "uColor");
  const Schema& s = schema<EveryKind>();
  ASSERT_EQ(s.fields.size(), 5u);
  EXPECT_EQ(s.fields[0].name, "f");
  EXPECT_EQ(s.fields[3].name, "arr");
  EXPECT_EQ(s.find("arr"), &s.fields[3]);
  EXPECT_EQ(s.find("nope"), nullptr);
}

TEST(Params, TheSchemaIsTheParamsStructsOwnLayout) {
  // A material's bytes ARE its params struct, so what a renderer writes
  // a uniform at is where the field stands in the struct — whatever the
  // compiler chose to put it. The kinds are this library's reading of
  // the C++ types beside them.
  const Schema& s = schema<EveryKind>();
  ASSERT_EQ(s.fields.size(), 5u);
  EXPECT_EQ(s.fields[0].kind, Kind::Float);
  EXPECT_EQ(s.fields[0].offset, offsetof(EveryKind, f));
  EXPECT_EQ(s.fields[1].kind, Kind::Vec2);
  EXPECT_EQ(s.fields[1].offset, offsetof(EveryKind, v2));
  EXPECT_EQ(s.fields[2].kind, Kind::Vec4);
  EXPECT_EQ(s.fields[2].offset, offsetof(EveryKind, v4));
  EXPECT_EQ(s.fields[3].kind, Kind::FloatArray);
  EXPECT_EQ(s.fields[3].floats, 3u);
  EXPECT_EQ(s.fields[3].offset, offsetof(EveryKind, arr));
  EXPECT_EQ(s.fields[4].kind, Kind::Color);
  EXPECT_EQ(s.fields[4].offset, offsetof(EveryKind, c));
  EXPECT_EQ(s.byteSize, sizeof(EveryKind));
}

TEST(Params, TheFieldWalkVisitsEveryFieldInDeclarationOrder) {
  const EveryKind p{
      1.5f, {2, 3}, {4, 5, 6, 7}, {8, 9, 10}, {0.1f, 0.2f, 0.3f, 1}};
  std::string names;
  forEachField(p, [&](std::string_view name, const auto& value) {
    names += name;
    names += ' ';
    (void)value;
  });
  EXPECT_EQ(names, "f v2 v4 arr c ");
}

TEST(Params, EachTargetSpellsTheDeclarationsItsCompilerReads) {
  const std::string sksl = declare<EveryKind>(Target::SkSL);
  EXPECT_EQ(sksl,
            "uniform float f;\n"
            "uniform float2 v2;\n"
            "uniform float4 v4;\n"
            "uniform float arr[3];\n"
            "uniform float4 c;\n");
  // Slang spells these vector types the same way SkSL does.
  EXPECT_EQ(declare<EveryKind>(Target::Slang), sksl);
}

TEST(Recipe, IdentityIsTheObjectAndEqualityIsTheDefinition) {
  auto a = twoRecipe();
  auto b = twoRecipe();
  EXPECT_TRUE(*a == *b);
  EXPECT_FALSE(a->id() == b->id());
  EXPECT_EQ(a->id().name, "two");
  EXPECT_EQ(a->id().recipe, a.get());
  auto c = twoRecipe("other");
  EXPECT_FALSE(*a == *c);
  EXPECT_TRUE(a->has(Target::SkSL));
  EXPECT_FALSE(a->has(Target::Slang));
  EXPECT_EQ(a->targets(), std::vector<Target>{Target::SkSL});
}

TEST(Recipe, LayoutAppendsFrameInputsAndDeclarationsListChildren) {
  Recipe r = Recipe::of<TwoParams>("r");
  r.frame(FrameInput::Resolution).frame(FrameInput::Time).child("uTex");
  EXPECT_TRUE(r.reads(FrameInput::Time));
  EXPECT_FALSE(r.reads(FrameInput::WorldTransform));
  ASSERT_EQ(r.layout().fields.size(), 4u);
  // Enum order, not declaration order.
  EXPECT_EQ(r.layout().fields[2].name, "uTime");
  EXPECT_EQ(r.layout().fields[2].offset, 20u);
  EXPECT_EQ(r.layout().fields[3].name, "uResolution");
  EXPECT_EQ(r.layout().fields[3].offset, 24u);
  EXPECT_EQ(r.layout().byteSize, 32u);
  EXPECT_EQ(r.params().byteSize, 20u);
  EXPECT_EQ(r.declarations(Target::SkSL),
            "uniform float uScale;\n"
            "uniform float4 uColor;\n"
            "uniform float uTime;\n"
            "uniform float2 uResolution;\n"
            "uniform shader uTex;\n");
  EXPECT_EQ(r.source(Target::SkSL), "");
  r.body(Target::SkSL, "half4 main(float2 p) { return half4(1); }");
  EXPECT_EQ(r.source(Target::SkSL),
            r.declarations(Target::SkSL) +
                "half4 main(float2 p) { return half4(1); }");
}

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

TEST(Recipe, NoParamsIsARecipeOverSlotsAndFrameInputsAlone) {
  struct NoParams {};
  auto r = std::make_shared<const Recipe>(Recipe::of<NoParams>("bare")
                                              .frame(FrameInput::Time)
                                              .child("uSrc")
                                              .body(Target::SkSL, "x"));
  EXPECT_TRUE(r->params().fields.empty());
  EXPECT_EQ(r->params().byteSize, 0u);
  // The frame uniform still lays out, from offset zero.
  ASSERT_EQ(r->layout().fields.size(), 1u);
  EXPECT_EQ(r->layout().fields[0].name, "uTime");
  EXPECT_EQ(r->layout().fields[0].offset, 0u);
  EXPECT_EQ(r->declarations(Target::SkSL),
            "uniform float uTime;\nuniform shader uSrc;\n");
  // An instance holds no bytes of its own, and resolving still lays out
  // the frame value it declared.
  Material m(r);
  EXPECT_TRUE(m.bytes().empty());
  const Material::Resolved resolved =
      m.resolve(Target::SkSL, FrameData{.seconds = 2.0});
  ASSERT_EQ(resolved.bytes.size(), sizeof(float));
  float seconds = 0.0f;
  std::memcpy(&seconds, resolved.bytes.data(), sizeof(float));
  EXPECT_EQ(seconds, 2.0f);
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

TEST(Material, WritingToAFieldNoBodyReadsSaysSo) {
  // A dial that does nothing looks exactly like a wrong value from the
  // call site, and no compiler's reflection can say which it is: the
  // declarations are generated from the params whether the body reads
  // them or not. The RECIPE can say, and it is asked at the write.
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("half-read")
          .body(Target::Slang, "float4 main() { return uColor; }"));
  Material m(r);
  const std::string said = captureStderr([&] { m.set("uScale", 2.0f); });
  EXPECT_NE(said.find("\"uScale\""), std::string::npos) << said;
  EXPECT_NE(said.find("no body"), std::string::npos) << said;
  // The field the body DOES read is silent, and so is a second write to
  // the one it does not.
  const std::string quiet = captureStderr([&] {
    m.set("uColor", Color{1, 0, 0, 1});
    m.set("uScale", 3.0f);
  });
  EXPECT_EQ(quiet, "") << quiet;
  // …and the value is still written: the report is about the picture,
  // not about the bytes.
  EXPECT_EQ(m.get<float>("uScale"), 3.0f);
  // A NAME INSIDE A LONGER ONE is a different name.
  auto sub = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("substring")
          .body(Target::Slang, "float4 main() { return uScaleFactor; }"));
  Material s(sub);
  EXPECT_NE(captureStderr([&] { s.set("uScale", 1.0f); }).find("uScale"),
            std::string::npos);
  // A recipe with no body at all has nothing to say.
  auto none = std::make_shared<const Recipe>(Recipe::of<TwoParams>("bodiless"));
  Material n(none);
  EXPECT_EQ(captureStderr([&] { n.set("uScale", 1.0f); }), "");
}

TEST(Material, MirrorsParamsAsBytesAndSetsFields) {
  auto r = twoRecipe();
  Material m(r, TwoParams{2.0f, {0.1f, 0.2f, 0.3f, 1.0f}});
  EXPECT_EQ(m.bytes().size(), sizeof(TwoParams));
  EXPECT_EQ(m.get<float>("uScale"), 2.0f);
  EXPECT_EQ(m.get<Color>("uColor"), (Color{0.1f, 0.2f, 0.3f, 1.0f}));
  m.set("uScale", 3.0f);
  EXPECT_EQ(m.get<float>("uScale"), 3.0f);
  // A float4 fills a Color field: both are four floats.
  m.set("uColor", glm::vec4{1, 1, 0, 1});
  EXPECT_EQ(m.get<Color>("uColor"), (Color{1, 1, 0, 1}));
  // A mismatch is ignored, not written.
  m.set("uScale", glm::vec2{9, 9});
  EXPECT_EQ(m.get<float>("uScale"), 3.0f);
  m.set("missing", 1.0f);
  Material zero(r);
  EXPECT_EQ(zero.get<float>("uScale"), 0.0f);
  zero.set(TwoParams{3.0f, {1, 1, 0, 1}});
  EXPECT_TRUE(zero == m);
}

TEST(Material, EqualityIsByValueWithBindingsByIdentity) {
  auto r = twoRecipe();
  const TwoParams p{1.0f, {0, 0, 0, 1}};
  Material a(r, p), b(r, p);
  EXPECT_TRUE(a == b);
  b.set("uScale", 2.0f);
  EXPECT_FALSE(a == b);
  b.set("uScale", 1.0f);
  EXPECT_TRUE(a == b);
  // A different recipe object with the same definition is a different
  // material.
  Material c(twoRecipe(), p);
  EXPECT_FALSE(a == c);
  choreograph::Output<float> out{0.5f};
  a.bind("uScale", &out);
  EXPECT_FALSE(a == b);
  b.bind("uScale", &out);
  EXPECT_TRUE(a == b);
  choreograph::Output<float> other{0.5f};
  b.bind("uScale", &other);
  EXPECT_FALSE(a == b);
  b.unbind("uScale");
  a.unbind("uScale");
  EXPECT_TRUE(a == b);
  a.amount(0.5f);
  EXPECT_FALSE(a == b);
  b.amount(0.5f);
  a.quantizeTime(12);
  EXPECT_FALSE(a == b);
  b.quantizeTime(12);
  a.worldSpace();
  EXPECT_FALSE(a == b);
  b.worldSpace();
  EXPECT_TRUE(a == b);
}

TEST(Material, TiersFollowBindingsFrameInputsAndChildren) {
  auto plain = twoRecipe();
  Material still(plain, TwoParams{});
  EXPECT_FALSE(still.isAnimated());
  EXPECT_FALSE(still.geometryDependent());

  choreograph::Output<float> out{0.0f};
  Material bound = still;
  bound.bind("uScale", &out);
  EXPECT_TRUE(bound.isAnimated());
  EXPECT_FALSE(bound.geometryDependent());

  auto timed = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("timed").frame(FrameInput::Time));
  EXPECT_TRUE(Material(timed).isAnimated());
  auto sized = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("sized").frame(FrameInput::Resolution));
  EXPECT_FALSE(Material(sized).isAnimated());
  EXPECT_TRUE(Material(sized).geometryDependent());

  auto parentRecipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("parent").child("uA").child("uB"));
  Material parent(parentRecipe);
  EXPECT_FALSE(parent.isAnimated());
  parent.child("uB", Material(sized));
  EXPECT_TRUE(parent.geometryDependent());
  EXPECT_FALSE(parent.isAnimated());
  parent.child("uA", bound);
  EXPECT_TRUE(parent.isAnimated());
  ASSERT_EQ(parent.children().size(), 2u);
  // Slots sit in recipe order however they were filled.
  EXPECT_EQ(parent.children()[0].first, "uA");
  EXPECT_NE(parent.child("uA"), nullptr);
  EXPECT_EQ(parent.child("uZ"), nullptr);
  parent.child("uZ", still);  // undeclared: ignored
  EXPECT_EQ(parent.children().size(), 2u);

  Material same(parentRecipe);
  same.child("uA", bound).child("uB", Material(sized));
  EXPECT_TRUE(parent == same);
  same.child("uB", Material(sized).amount(0.2f));
  EXPECT_FALSE(parent == same);
}

TEST(Material, ResolveSamplesBindingsInjectsFrameAndMemoises) {
  ProgramCache::shared().registerCompiler(Target::Slang, countingCompiler);
  struct P {
    float uScale;
    std::array<float, 4> uTable;
  };
  auto r = std::make_shared<const Recipe>(Recipe::of<P>("live")
                                              .frame(FrameInput::Time)
                                              .frame(FrameInput::Resolution)
                                              .body(Target::Slang, "x"));
  Material m(r, P{1.0f, {1, 2, 3, 4}});
  choreograph::Output<float> out{5.0f};
  auto block = std::make_shared<UniformBlock>(4);
  m.bind("uScale", &out).bind("uTable", block);
  m.bind("uTable", std::make_shared<UniformBlock>(3));  // wrong size: ignored
  EXPECT_TRUE(m.isAnimated());

  FrameData frame;
  frame.seconds = 1.25;
  frame.resolution = {64, 32};
  gCompiles = 0;
  Material::Resolved a = m.resolve(Target::Slang, frame);
  ASSERT_NE(a.program, nullptr);
  ASSERT_EQ(a.bytes.size(), r->layout().byteSize);
  const auto at = [&](const Material::Resolved& res, const char* name) {
    float v;
    std::memcpy(&v, res.bytes.data() + r->layout().find(name)->offset,
                sizeof v);
    return v;
  };
  EXPECT_EQ(at(a, "uScale"), 5.0f);
  EXPECT_EQ(at(a, "uTable"), 0.0f);  // the block's zeros, not the params
  EXPECT_EQ(at(a, "uTime"), 1.25f);
  EXPECT_EQ(at(a, "uResolution"), 64.0f);

  // Nothing changed: the same bytes come back, from the memo.
  Material::Resolved b = m.resolve(Target::Slang, frame);
  EXPECT_EQ(a.bytes.data(), b.bytes.data());
  EXPECT_EQ(a.program, b.program);

  // The block wrote without committing — a resolve still reads the current
  // values, because the block is live.
  block->values()[0] = 7.0f;
  Material::Resolved c = m.resolve(Target::Slang, frame);
  EXPECT_EQ(at(c, "uTable"), 7.0f);
  out = 6.0f;
  EXPECT_EQ(at(m.resolve(Target::Slang, frame), "uScale"), 6.0f);

  // Quantised time snaps to the step, so within one step the memo holds.
  m.quantizeTime(4.0f);
  frame.seconds = 1.30;
  const std::byte* first = m.resolve(Target::Slang, frame).bytes.data();
  frame.seconds = 1.45;
  EXPECT_EQ(m.resolve(Target::Slang, frame).bytes.data(), first);
  EXPECT_EQ(at(m.resolve(Target::Slang, frame), "uTime"), 1.25f);
  frame.seconds = 1.5;
  EXPECT_EQ(at(m.resolve(Target::Slang, frame), "uTime"), 1.5f);

  // No SkSL body: null program, bytes still resolved.
  Material::Resolved none = m.resolve(Target::SkSL, frame);
  EXPECT_EQ(none.program, nullptr);
  EXPECT_EQ(none.bytes.size(), r->layout().byteSize);
}

TEST(UniformBlock, RevisionAdvancesOnCommitOnly) {
  UniformBlock block(3);
  EXPECT_EQ(block.size(), 3u);
  EXPECT_EQ(block.revision(), 0u);
  block.values()[1] = 2.0f;
  EXPECT_EQ(block.revision(), 0u);
  block.commit();
  EXPECT_EQ(block.revision(), 1u);
  block.commit();
  EXPECT_EQ(block.revision(), 2u);
  const UniformBlock& ro = block;
  EXPECT_EQ(ro.values()[1], 2.0f);
  EXPECT_EQ(ro.values()[0], 0.0f);
}

namespace {

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
  EXPECT_FLOAT_EQ(bank
                      .get(recipe, TwoParams{}, 9,
                           [&](uint32_t) { return Material(recipe); })
                      .get<float>("uScale"),
                  7.0f);
}

// ---- the embedded shader table --------------------------------------------

TEST(CoreShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      shaderSources(), SIGIL_MATERIAL_CORE_SHADER_DIR);
}
