/** @file
 * The SkSL backend's compile: a two-uniform recipe compiles through the
 * cache, resolves, and shades a raster byte-identically to the same SkSL
 * compiled and filled by hand; a slot samples another material; a
 * body that redeclares a name a device backend owns is refused here,
 * where the refusal costs nothing.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilshaders/MaterialSkia.h>

#include <memory>
#include <string>

#include "ShaderTable.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::identical;
using sigil::material::test::render;

namespace {

struct TwoParameters {
  float uScale;
  Color uColor;
};

constexpr const char* kBody =
    "half4 main(float2 p) { return half4(uColor * uScale); }";

Material childSampler(Material child) {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("compiler.composed-child")
          .slot("uSrc")
          .body(Target::SkSL, "half4 main(float2 p) { return uSrc.eval(p); }"));
  Material sampler(recipe);
  sampler.slot("uSrc", std::move(child));
  return sampler;
}

}  // namespace

TEST(SkiaCompiler, TwoUniformRecipeMatchesHandCompiledSkSL) {
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("two").body(Target::SkSL, kBody));
  Material m(recipe, TwoParameters{0.5f, {0.8f, 0.4f, 0.2f, 1.0f}});
  EXPECT_FALSE(m.isRunning());

  const FrameData frame;
  sk_sp<SkShader> ours = skia::shader(m, frame);
  ASSERT_NE(ours, nullptr);
  Material::Resolved resolved = m.resolve(Target::SkSL, frame);
  ASSERT_NE(resolved.program, nullptr);
  const auto* program = resolved.program->as<skia::SkiaProgram>();
  ASSERT_NE(program, nullptr);
  EXPECT_EQ(program->target(), Target::SkSL);

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
  Material other(recipe, TwoParameters{1.0f, {1, 1, 1, 1}});
  EXPECT_EQ(other.resolve(Target::SkSL, frame).program, resolved.program);
}

TEST(SkiaCompiler, ChildSlotSamplesAnotherMaterial) {
  auto inner = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("inner").body(Target::SkSL, kBody));
  auto outer = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("outer").slot("uSrc").body(
          Target::SkSL,
          "half4 main(float2 p) { return uSrc.eval(p) * half4(uScale); }"));
  Material m(outer, TwoParameters{1.0f, {0, 0, 0, 1}});
  m.slot("uSrc",
         Material(inner, TwoParameters{1.0f, {0.0f, 1.0f, 0.0f, 1.0f}}));
  sk_sp<SkShader> shader = skia::shader(m, FrameData{});
  ASSERT_NE(shader, nullptr);
  const SkBitmap bm = render(shader);
  EXPECT_EQ(bm.getColor(2, 2), 0xff00ff00u);
}

TEST(SkiaCompiler, AChildSlotSamplesColorsAndLayers) {
  const auto red = skia::shader(childSampler(Color{1, 0, 0, 1}), {});
  ASSERT_NE(red, nullptr);
  EXPECT_EQ(render(red).getColor(2, 2), SK_ColorRED);

  Material layered(Color{1, 0, 0, 1});
  layered.layer(Color{0, 0, 1, 1}, {.opacity = 0.5f});
  const Paint held = skia::paint(childSampler(std::move(layered)));
  const auto first = skia::shader(held, FrameData{});
  ASSERT_NE(first, nullptr);
  const SkColor mixed = render(first).getColor(2, 2);
  EXPECT_NEAR(SkColorGetR(mixed), 128, 1);
  EXPECT_EQ(SkColorGetG(mixed), 0);
  EXPECT_NEAR(SkColorGetB(mixed), 128, 1);
  EXPECT_EQ(SkColorGetA(mixed), 255);
  EXPECT_FALSE(held.isRunning());
  EXPECT_EQ(skia::shader(held, FrameData{.seconds = 2}), first);
}

TEST(SkiaCompiler, AComposedChildLayerReadsTheCurrentFrameThroughAHeldPaint) {
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("compiler.live-child-layer")
          .frame(FrameInput::Time)
          .body(Target::SkSL,
                "half4 main(float2 p) { return uTime < 1.0 ? "
                "half4(1, 0, 0, 1) : half4(0, 0, 1, 1); }"));
  Material child(Color{0, 1, 0, 1});
  child.layer(Material(recipe));
  const Paint held = skia::paint(childSampler(std::move(child)));
  ASSERT_TRUE(held.isRunning());
  const auto first = skia::shader(held, FrameData{.seconds = 0});
  const auto second = skia::shader(held, FrameData{.seconds = 2});
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(render(first).getColor(2, 2), SK_ColorRED);
  EXPECT_EQ(render(second).getColor(2, 2), SK_ColorBLUE);
  EXPECT_EQ(render(skia::shader(held, FrameData{.seconds = 0})).getColor(2, 2),
            SK_ColorRED);
}

TEST(SkiaCompiler, AComposedChildPaintUsesTheCurrentBoxThroughAHeldPaint) {
  const Paint held = skia::paint(childSampler(
      linearGradient({0, 0}, {1, 0}, {Color{1, 0, 0, 1}, Color{0, 0, 1, 1}})));
  ASSERT_TRUE(held.geometryDependent());
  const auto narrow = skia::shader(held, FrameData{.resolution = {4, 4}});
  const auto wide = skia::shader(held, FrameData{.resolution = {8, 4}});
  ASSERT_NE(narrow, nullptr);
  ASSERT_NE(wide, nullptr);
  const SkColor atNarrow = render(narrow, 8, 4).getColor(2, 2);
  const SkColor atWide = render(wide, 8, 4).getColor(2, 2);
  EXPECT_GT(SkColorGetB(atNarrow), SkColorGetR(atNarrow));
  EXPECT_GT(SkColorGetR(atWide), SkColorGetB(atWide));
  EXPECT_LT(SkColorGetB(atWide), SkColorGetB(atNarrow));
}

TEST(SkiaCompiler, AComposedChildExceedingTheImageSamplerLimitIsRefused) {
  Material layered(Color{0, 0, 0, 0});
  for (int i = 0; i < 17; ++i)
    layered.layer(image(Texture(
        test::solid(SkColorSetARGB(255, i * 7, 255 - i * 7, i * 11), 8, 8))));
  const Material sampler = childSampler(std::move(layered));
  EXPECT_EQ(skia::samplerCount(sampler), 17);
  EXPECT_EQ(skia::shader(sampler, {}), nullptr);
}

TEST(SkiaCompiler, ABodyThatDoesNotCompileResolvesToNoProgram) {
  auto broken = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("broken").body(Target::SkSL, "half4 main("));
  Material m(broken);
  EXPECT_EQ(skia::shader(m, FrameData{}), nullptr);
  EXPECT_EQ(m.resolve(Target::SkSL, FrameData{}).program, nullptr);
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
    {"AReservedDeclarationAfterABlockComment",
     "/* the shader's coordinates */\n"
     "half4 main(float2 p) { float2 pos = p; "
     "return half4(half2(pos), 0.0, 1.0); }",
     true},
    {"AReservedDeclarationBetweenBlockComments",
     "half4 main(float2 p) { /* before */ half4 inColor = half4(1); "
     "/* after */ return inColor; }",
     true},
};

}  // namespace

