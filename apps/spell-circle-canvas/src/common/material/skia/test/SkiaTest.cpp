/** @file
 * The SkSL backend: a two-uniform recipe compiles through the cache,
 * resolves, and shades a raster byte-identically to the same SkSL
 * compiled and filled by hand; a child slot samples another material.
 * Beside them, the colour bridge's round trip.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

TEST(SkiaPaint, APassBodyIsNotCompiledAsAShaderOfItsOwn) {
  skia::install();
  // A pass body is written against declarations the fx() runtime
  // prepends once it knows the track's unit count. Compiled standalone it
  // names four things that do not exist yet and the compiler reports one
  // error per mention — a page of diagnostics about a compile nobody
  // asked for, on a material that then renders correctly through the
  // pass path. So it is not attempted.
  auto pass = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("pass.body")
          .body(Target::SkSL,
                "half4 main(float2 p) {\n"
                "  half4 c = uContent.eval(p);\n"
                "  for (int i = 0; i < kUnitCount; ++i)\n"
                "    c += half4(uUnitRect[i]) * uUnitPhase[i].x;\n"
                "  return c * half4(uColor * uScale);\n"
                "}"));
  EXPECT_TRUE(skia::detail::isPassBody(*pass));
  std::string said;
  {
    testing::internal::CaptureStderr();
    const skia::Paint paint = skia::Paint::recipe(Material(pass));
    // Nothing to draw on its own — a pass material used as an ordinary
    // fill has no picture to give, and now it says so by drawing nothing.
    EXPECT_EQ(paint.staticShader(), nullptr);
    said = testing::internal::GetCapturedStderr();
  }
  EXPECT_EQ(said, "") << said;

  // An ordinary recipe is unaffected: it still compiles at the paint.
  auto plain = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("pass.notone").body(Target::SkSL, kBody));
  EXPECT_FALSE(skia::detail::isPassBody(*plain));
  EXPECT_NE(skia::Paint::recipe(Material(plain)).staticShader(), nullptr);
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

// ---------------------------------------------------------------------------
// The post-processing value.

TEST(SkiaEffect, AFilterIsBuiltOnceAndComparesByItsIdentity) {
  const skia::Effect glow = skia::Effect::glow({0, 1, 1, 1}, 6.0f);
  EXPECT_NE(glow.resolvedImageFilter(nullptr), nullptr);
  EXPECT_FALSE(glow.isAnimated());
  // filter() compares by the built filter's pointer, so a copy prunes and
  // a separately built one does not.
  EXPECT_TRUE(glow == skia::Effect(glow));
  EXPECT_FALSE(glow == skia::Effect::glow({0, 1, 1, 1}, 6.0f));
  // The empty effect resolves to nothing and is reflexive.
  EXPECT_EQ(skia::Effect().resolvedImageFilter(nullptr), nullptr);
  EXPECT_TRUE(skia::Effect() == skia::Effect{});
}

TEST(SkiaEffect, ABoundUniformMakesItLiveAndItNeverPrunes) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "uniform float uK;\n"
               "half4 main(float2 p) { return content.eval(p) * half(uK); }"));
  ASSERT_NE(effect, nullptr);
  choreograph::Output<float> k(1.0f);
  skia::Effect live = skia::Effect::shader(effect);
  EXPECT_FALSE(live.isAnimated());
  live.uniform("uK", &k);
  EXPECT_TRUE(live.isAnimated());
  // Live never prunes — the same rule a live paint follows.
  EXPECT_FALSE(live == live);
}

TEST(SkiaEffect, ChainingPrecomposesAndAnEmptySideIsTheOther) {
  const skia::Effect blur = skia::Effect::directionalBlur(4.0f, 0.0f, 1.0f);
  const skia::Effect glow = skia::Effect::glow({1, 0, 0, 1}, 3.0f);
  EXPECT_NE(blur.then(glow).resolvedImageFilter(nullptr), nullptr);
  // then() over nothing is the effect itself, so a conditional chain
  // needs no branch at the call site.
  EXPECT_TRUE(blur.then(skia::Effect{}) == blur);
  EXPECT_TRUE(skia::Effect{}.then(blur) == blur);
}

// ---------------------------------------------------------------------------
// A FIXED PALETTE THROUGH AN EFFECT. An indexed picture — a 1994 sprite
// sheet, a datashader's category ramp — is one channel of indices and one
// 256-entry table; both doors an effect has for that table are here, so a
// consumer never has to bake one sprite per palette.

namespace {

/** Entry i is (i/255, 1 - i/255, 0, 1): the index and its colour are the
 *  same fact stated twice, so a wrong lookup is visible in the pixel. */
