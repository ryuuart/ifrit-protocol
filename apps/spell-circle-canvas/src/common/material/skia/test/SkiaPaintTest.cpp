/** @file
 * The paint value: the three volatility tiers, the prune signature, what
 * a frame changes, the two doors a 256-entry palette reaches an effect
 * through, and the fit that maps an image onto the box it paints.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkM44.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkString.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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

// ---------------------------------------------------------------------------
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

TEST(SkiaPaint, TheFirstBlendLayerIsTheAccumulationAndItsLayerPropsAreNot) {
  // The first layer has nothing beneath it, so neither of its layer
  // properties is read: not its blend mode, which has no destination,
  // and not its amount, which has nothing to mix back toward. Both folds
  // — the eager flatten a static blend takes and the per-draw one a
  // geometry-dependent layer defers to — are one body, so they cannot
  // disagree about that.
  skia::Paint base = skia::Paint::solid({1, 0, 0, 1});
  base.amount(0.25f);
  const skia::Paint top = skia::Paint::solid({0, 0, 1, 1});
  const SkBitmap thinned =
      render(skia::Paint::blend(
                 {{base, SkBlendMode::kSrcOver}, {top, SkBlendMode::kPlus}})
                 .staticShader());
  const SkBitmap whole =
      render(skia::Paint::blend(
                 {{skia::Paint::solid({1, 0, 0, 1}), SkBlendMode::kSrcOver},
                  {top, SkBlendMode::kPlus}})
                 .staticShader());
  EXPECT_TRUE(identical(thinned, whole));

  // The SECOND layer's amount is read, and is the whole difference
  // between the two pictures.
  skia::Paint half = top;
  half.amount(0.5f);
  const SkBitmap mixed =
      render(skia::Paint::blend(
                 {{skia::Paint::solid({1, 0, 0, 1}), SkBlendMode::kSrcOver},
                  {half, SkBlendMode::kPlus}})
                 .staticShader());
  EXPECT_FALSE(identical(mixed, whole));
  EXPECT_NEAR(SkColorGetB(mixed.getColor(1, 1)),
              SkColorGetB(whole.getColor(1, 1)) / 2, 2);
}

TEST(SkiaPaint, ASweepWindowPastTheCircleClampsRatherThanWraps) {
  // A hue wheel that is meant to start at red is written `sweep(c, stops,
  // 90, 450)` by everyone who writes it once — and no canvas angle ever
  // reaches past 360, so the run before 90 degrees paints the first
  // stop's flat colour instead of the ramp's tail. The factory says so,
  // once for the process.
  const std::vector<skia::Stop> stops{{0.0f, {1, 0, 0, 1}},
                                      {1.0f, {0, 0, 1, 1}}};
  testing::internal::CaptureStderr();
  const skia::Paint past = skia::Paint::sweep({50, 50}, stops, 90, 450);
  const std::string said = testing::internal::GetCapturedStderr();
  EXPECT_NE(said.find("do not wrap"), std::string::npos) << said;

  const SkBitmap bm = render(past.staticShader(), 100, 100);
  // Down and to the right of the centre is 45 degrees on the canvas —
  // before the window opens at 90 — so it paints the first stop flat
  // rather than the ramp the caller thought they had rotated onto it.
  EXPECT_EQ(SkColorGetR(bm.getColor(85, 85)), 255u);
  EXPECT_EQ(SkColorGetB(bm.getColor(85, 85)), 0u);
  // …and a window inside the circle says nothing at all.
  testing::internal::CaptureStderr();
  const skia::Paint inside = skia::Paint::sweep({50, 50}, stops, 0, 360);
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
  EXPECT_FALSE(inside == past);
}

TEST(SkiaPaint, AConicalRampMovesItsHotSpotAndLeavesItsOuterCircle) {
  // What a radial cannot do: moving a radial's centre slides the whole
  // ramp, its outer edge included, where a conical keeps the outer circle
  // where it was put and moves only the focus. A displaced highlight on a
  // sphere is the case, and the corner farthest from the displacement is
  // where the difference shows.
  const std::vector<skia::Stop> stops{{0.0f, {1, 1, 1, 1}},
                                      {1.0f, {0, 0, 0, 1}}};
  const SkBitmap centred =
      render(skia::Paint::radial({50, 50}, 50, stops).staticShader(), 100, 100);
  const SkBitmap displaced = render(
      skia::Paint::conical({30, 30}, 0, {50, 50}, 50, stops).staticShader(),
      100, 100);
  const SkBitmap slid =
      render(skia::Paint::radial({30, 30}, 50, stops).staticShader(), 100, 100);

  // The hot spot moved in both.
  EXPECT_GT(SkColorGetR(displaced.getColor(30, 30)),
            SkColorGetR(centred.getColor(30, 30)));
  // The outer circle did not, in the conical: at the left edge the ramp
  // has all but run out, as it had before the focus moved — where the
  // slid radial, whose whole ramp went with its centre, still has a long
  // way to go there.
  EXPECT_LT(SkColorGetR(displaced.getColor(2, 50)),
            SkColorGetR(slid.getColor(2, 50)));
  EXPECT_NEAR(SkColorGetR(displaced.getColor(2, 50)),
              SkColorGetR(centred.getColor(2, 50)), 24);
}

TEST(SkiaPaint, ABufferPrunesUntilItIsCommitted) {
  // The whole point of a buffer over a custom leaf: the node keeps its
  // picture caching, because the recipe compares by (source, revision).
  // An identical re-describe between commits is the same value, and the
  // first describe after a commit is a different one — exactly once.
  auto pixels = std::make_shared<skia::PixelBuffer>(4, 4);
  pixels->bitmap().eraseColor(SK_ColorRED);
  const skia::Paint described = skia::Paint::buffer(pixels);
  EXPECT_TRUE(described == skia::Paint::buffer(pixels));

  // Writing without committing publishes nothing: the snapshot the
  // shader holds is the one taken at the last commit, so a describe over
  // an edited-but-uncommitted buffer still prunes.
  pixels->bitmap().eraseColor(SK_ColorBLUE);
  EXPECT_TRUE(described == skia::Paint::buffer(pixels));
  EXPECT_EQ(SkColorGetR(render(described.staticShader()).getColor(1, 1)), 255u);

  pixels->commit();
  const skia::Paint after = skia::Paint::buffer(pixels);
  EXPECT_FALSE(described == after);
  EXPECT_TRUE(after == skia::Paint::buffer(pixels));
  EXPECT_EQ(SkColorGetB(render(after.staticShader()).getColor(1, 1)), 255u);

  // A null buffer is a paint that draws nothing rather than a crash.
  EXPECT_EQ(skia::Paint::buffer(nullptr).staticShader(), nullptr);
}

TEST(SkiaPaint, SettingOneUniformTwiceReplacesItRatherThanStacking) {
  // Two entries under one name are both assigned by the builder, so the
  // picture is the last one either way — but the lane grows without
  // bound in a live-coding host that re-describes every frame, and the
  // paint stops comparing equal to the same paint described once, which
  // is what a node prunes on.
  skia::Paint twice = skia::Paint::sksl(constantEffect());
  twice.uniform("uK", 0.25f);
  twice.uniform("uK", 1.0f);
  EXPECT_TRUE(twice == skia::Paint::sksl(constantEffect(), {{"uK", 1.0f}}));
  EXPECT_TRUE(identical(
      render(twice.staticShader()),
      render(
          skia::Paint::sksl(constantEffect(), {{"uK", 1.0f}}).staticShader())));
}

TEST(SkiaPaint, TwoThreadsResolveOneSharedPaintsMemo) {
  // Copies of a Paint share the state its resolve memo hangs off, and a
  // host may paint two composers on two threads: both reach the memo,
  // and the answer has to be a whole shader either way.
  skia::Paint live = skia::Paint::sksl(resolutionEffect());
  EXPECT_TRUE(live.geometryDependent());
  std::vector<std::thread> painters;
  std::atomic<int> built{0};
  for (int t = 0; t < 4; ++t)
    painters.emplace_back([copy = live, t, &built] {
      for (int i = 0; i < 64; ++i) {
        const float side = (float)(8 + ((t + i) % 16));
        if (copy.shaderFor(skia::PaintFrame{.size = {side, side}})) ++built;
      }
    });
  for (std::thread& painter : painters) painter.join();
  EXPECT_EQ(built.load(), 4 * 64);
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

namespace {

/** Two pixels side by side, red then blue — a source whose halves are
 *  told apart wherever it lands. */
