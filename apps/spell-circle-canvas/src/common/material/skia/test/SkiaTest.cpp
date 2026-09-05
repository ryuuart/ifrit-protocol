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
#include <sigilshaders/MaterialSkia.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ShaderTable.h"

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

// A GPU backend inlines a runtime effect's body into its fragment shader
// under parameters it names itself, discarding the names the body's own
// main declared: `pos` for the coordinates, `inColor` for the colour from
// the stage before, `destColor` for a blender's destination,
// `primitiveColor` for the draw's own. A body declaring anything else by
// one of those names redeclares a parameter — which is invisible to
// SkRuntimeEffect::MakeForShader, where the body IS the whole program and
// the name is free, and fatal on a device. So the compile refuses them
// here, on the CPU, where the refusal is a resolve that answers nothing
// and a message naming the recipe.
TEST(SkiaCompiler, ABodyDeclaringAReservedParameterNameResolvesToNoProgram) {
  skia::install();
  const auto refused = [](const char* body) {
    static int serial = 0;
    auto recipe = std::make_shared<const Recipe>(
        Recipe::of<TwoParams>("reserved." + std::to_string(serial++))
            .body(Target::SkSL, body));
    return skia::shader(Material(recipe), FrameData{}) == nullptr;
  };
  EXPECT_TRUE(
      refused("half4 main(float2 p) { float2 pos = p * 0.5; "
              "return half4(half2(pos), 0.0, 1.0); }"));
  EXPECT_TRUE(
      refused("half4 main(float2 p) { half4 inColor = half4(1.0); "
              "return inColor; }"));
  EXPECT_TRUE(
      refused("half4 main(float2 p) { half4 destColor = half4(1.0); "
              "return destColor; }"));
  EXPECT_TRUE(
      refused("half4 main(float2 p) { half4 primitiveColor = half4(1.0); "
              "return primitiveColor; }"));
  // A helper's parameter is a declaration too, and lands in the same
  // generated scope.
  EXPECT_TRUE(
      refused("float2 shift(float2 pos) { return pos * 0.5; }\n"
              "half4 main(float2 p) { return half4(half2(shift(p)), "
              "0.0, 1.0); }"));

  // What must still pass: main's OWN parameter by that name, which is the
  // one declaration the backend replaces rather than collides with; and
  // the word in a comment or inside another identifier.
  EXPECT_FALSE(
      refused("half4 main(float2 pos) { return half4(half2(pos), "
              "0.0, 1.0); }"));
  EXPECT_FALSE(
      refused("half4 main(float2 p) { /* float2 pos; */ float2 "
              "position = p; return half4(half2(position), 0.0, "
              "1.0); }"));
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

namespace {

/** A 32x32 white layer with a black square in the middle, painted
 *  through @p filter as one layer — the smallest picture a blur changes. */
SkBitmap squareThrough(const sk_sp<SkImageFilter>& filter) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(32, 32));
  SkCanvas canvas(bm);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint layer;
  layer.setImageFilter(filter);
  canvas.saveLayer(nullptr, &layer);
  canvas.clear(SK_ColorWHITE);
  SkPaint ink;
  ink.setColor(SK_ColorBLACK);
  canvas.drawRect(SkRect::MakeXYWH(12, 12, 8, 8), ink);
  canvas.restore();
  return bm;
}

}  // namespace

TEST(SkiaEffect, ABoundBlurSigmaRidesInsideTheDeclaredPyramid) {
  // The declared range builds the pyramid once; a bound sigma re-wraps
  // only the mix, so two resolves at different sigmas share their blur
  // inputs by identity — which is what lets Skia's filter cache keep the
  // blurred layers between frames while the sigma breathes.
  choreograph::Output<float> sigma(2.0f);
  skia::Effect blur =
      skia::Effect::blur(skia::Paint::solid({1, 1, 1, 1}), 8.0f);
  blur.uniform("maxSigma", &sigma);
  EXPECT_TRUE(blur.isAnimated());
  const sk_sp<SkImageFilter> at2 = blur.resolvedImageFilter(nullptr);
  sigma = 6.0f;
  const sk_sp<SkImageFilter> at6 = blur.resolvedImageFilter(nullptr);
  ASSERT_NE(at2, nullptr);
  ASSERT_NE(at6, nullptr);
  EXPECT_NE(at2, at6);  // the mix is re-wrapped for the new sigma
  ASSERT_EQ(at2->countInputs(), 3);
  ASSERT_EQ(at6->countInputs(), 3);
  EXPECT_EQ(at2->getInput(1), at6->getInput(1));
  EXPECT_EQ(at2->getInput(2), at6->getInput(2));

  // Exact at a pass sigma: a white map bound to half the declared range
  // IS the half-range pass, so it paints what a blur declared at that
  // sigma with no binding paints.
  sigma = 4.0f;
  const SkBitmap ridden = squareThrough(blur.resolvedImageFilter(nullptr));
  const SkBitmap declared =
      squareThrough(skia::Effect::blur(skia::Paint::solid({1, 1, 1, 1}), 4.0f)
                        .resolvedImageFilter(nullptr));
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x)
      EXPECT_NEAR((int)SkColorGetR(ridden.getColor(x, y)),
                  (int)SkColorGetR(declared.getColor(x, y)), 1)
          << "at " << x << "," << y;
  // The edge of the square is softened, so the picture is a blur at all.
  EXPECT_GT(SkColorGetR(ridden.getColor(11, 16)), 0u);
  EXPECT_LT(SkColorGetR(ridden.getColor(11, 16)), 255u);

  // Above the declared range the sigma clamps to it: the top of the
  // pyramid is the widest the effect ever paints.
  sigma = 40.0f;
  const SkBitmap clamped = squareThrough(blur.resolvedImageFilter(nullptr));
  const SkBitmap top =
      squareThrough(skia::Effect::blur(skia::Paint::solid({1, 1, 1, 1}), 8.0f)
                        .resolvedImageFilter(nullptr));
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x)
      EXPECT_NEAR((int)SkColorGetR(clamped.getColor(x, y)),
                  (int)SkColorGetR(top.getColor(x, y)), 1)
          << "at " << x << "," << y;
}

