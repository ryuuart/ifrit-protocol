/** @file
 * The SkSL backend's compile: a two-uniform recipe compiles through the
 * cache, resolves, and shades a raster byte-identically to the same SkSL
 * compiled and filled by hand; a child slot samples another material; a
 * body that redeclares a name a device backend owns is refused here,
 * where the refusal costs nothing.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilshaders/MaterialSkia.h>

#include <memory>
#include <string>

#include "ShaderTable.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::identical;
using sigil::material::test::render;

namespace {

struct TwoParams {
  float uScale;
  Color uColor;
};

constexpr const char* kBody =
    "half4 main(float2 p) { return half4(uColor * uScale); }";

}  // namespace

TEST(SkiaCompiler, TwoUniformRecipeMatchesHandCompiledSkSL) {
  skia::install();
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("two").body(Target::SkSL, kBody));
  Material m(recipe, TwoParams{0.5f, {0.8f, 0.4f, 0.2f, 1.0f}});
  EXPECT_FALSE(m.isAnimated());

  const FrameData frame;
  Material::Resolved resolved = m.resolve(Target::SkSL, frame);
  ASSERT_NE(resolved.program, nullptr);
  const auto* program = resolved.program->as<skia::SkiaProgram>();
  ASSERT_NE(program, nullptr);
  EXPECT_EQ(program->target(), Target::SkSL);
  sk_sp<SkShader> ours = skia::shader(m, frame);
  ASSERT_NE(ours, nullptr);

  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uScale;\nuniform float4 uColor;\n" + std::string(kBody)));
  ASSERT_NE(effect, nullptr) << error.c_str();
  SkRuntimeShaderBuilder hand(effect);
  hand.uniform("uScale") = 0.5f;
  hand.uniform("uColor") = SkV4{0.8f, 0.4f, 0.2f, 1.0f};
  sk_sp<SkShader> theirs = hand.makeShader();
  ASSERT_NE(theirs, nullptr);

  const SkBitmap a = render(ours), b = render(theirs);
  EXPECT_TRUE(identical(a, b));
  // And the pixels are the expected colour, not two matching blanks.
  EXPECT_NE(a.getColor(1, 1) & 0xff000000, 0u);

  // The same recipe resolves to the same program object every time.
  EXPECT_EQ(m.resolve(Target::SkSL, frame).program, resolved.program);
  Material other(recipe, TwoParams{1.0f, {1, 1, 1, 1}});
  EXPECT_EQ(other.resolve(Target::SkSL, frame).program, resolved.program);
}

TEST(SkiaCompiler, ChildSlotSamplesAnotherMaterial) {
  skia::install();
  auto inner = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("inner").body(Target::SkSL, kBody));
  auto outer = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("outer").child("uSrc").body(
          Target::SkSL,
          "half4 main(float2 p) { return uSrc.eval(p) * half4(uScale); }"));
  Material m(outer, TwoParams{1.0f, {0, 0, 0, 1}});
  m.child("uSrc", Material(inner, TwoParams{1.0f, {0.0f, 1.0f, 0.0f, 1.0f}}));
  sk_sp<SkShader> shader = skia::shader(m, FrameData{});
  ASSERT_NE(shader, nullptr);
  const SkBitmap bm = render(shader);
  EXPECT_EQ(bm.getColor(2, 2), 0xff00ff00u);
}

TEST(SkiaCompiler, ABodyThatDoesNotCompileResolvesToNoProgram) {
  skia::install();
  auto broken = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("broken").body(Target::SkSL, "half4 main("));
  Material m(broken);
  EXPECT_EQ(m.resolve(Target::SkSL, FrameData{}).program, nullptr);
  EXPECT_EQ(skia::shader(m, FrameData{}), nullptr);
}

namespace {

/** A GPU backend inlines a runtime effect's body into its fragment
 *  shader under parameters it names itself, discarding the names the
 *  body's own main declared: `pos` for the coordinates, `inColor` for the
 *  colour from the stage before, `destColor` for a blender's destination,
 *  `primitiveColor` for the draw's own. A body declaring anything else by
 *  one of those names redeclares a parameter — which is invisible to
 *  SkRuntimeEffect::MakeForShader, where the body IS the whole program
 *  and the name is free, and fatal on a device. So the compile refuses
 *  them here, on the CPU, where the refusal is a resolve that answers
 *  nothing. One body per row, and the rows that must still COMPILE are
 *  here too: a rule that refused everything would pass every refusal. */
struct ReservedName {
  const char* what;
  const char* body;
  bool refused;
};

class BodyOverAReservedName : public testing::TestWithParam<ReservedName> {};

std::string reservedNameOf(const testing::TestParamInfo<ReservedName>& info) {
  return info.param.what;
}

const ReservedName kReservedNames[] = {
    {"ALocalNamedForTheCoordinates",
     "half4 main(float2 p) { float2 pos = p * 0.5; "
     "return half4(half2(pos), 0.0, 1.0); }",
     true},
    {"ALocalNamedForTheIncomingColour",
     "half4 main(float2 p) { half4 inColor = half4(1.0); return inColor; }",
     true},
    {"ALocalNamedForTheDestination",
     "half4 main(float2 p) { half4 destColor = half4(1.0); return destColor; }",
     true},
    {"ALocalNamedForThePrimitiveColour",
     "half4 main(float2 p) { half4 primitiveColor = half4(1.0); "
     "return primitiveColor; }",
     true},
    // A helper's parameter is a declaration too, and lands in the same
    // generated scope.
    {"AHelpersParameter",
     "float2 shift(float2 pos) { return pos * 0.5; }\n"
     "half4 main(float2 p) { return half4(half2(shift(p)), 0.0, 1.0); }",
     true},
    // Main's OWN parameter by that name is the one declaration the
    // backend replaces rather than collides with…
    {"MainsOwnParameter",
     "half4 main(float2 pos) { return half4(half2(pos), 0.0, 1.0); }", false},
    // …and the word in a comment, or inside a longer identifier, is not a
    // declaration at all.
    {"TheWordInACommentAndInsideALongerName",
     "half4 main(float2 p) { /* float2 pos; */ float2 position = p; "
     "return half4(half2(position), 0.0, 1.0); }",
     false},
};

}  // namespace

TEST_P(BodyOverAReservedName, ResolvesToNoProgramWhereItRedeclaresAParameter) {
  skia::install();
  static int serial = 0;
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("reserved." + std::to_string(serial++))
          .body(Target::SkSL, GetParam().body));
  const bool refused = skia::shader(Material(recipe), FrameData{}) == nullptr;
  EXPECT_EQ(refused, GetParam().refused);
}

INSTANTIATE_TEST_SUITE_P(TheNamesAGpuBackendTakes, BodyOverAReservedName,
                         testing::ValuesIn(kReservedNames), reservedNameOf);

// ---- the embedded shader table --------------------------------------------

TEST(SkiaShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      sigil::material::skia::shaderSources(), SIGIL_MATERIAL_SKIA_SHADER_DIR);
}