std::vector<SkColor4f> paletteTable() {
  std::vector<SkColor4f> pal((size_t)256);
  for (int i = 0; i < 256; ++i)
    pal[(size_t)i] = {(float)i / 255.0f, 1.0f - (float)i / 255.0f, 0.0f, 1.0f};
  return pal;
}

/** The same table as the 256 x 1 unpremultiplied image a child slot takes. */
sk_sp<SkImage> paletteImage(const std::vector<SkColor4f>& pal) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::Make(256, 1, kRGBA_8888_SkColorType,
                                   kUnpremul_SkAlphaType));
  for (int i = 0; i < 256; ++i) {
    const SkColor4f c = pal[(size_t)i];
    auto b = [](float v) {
      return (uint32_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
    };
    *bm.getAddr32(i, 0) =
        b(c.fR) | (b(c.fG) << 8) | (b(c.fB) << 16) | (b(c.fA) << 24);
  }
  bm.setImmutable();
  return bm.asImage();
}

}  // namespace

TEST(SkiaPaint, APaletteReachesAnEffectAsOneChildImage) {
  const std::vector<SkColor4f> pal = paletteTable();
  // The lookup a fixed-palette picture needs is DYNAMIC — the index is a
  // pixel value, not a literal — and a sampled 256 x 1 strip is the form
  // that takes: nearest, at the texel centre, so entry 200 is entry 200
  // and not a blend of two unrelated ones.
  skia::Paint lut = skia::Paint::sksl(effectFor(
      "uniform shader uPalette;\n"
      "uniform float uIndex;\n"
      "half4 main(float2 p) {\n"
      "  return uPalette.eval(float2(uIndex + 0.5, 0.5));\n"
      "}"));
  lut.uniform("uIndex", 200.0f);
  lut.child("uPalette",
            skia::Paint::image(paletteImage(pal), SkTileMode::kClamp,
                               SkTileMode::kClamp, SkMatrix::I(),
                               SkSamplingOptions(SkFilterMode::kNearest)));
  // One child, one uniform: the whole table is in the shader and nothing
  // was baked per entry.
  sk_sp<SkShader> shader = lut.staticShader();
  ASSERT_NE(shader, nullptr);
  const SkBitmap bm = render(shader);
  const SkColor got = bm.getColor(1, 1);
  EXPECT_EQ(SkColorGetR(got), 200u);
  EXPECT_EQ(SkColorGetG(got), 55u);
  EXPECT_EQ(SkColorGetB(got), 0u);
  EXPECT_EQ(SkColorGetA(got), 255u);
}

TEST(SkiaPaint, APaletteReachesAnEffectAsOneUniformArray) {
  const std::vector<SkColor4f> pal = paletteTable();
  std::vector<float> flat;
  flat.reserve(pal.size() * 4);
  for (const SkColor4f& c : pal) {
    flat.push_back(c.fR);
    flat.push_back(c.fG);
    flat.push_back(c.fB);
    flat.push_back(c.fA);
  }
  ASSERT_EQ(flat.size(), 1024u);

  // The array door: 1024 floats fill `float4 uPalette[256]`, because the
  // builder matches the DECLARED TOTAL float count and nothing finer.
  skia::Paint lut = skia::Paint::sksl(
      effectFor("uniform float4 uPalette[256];\n"
                "half4 main(float2 p) { return half4(uPalette[200]); }"));
  lut.uniform("uPalette", flat);
  sk_sp<SkShader> shader = lut.staticShader();
  ASSERT_NE(shader, nullptr);
  const SkBitmap bm = render(shader);
  const SkColor got = bm.getColor(1, 1);
  EXPECT_EQ(SkColorGetR(got), 200u);
  EXPECT_EQ(SkColorGetG(got), 55u);

  // A count that is not the declaration's is refused whole rather than
  // written partly: the paint keeps the table it had.
  skia::Paint partial = lut;
  partial.uniform("uPalette", std::vector<float>(8, 1.0f));
  const SkBitmap same = render(partial.staticShader());
  EXPECT_TRUE(identical(bm, same));
}