TEST_P(BodyOverAReservedName, ResolvesToNoProgramWhereItRedeclaresAParameter) {
  static int serial = 0;
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("reserved." + std::to_string(serial++))
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

TEST(SkiaCompiler,
     ALayerSlotNothingFilledRefusesRatherThanShadingAnEmptyChild) {
  // A slot an executor fills from the layer has no source at all when
  // the material is painted as an ordinary fill: there is no layer and
  // no caller leaving the name. Refused by name here, because a child
  // nothing binds shades nothing and leaves no trace of which slot it
  // was.
  struct OneRadius {
    float uRadius;
  };
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<OneRadius>("compiler.layerslot")
          .slot("content")
          .slot("bloom", LayerFilter::Blurred, "uRadius")
          .body(Target::SkSL,
                "half4 main(float2 p) { return content.eval(p) + "
                "bloom.eval(p); }"));
  Material material(recipe, OneRadius{4});
  material.slot("content", Texture(test::solid(SK_ColorRED, 8, 8)));
  EXPECT_EQ(skia::shader(material, {}), nullptr);
  // Filled by hand, it shades: the declaration states who fills the slot
  // by default, and an author may answer for it.
  material.slot("bloom", Texture(test::solid(SK_ColorBLUE, 8, 8)));
  EXPECT_NE(skia::shader(material, {}), nullptr);
}
