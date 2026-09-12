/** @file The Skia backend works before any other material has been drawn. */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkString.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "support/Shade.h"

namespace sigil::material {
namespace {

struct EmptyParameters {};

Material green(const char* name) {
  return Material(
      std::make_shared<const Recipe>(Recipe::of<EmptyParameters>(name).body(
          Target::SkSL, "half4 main(float2 p) { return half4(0, 1, 0, 1); }")));
}

bool drawsGreen(const sk_sp<SkShader>& shader) {
  return shader && test::render(shader).getColor(1, 1) == SK_ColorGREEN;
}

std::shared_ptr<Program> compileForSkia(std::shared_ptr<const Recipe> recipe,
                                        Variant variant, std::string& error) {
  auto [effect, message] =
      SkRuntimeEffect::MakeForShader(SkString(recipe->source(Target::SkSL)));
  error = message.c_str();
  if (!effect) return nullptr;
  return std::make_shared<skia::SkiaProgram>(std::move(recipe), variant,
                                             std::move(effect));
}

enum class Entry { Builder, Shader, Fill, Paint, Effect, Pass, Warmup };

bool firstUse(Entry entry) {
  // The child re-executes the binary. A prior test's compiler must not
  // make an entry that omitted initialization appear to work.
  if (ProgramCache::shared().hasCompiler(Target::SkSL)) return false;
  const Material material = green("first-use");
  switch (entry) {
    case Entry::Builder: {
      const auto built = skia::builder(material, {});
      return built && drawsGreen(built->makeShader());
    }
    case Entry::Shader:
      return drawsGreen(skia::shader(material, {}));
    case Entry::Fill: {
      SkBitmap pixels;
      pixels.allocN32Pixels(4, 4);
      pixels.eraseColor(SK_ColorTRANSPARENT);
      SkCanvas canvas(pixels);
      skia::fill(canvas, SkPath::Rect(SkRect::MakeWH(4, 4)), material);
      return pixels.getColor(1, 1) == SK_ColorGREEN;
    }
    case Entry::Paint:
      return drawsGreen(skia::Paint::recipe(material).asShader());
    case Entry::Effect: {
      const Material effect(std::make_shared<const Recipe>(
          Recipe::of<EmptyParameters>("first-effect")
              .child("content")
              .body(Target::SkSL,
                    "half4 main(float2 p) { return content.eval(p); }")));
      return skia::Effect::recipe(effect).resolvedImageFilter(nullptr) !=
             nullptr;
    }
    case Entry::Pass: {
      const Material pass(std::make_shared<const Recipe>(
          Recipe::of<EmptyParameters>("first-pass")
              .body(Target::SkSL,
                    "half4 main(float2 p) { return uContent.eval(p); }")));
      const skia::Paint paint = skia::Paint::recipe(pass);
      skia::PassInputs input;
      input.content = SkShaders::Color(SK_ColorGREEN);
      input.units = 1;
      return drawsGreen(paint.resolvePass(input, {}));
    }
    case Entry::Warmup: {
      const std::array materials{material, material};
      const WarmupResult result = skia::warmup(materials);
      return result.requested == 2 && result.unique == 1 && result.ready == 1 &&
             drawsGreen(skia::shader(material, {}));
    }
  }
  return false;
}

class SkiaFirstUse : public testing::TestWithParam<Entry> {};

TEST_P(SkiaFirstUse, DrawsWithoutCompilerSetup) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_EXIT(std::_Exit(firstUse(GetParam()) ? 0 : 1),
              testing::ExitedWithCode(0), "");
}

std::string entryName(const testing::TestParamInfo<Entry>& info) {
  constexpr const char* names[] = {"Builder", "Shader", "Fill",  "Paint",
                                   "Effect",  "Pass",   "Warmup"};
  return names[static_cast<size_t>(info.param)];
}

INSTANTIATE_TEST_SUITE_P(EveryLoweringEntry, SkiaFirstUse,
                         testing::Values(Entry::Builder, Entry::Shader,
                                         Entry::Fill, Entry::Paint,
                                         Entry::Effect, Entry::Pass,
                                         Entry::Warmup),
                         entryName);

bool customCompilerWins(bool useBuiltinFirst) {
  if (ProgramCache::shared().hasCompiler(Target::SkSL)) return false;
  if (useBuiltinFirst && !drawsGreen(skia::shader(green("builtin"), {})))
    return false;
  int compiled = 0;
  registerCompiler(Target::SkSL,
                   [&](std::shared_ptr<const Recipe> recipe, Variant variant,
                       std::string& error) -> std::shared_ptr<Program> {
                     ++compiled;
                     return compileForSkia(std::move(recipe), variant, error);
                   });
  const std::array materials{green("custom-warmup")};
  const WarmupResult warm = skia::warmup(materials);
  return warm.ready == 1 && drawsGreen(skia::shader(materials.front(), {})) &&
         drawsGreen(skia::shader(green("custom-draw"), {})) && compiled == 2;
}

class SkiaCustomCompiler : public testing::TestWithParam<bool> {};

TEST_P(SkiaCustomCompiler, ExplicitRegistrationTakesPrecedence) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_EXIT(std::_Exit(customCompilerWins(GetParam()) ? 0 : 1),
              testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(RegistrationOrder, SkiaCustomCompiler, testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "AfterFirstDraw"
                                             : "BeforeFirstDraw";
                         });

bool recipeSnapshotsDistinguishCompilers() {
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<EmptyParameters>("effect.compiler-snapshot")
          .child("content")
          .body(Target::SkSL,
                "half4 main(float2 p) { return content.eval(p); }"));
  const skia::Effect builtin = skia::Effect::recipe(Material(recipe));
  if (!builtin.imageFilter()) return false;
  registerCompiler(Target::SkSL,
                   [](std::shared_ptr<const Recipe> source, Variant variant,
                      std::string& error) -> std::shared_ptr<Program> {
                     auto [effect, message] =
                         SkRuntimeEffect::MakeForShader(SkString(
                             "uniform shader content; half4 main(float2 p) { "
                             "return content.eval(p) * 0.5; }"));
                     error = message.c_str();
                     if (!effect) return nullptr;
                     return std::make_shared<skia::SkiaProgram>(
                         std::move(source), variant, std::move(effect));
                   });
  ProgramCache::shared().clear();
  const skia::Effect custom = skia::Effect::recipe(Material(recipe));
  return custom.imageFilter() && !(builtin == custom);
}

