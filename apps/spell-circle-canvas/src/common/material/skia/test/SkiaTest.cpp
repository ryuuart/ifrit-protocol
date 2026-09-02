/** @file
 * The SkSL backend: a two-uniform recipe compiles through the cache,
 * resolves, and shades a raster byte-identically to the same SkSL
 * compiled and filled by hand; a child slot samples another material.
 * Beside them, the colour bridge's round trip.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

using namespace sigil::material;

namespace {

struct TwoParams {
  float uScale;
  Color uColor;
};

constexpr const char* kBody =
    "half4 main(float2 p) { return half4(uColor * uScale); }";

SkBitmap render(const sk_sp<SkShader>& shader) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(4, 4));
  SkCanvas canvas(bm);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setShader(shader);
  canvas.drawPaint(paint);
  return bm;
}

bool identical(const SkBitmap& a, const SkBitmap& b) {
  return a.computeByteSize() == b.computeByteSize() &&
         std::memcmp(a.getPixels(), b.getPixels(), a.computeByteSize()) == 0;
}

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

// ---------------------------------------------------------------------------
// The colour bridge.

TEST(SkiaColor, ChannelsAndAlphaSurviveTheRoundTrip) {
  constexpr SkColor4f in{0.25f, 0.5f, 0.75f, 0.125f};
  constexpr Color mid = skia::toColor(in);
  EXPECT_FLOAT_EQ(mid.r, 0.25f);
  EXPECT_FLOAT_EQ(mid.g, 0.5f);
  EXPECT_FLOAT_EQ(mid.b, 0.75f);
  EXPECT_FLOAT_EQ(mid.a, 0.125f);
  constexpr SkColor4f back = skia::toSkColor(mid);
  EXPECT_EQ(back, in);
}

TEST(SkiaColor, AChannelAboveOneIsCarriedRatherThanClamped) {
  // Straight copy, no transfer function: a wide-gamut channel survives.
  const Color c = skia::toColor(SkColor4f{1.5f, -0.25f, 0, 1});
  EXPECT_FLOAT_EQ(c.r, 1.5f);
  EXPECT_FLOAT_EQ(c.g, -0.25f);
}

TEST(SkiaColor, APaletteConvertsInOrder) {
  const std::vector<SkColor4f> palette{{1, 0, 0, 1}, {0, 1, 0, 1}};
  const std::vector<Color> out = skia::toColors(palette);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0], (Color{1, 0, 0, 1}));
  EXPECT_EQ(out[1], (Color{0, 1, 0, 1}));
}

// ---------------------------------------------------------------------------
// The paint value: the three volatility tiers, the prune signature, and
// what a frame changes.

namespace {

sk_sp<SkRuntimeEffect> effectFor(const char* src) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(src));
  return effect;
}

/** A constants-only effect: nothing about it changes between draws. */
sk_sp<SkRuntimeEffect> constantEffect() {
  static sk_sp<SkRuntimeEffect> fx = effectFor(
      "uniform float uK;\n"
      "half4 main(float2 p) { return half4(half(uK), 0, 0, 1); }");
  return fx;
}

/** One that reads the clock, which is the LIVE declaration. */
sk_sp<SkRuntimeEffect> timeEffect() {
  static sk_sp<SkRuntimeEffect> fx = effectFor(
      "uniform float uTime;\n"
      "half4 main(float2 p) { return half4(half(uTime), 0, 0, 1); }");
  return fx;
}

/** One that reads the box, which is the GEOMETRY declaration. */
sk_sp<SkRuntimeEffect> resolutionEffect() {
  static sk_sp<SkRuntimeEffect> fx = effectFor(
      "uniform float2 uResolution;\n"
      "half4 main(float2 p) { return half4(half(p.x / uResolution.x), 0, 0, "
      "1); }");
  return fx;
}

}  // namespace

TEST(SkiaPaint, TheThreeTiersAreDeclaredByWhatTheEffectReads) {
  const skia::Paint flat = skia::Paint::solid({1, 0, 0, 1});
  EXPECT_FALSE(flat.isAnimated());
  EXPECT_FALSE(flat.geometryDependent());
  EXPECT_TRUE(flat.isSolid());

  skia::Paint constants = skia::Paint::sksl(constantEffect(), {{"uK", 1.0f}});
  EXPECT_FALSE(constants.isAnimated());
  EXPECT_FALSE(constants.geometryDependent());
  // A constants-only sksl paint has resolved already, so it answers a
  // shader with no frame at all.
  EXPECT_NE(constants.staticShader(), nullptr);

  EXPECT_TRUE(skia::Paint::sksl(timeEffect()).isAnimated());
  const skia::Paint sized = skia::Paint::sksl(resolutionEffect());
  EXPECT_FALSE(sized.isAnimated());
  EXPECT_TRUE(sized.geometryDependent());
  // Geometry-dependent means the frame decides: the box-less snapshot is
  // not what a consumer paints with, and the framed answer is a different
  // shader.
  EXPECT_NE(sized.shaderFor(skia::PaintFrame{.size = {100, 40}}),
            sized.staticShader());
}