TEST(SkiaEffect, PhosphorBloomIsAComparableSpectralPostProcess) {
  const skia::Effect bloom =
      skia::Effect::phosphorBloom(8.0f, 0.6f, 0.4f, 0.75f);
  EXPECT_NE(bloom.resolvedImageFilter(nullptr), nullptr);
  EXPECT_FALSE(bloom.isAnimated());
  EXPECT_TRUE(bloom == skia::Effect::phosphorBloom(8.0f, 0.6f, 0.4f, 0.75f));
  EXPECT_FALSE(bloom == skia::Effect::phosphorBloom(10.0f, 0.6f, 0.4f, 0.75f));
}

namespace {

/** A source for a bloom: a 64x64 black field with a bright 24x24 square
 *  of @p color in the middle, painted through @p filter as one layer
 *  onto an F32 surface so a sum above one survives the read-back. */
std::vector<float> bloomThrough(const sk_sp<SkImageFilter>& filter,
                                SkColor4f color) {
  const SkImageInfo info =
      SkImageInfo::Make(64, 64, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorBLACK);
  SkPaint layer;
  layer.setImageFilter(filter);
  canvas.saveLayer(nullptr, &layer);
  canvas.clear(SK_ColorBLACK);
  SkPaint ink;
  ink.setColor(color);
  canvas.drawRect(SkRect::MakeXYWH(20, 20, 24, 24), ink);
  canvas.restore();
  std::vector<float> px((size_t)64 * 64 * 4);
  EXPECT_TRUE(surface->readPixels(SkPixmap(info, px.data(), 64 * 4 * 4), 0, 0));
  return px;
}

const float* texel(const std::vector<float>& px, int x, int y) {
  return px.data() + ((size_t)y * 64 + x) * 4;
}

/** The three-kernel spectral falloff with no hue drift and no tail,
 *  written out in full: what the defaults must paint, to the bit. */
constexpr char kPlainPhosphor[] = R"(
uniform shader content;
uniform float uRadius;
uniform float uThreshold;
uniform float uIntensity;
uniform float uChroma;

half3 bright(float2 p) {
  half3 color = content.eval(p).rgb;
  half peak = max(color.r, max(color.g, color.b));
  half gate = smoothstep(half(uThreshold),
                         half(min(uThreshold + 0.30, 1.0)), peak);
  return color * gate;
}

half3 ring(float2 p, float radius) {
  float diagonal = radius * 0.70710678;
  half3 sum = bright(p + float2( radius, 0.0));
  sum += bright(p + float2(-radius, 0.0));
  sum += bright(p + float2(0.0,  radius));
  sum += bright(p + float2(0.0, -radius));
  sum += bright(p + float2( diagonal,  diagonal));
  sum += bright(p + float2(-diagonal,  diagonal));
  sum += bright(p + float2( diagonal, -diagonal));
  sum += bright(p + float2(-diagonal, -diagonal));
  return sum * 0.125;
}

half4 main(float2 p) {
  half4 source = content.eval(p);
  half3 near = ring(p, uRadius * 0.28);
  half3 middle = ring(p, uRadius * 0.62);
  half3 far = ring(p, uRadius);

  half3 common = near * 0.52 + middle * 0.31 + far * 0.17;
  half3 spectral = half3(
      near.r * 0.16 + middle.r * 0.29 + far.r * 0.55,
      near.g * 0.27 + middle.g * 0.50 + far.g * 0.23,
      near.b * 0.58 + middle.b * 0.29 + far.b * 0.13);
  half3 bloom = mix(common, spectral, half(uChroma));
  return half4(source.rgb + bloom * half(uIntensity), source.a);
}
)";

}  // namespace