TEST(SkiaEffect, RecipeSnapshotsDistinguishCompiledPrograms) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_EXIT(std::_Exit(recipeSnapshotsDistinguishCompilers() ? 0 : 1),
              testing::ExitedWithCode(0), "");
}

bool passProgramsAreReused() {
  if (ProgramCache::shared().hasCompiler(Target::SkSL)) return false;
  std::vector<std::shared_ptr<const Recipe>> compiled;
  registerCompiler(Target::SkSL, [&](std::shared_ptr<const Recipe> recipe,
                                     Variant variant, std::string& error) {
    compiled.push_back(recipe);
    return compileForSkia(std::move(recipe), variant, error);
  });
  struct Parameters {
    float level = 1;
  };
  const auto authored = std::make_shared<const Recipe>(
      Recipe::of<Parameters>("pass-reuse")
          .body(Target::SkSL,
                "half4 main(float2 p) { return half4(0, "
                "half(uUnitRect[kUnitCount - 1].x * level), 0, 1); }"));
  const skia::Paint paint =
      skia::Paint::recipe(Material(authored, Parameters{}));
  std::array<float, 12> three{};
  std::array<float, 20> five{};
  three[8] = 1;
  five[16] = 1;
  skia::PassInputs input;
  input.content = SkShaders::Color(SK_ColorGREEN);
  input.rects = three.data();
  input.units = 3;
  if (!drawsGreen(paint.resolvePass(input, {})) || compiled.size() != 1)
    return false;
  if (!drawsGreen(paint.resolvePass(input, {})) || compiled.size() != 1)
    return false;
  input.rects = five.data();
  input.units = 5;
  if (!drawsGreen(paint.resolvePass(input, {})) || compiled.size() != 2)
    return false;
  input.rects = three.data();
  input.units = 3;
  if (!drawsGreen(paint.resolvePass(input, {})) || compiled.size() != 2)
    return false;
  return compiled[0]->parameters() == authored->parameters() &&
         compiled[1]->parameters() == authored->parameters() &&
         compiled[0] != compiled[1];
}

TEST(SkiaPass, RepeatedUnitCountsReuseProgramsAndKeepTheAuthoredParameters) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_EXIT(std::_Exit(passProgramsAreReused() ? 0 : 1),
              testing::ExitedWithCode(0), "");
}

}  // namespace
}  // namespace sigil::material