sk_sp<SkImage> twoHalves() {
  SkBitmap bm;
  bm.allocPixels(
      SkImageInfo::Make(2, 1, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  *bm.getAddr32(0, 0) = 0xFF0000FFu;  // ABGR: opaque red
  *bm.getAddr32(1, 0) = 0xFFFF0000u;  // opaque blue
  bm.setImmutable();
  return bm.asImage();
}

/** The image's POINTER is its identity in a recipe, so every case here
 *  fits the same one — two paints over two copies of the same pixels are
 *  deliberately not equal. */
sk_sp<SkImage> theHalves() {
  static const sk_sp<SkImage> image = twoHalves();
  return image;
}

skia::Paint fitted(skia::Fit how) {
  skia::Paint p = skia::Paint::image(theHalves(), SkTileMode::kDecal,
                                     SkTileMode::kDecal, SkMatrix::I(),
                                     SkSamplingOptions(SkFilterMode::kNearest));
  p.fit(how);
  return p;
}

}  // namespace

TEST(SkiaPaint, AFittedImageIsMappedOntoTheBoxItIsPainting) {
  // Native is the absence of the question: two source pixels at the
  // origin, and the rest of the box is not the image's business.
  const skia::Paint native = fitted(skia::Fit::Native);
  EXPECT_FALSE(native.geometryDependent());
  EXPECT_EQ(SkColorGetA(render(native.staticShader(), 40, 10).getColor(20, 5)),
            0u);

  // Stretch fills both axes: the halves land either side of the middle and
  // the whole box is covered.
  const skia::Paint stretch = fitted(skia::Fit::Stretch);
  EXPECT_TRUE(stretch.geometryDependent());
  const SkBitmap wide =
      render(stretch.shaderFor(skia::PaintFrame{.size = {40, 10}}), 40, 10);
  EXPECT_EQ(wide.getColor(5, 5), SK_ColorRED);
  EXPECT_EQ(wide.getColor(35, 5), SK_ColorBLUE);
  EXPECT_EQ(wide.getColor(5, 9), SK_ColorRED);

  // Contain keeps the aspect and leaves the margin: a 2:1 source in a
  // square box is half the height of it, centred, and the box's own top
  // is not the image.
  const SkBitmap inside = render(
      fitted(skia::Fit::Contain).shaderFor(skia::PaintFrame{.size = {40, 40}}),
      40, 40);
  EXPECT_EQ(SkColorGetA(inside.getColor(5, 5)), 0u);
  EXPECT_EQ(inside.getColor(5, 20), SK_ColorRED);
  EXPECT_EQ(inside.getColor(35, 20), SK_ColorBLUE);

  // Cover keeps the aspect the other way: nothing of the box is left, and
  // what does not fit is off the edges.
  const SkBitmap over = render(
      fitted(skia::Fit::Cover).shaderFor(skia::PaintFrame{.size = {40, 40}}),
      40, 40);
  EXPECT_EQ(over.getColor(5, 2), SK_ColorRED);
  EXPECT_EQ(over.getColor(5, 38), SK_ColorRED);
  EXPECT_EQ(over.getColor(35, 20), SK_ColorBLUE);
}

TEST(SkiaPaint, AFitIsPartOfTheRecipeAndDegradesWhereThereIsNoBox) {
  EXPECT_FALSE(fitted(skia::Fit::Cover) == fitted(skia::Fit::Contain));
  EXPECT_FALSE(fitted(skia::Fit::Cover) == fitted(skia::Fit::Native));
  EXPECT_TRUE(fitted(skia::Fit::Cover) == fitted(skia::Fit::Cover));

  // Asked with no box in reach — a standalone decoration, a measurement —
  // it answers the unfitted mapping rather than nothing at all, the same
  // degradation a world-space material makes outside a composer.
  const SkBitmap loose = render(fitted(skia::Fit::Cover).asShader(), 40, 40);
  EXPECT_EQ(SkColorGetA(loose.getColor(20, 20)), 0u);
  EXPECT_EQ(loose.getColor(0, 0), SK_ColorRED);
}