TEST(SkiaEffect, PhosphorBloomDefaultsAreThePlainFalloffToTheBit) {
  // The hue drift and the tail default to zero, and zero is the falloff
  // without them: every texel of a bloomed source through the defaults
  // equals the plain three-kernel program, so no picture made before
  // either parameter existed moves.
  auto [plain, error] =
      SkRuntimeEffect::MakeForShader(SkString(kPlainPhosphor));
  ASSERT_NE(plain, nullptr) << error.c_str();
  const skia::Effect oracle =
      skia::Effect::shader(plain, {{"uRadius", 9.0f},
                                   {"uThreshold", 0.52f},
                                   {"uIntensity", 0.46f},
                                   {"uChroma", 0.80f}});
  const SkColor4f amber{1.0f, 0.72f, 0.1f, 1.0f};
  const std::vector<float> want =
      bloomThrough(oracle.resolvedImageFilter(nullptr), amber);
  const std::vector<float> got = bloomThrough(
      skia::Effect::phosphorBloom().resolvedImageFilter(nullptr), amber);
  ASSERT_EQ(want.size(), got.size());
  for (size_t i = 0; i < want.size(); ++i)
    ASSERT_EQ(want[i], got[i]) << "float " << i;
  // And the picture is a bloom at all: the field beside the square is lit.
  EXPECT_GT(texel(got, 48, 32)[0], 0.0f);
}

TEST(SkiaEffect, PhosphorHueDriftTurnsTheHaloAndNotTheSource) {
  // A negative drift takes an amber halo toward red: at the halo's edge
  // the green share of the light drops against the red, while the centre
  // of the square — lit by its own source — is the same texel with or
  // without the drift.
  const SkColor4f amber{1.0f, 0.72f, 0.1f, 1.0f};
  const std::vector<float> still =
      bloomThrough(skia::Effect::phosphorBloom(9, 0.52f, 0.46f, 0.80f, 0, 0)
                       .resolvedImageFilter(nullptr),
                   amber);
  const std::vector<float> drifted = bloomThrough(
      skia::Effect::phosphorBloom(9, 0.52f, 0.46f, 0.80f, -40.0f, 0)
          .resolvedImageFilter(nullptr),
      amber);
  for (int c = 0; c < 4; ++c)
    EXPECT_EQ(texel(still, 32, 32)[c], texel(drifted, 32, 32)[c]) << c;
  const float* edgeStill = texel(still, 49, 32);  // 5 px past the square
  const float* edgeDrift = texel(drifted, 49, 32);
  ASSERT_GT(edgeStill[0], 0.0f);
  ASSERT_GT(edgeDrift[0], 0.0f);
  EXPECT_LT(edgeDrift[1] / edgeDrift[0], edgeStill[1] / edgeStill[0]);

  // A cool source drifts the other way round the wheel by the same
  // rule: blue's halo gains green against blue.
  const SkColor4f blue{0.2f, 0.3f, 1.0f, 1.0f};
  const std::vector<float> coolStill =
      bloomThrough(skia::Effect::phosphorBloom(9, 0.52f, 0.46f, 0.80f, 0, 0)
                       .resolvedImageFilter(nullptr),
                   blue);
  const std::vector<float> coolDrift = bloomThrough(
      skia::Effect::phosphorBloom(9, 0.52f, 0.46f, 0.80f, -40.0f, 0)
          .resolvedImageFilter(nullptr),
      blue);
  const float* coolEdgeStill = texel(coolStill, 49, 32);
  const float* coolEdgeDrift = texel(coolDrift, 49, 32);
  EXPECT_GT(coolEdgeDrift[1] / coolEdgeDrift[2],
            coolEdgeStill[1] / coolEdgeStill[2]);

  // The tail adds reach: the far field is brighter with it, the source
  // centre unchanged in hue.
  const std::vector<float> tailed =
      bloomThrough(skia::Effect::phosphorBloom(9, 0.52f, 0.46f, 0.80f, 0, 0.5f)
                       .resolvedImageFilter(nullptr),
                   amber);
  EXPECT_GT(texel(tailed, 52, 32)[0], texel(still, 52, 32)[0]);
  // Comparable by recipe, as any shader effect: the new parameters are
  // constant uniforms and take part in equality.
  EXPECT_FALSE(skia::Effect::phosphorBloom() ==
               skia::Effect::phosphorBloom(9, 0.52f, 0.46f, 0.80f, -40.0f));
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
  bm.allocPixels(
      SkImageInfo::Make(256, 1, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
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
  skia::Paint lut = skia::Paint::sksl(
      effectFor("uniform shader uPalette;\n"
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

// ---- the embedded shader table --------------------------------------------

TEST(ShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      sigil::material::skia::shaderSources(), SIGIL_MATERIAL_SKIA_SHADER_DIR);
}