TEST(SkiaPaint, ChildAndBlendInheritTheirLayersTier) {
  skia::Paint parent = skia::Paint::sksl(
      effectFor("uniform shader uSrc;\n"
                "half4 main(float2 p) { return uSrc.eval(p); }"));
  EXPECT_FALSE(parent.isAnimated());
  parent.child("uSrc", skia::Paint::sksl(timeEffect()));
  EXPECT_TRUE(parent.isAnimated());

  const skia::Paint stack = skia::Paint::blend(
      {{skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kSrc},
       {skia::Paint::sksl(resolutionEffect()), SkBlendMode::kPlus}});
  EXPECT_FALSE(stack.isAnimated());
  EXPECT_TRUE(stack.geometryDependent());
}

TEST(SkiaPaint, EqualityIsTheRecipeSoARebuiltPaintPrunes) {
  const std::vector<skia::Stop> stops{{0, {1, 0, 0, 1}}, {1, {0, 0, 1, 1}}};
  EXPECT_TRUE(skia::Paint::solid({1, 0, 0, 1}) ==
              skia::Paint::solid({1, 0, 0, 1}));
  EXPECT_FALSE(skia::Paint::solid({1, 0, 0, 1}) ==
               skia::Paint::solid({1, 0, 0.5f, 1}));
  // Two separately built gradients over the same recipe are equal even
  // though each minted its own SkShader — that is what lets a node prune
  // across describes.
  EXPECT_TRUE(skia::Paint::linear({0, 0}, {10, 0}, stops) ==
              skia::Paint::linear({0, 0}, {10, 0}, stops));
  EXPECT_FALSE(skia::Paint::linear({0, 0}, {10, 0}, stops) ==
               skia::Paint::linear({0, 0}, {20, 0}, stops));
  // The empty paint is reflexive; a holder that compared unequal to itself
  // would patch forever.
  EXPECT_TRUE(skia::Paint() == skia::Paint{});
  EXPECT_FALSE(skia::Paint{} == skia::Paint::solid({0, 0, 0, 0}));
  // A child is part of the signature: two paints with different second
  // sources must never prune onto each other.
  skia::Paint a = skia::Paint::sksl(
      effectFor("uniform shader uSrc;\n"
                "half4 main(float2 p) { return uSrc.eval(p); }"));
  skia::Paint b = a;
  a.child("uSrc", skia::Paint::solid({1, 0, 0, 1}));
  b.child("uSrc", skia::Paint::solid({0, 1, 0, 1}));
  EXPECT_FALSE(a == b);
}

TEST(SkiaPaint, AWorldSpacePaintDegradesToBoxLocalWithoutAMatrix) {
  skia::Paint anchored = skia::Paint::sksl(resolutionEffect());
  anchored.worldSpace();
  // The reader is the CONST overload; on a mutable value the same
  // spelling is the setter.
  EXPECT_TRUE(std::as_const(anchored).worldSpace());
  EXPECT_TRUE(anchored.usesWorldSpace());
  // An identity toRoot is the honest answer outside a composite: the
  // paint resolves, and it resolves box-locally.
  EXPECT_NE(anchored.shaderFor(skia::PaintFrame{.size = {64, 64}}), nullptr);
  // The flag is part of the recipe, so it cannot prune onto the unflagged
  // paint it was copied from.
  EXPECT_FALSE(anchored == skia::Paint::sksl(resolutionEffect()));
}

TEST(SkiaPaint, CopyOnWriteKeepsAMutationOffTheValueItWasCopiedFrom) {
  skia::Paint base = skia::Paint::sksl(constantEffect(), {{"uK", 1.0f}});
  skia::Paint copy = base;
  copy.uniform("uK", 0.25f);
  EXPECT_FALSE(base == copy);
  EXPECT_TRUE(base == skia::Paint::sksl(constantEffect(), {{"uK", 1.0f}}));
}
