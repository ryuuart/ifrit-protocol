/** @file
 * THE LIGHT AN EFFECT EMITS, as distinct from the light it adds: the
 * bright pass on its own, emitted as a layer with its own coverage; the
 * emitted light leaving the sharp layer at device resolution; the four
 * optical-bloom stages — spread, deepening, whitening, dilation — each
 * on its own and then as the composition they add up to; and the dilate
 * and emit doors under them.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/skia/Bloom.h>
#include <sigilmaterial/skia/Effect.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "support/EffectRead.h"

using namespace sigil::material;
using sigil::material::test::bloomThrough;
using sigil::material::test::texel;

// ---------------------------------------------------------------------------
// THE BRIGHT PASS ON ITS OWN: the first half of a bloom, emitted as a
// layer with its own coverage rather than as light to add.

namespace {

/** A 64x64 EMPTY field with one 24x24 square of @p color in the middle,
 *  run through @p filter as a layer onto an F32 surface. The layer is
 *  left transparent outside the square, which is what lets a partly
 *  covered source reach the pass at its own alpha. */
std::vector<float> brightThrough(const sk_sp<SkImageFilter>& filter,
                                 SkColor4f color) {
  const SkImageInfo info =
      SkImageInfo::Make(64, 64, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint layer;
  layer.setImageFilter(filter);
  canvas.saveLayer(nullptr, &layer);
  SkPaint ink;
  ink.setColor(color);
  canvas.drawRect(SkRect::MakeXYWH(20, 20, 24, 24), ink);
  canvas.restore();
  std::vector<float> px((size_t)64 * 64 * 4);
  EXPECT_TRUE(surface->readPixels(SkPixmap(info, px.data(), 64 * 4 * 4), 0, 0));
  return px;
}

/** A 64x64 scene holding one thin bright diagonal, painted through
 *  @p filter as one layer onto a surface SCALED by @p scale — so the
 *  picture is read in DEVICE pixels, and a filter graph that can only be
 *  placed by a translation shows the resample it forces on everything it
 *  keeps. A thin antialiased line is the instrument: its coverage at the
 *  layer's own resolution is nothing like its coverage at twice it, and
 *  an eight-bit ground is what a filtered layer is composited through
 *  anyway, so a light that adds nothing leaves the codes alone. */
std::vector<uint8_t> scaledThrough(const sk_sp<SkImageFilter>& filter,
                                   int scale) {
  const int side = 64 * scale;
  const SkImageInfo info = SkImageInfo::MakeN32Premul(side, side);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorBLACK);
  canvas.scale((float)scale, (float)scale);
  SkPaint layer;
  layer.setImageFilter(filter);
  canvas.saveLayer(nullptr, &layer);
  canvas.clear(SK_ColorBLACK);
  SkPaint ink;
  ink.setColor(SkColor4f{1.0f, 0.92f, 0.45f, 1.0f});
  ink.setAntiAlias(true);
  ink.setStroke(true);
  ink.setStrokeWidth(0.75f);
  canvas.drawLine(9.5f, 12.25f, 53.5f, 49.75f, ink);
  canvas.restore();
  std::vector<uint8_t> px((size_t)side * side * 4);
  EXPECT_TRUE(
      surface->readPixels(SkPixmap(info, px.data(), (size_t)side * 4), 0, 0));
  return px;
}

/** A light that adds nothing: the alpha-scaling colour matrix at
 *  @p alpha, which at zero leaves whatever it is blended over. */
skia::Effect dimming(float alpha) {
  const float m[20] = {1, 0, 0, 0, 0, 0, 1, 0, 0,     0,
                       0, 0, 1, 0, 0, 0, 0, 0, alpha, 0};
  return skia::Effect::filter(SkColorFilters::Matrix(m));
}

}  // namespace

TEST(SkiaEffect, AnEmittedLightLeavesTheSharpLayerAtDeviceResolution) {
  // A light that is wholly transparent adds nothing, so on a canvas
  // carrying a scale the layer beneath it must come back exactly as it
  // was drawn. The bright pass reads its own pixel and no neighbour, so
  // it is a colour map, and a filter graph carrying one is evaluated in
  // the device's own pixels.
  const std::vector<uint8_t> plain = scaledThrough(nullptr, 2);
  const std::vector<uint8_t> overPass = scaledThrough(
      skia::Effect()
          .emit(skia::Effect::brightPass().then(dimming(0)))
          .resolvedImageFilter(nullptr),
      2);
  ASSERT_EQ(plain.size(), overPass.size());
  for (size_t i = 0; i < plain.size(); ++i)
    ASSERT_EQ((int)plain[i], (int)overPass[i]) << i;

  // The case discriminates, and this is what it is guarding against: a
  // light made of a program over COORDINATES, transparent though it is,
  // has the whole graph — the kept layer with it — evaluated in the
  // layer's own coordinates and resampled up, which moves the thin
  // antialiased diagonal it was supposed to keep. The light is the same
  // light as above, dimmed to nothing by the same colour matrix; only
  // the pass in front of it is a program over coordinates rather than
  // over a colour.
  auto [passThrough, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "half4 main(float2 p) { return content.eval(p); }"));
  ASSERT_NE(passThrough, nullptr) << error.c_str();
  // A program whose child the filter factory will not bind builds no
  // node at all, and a graph with no node in it is no control.
  ASSERT_NE(skia::Effect::shader(passThrough).imageFilter(), nullptr);
  const std::vector<uint8_t> overShader = scaledThrough(
      skia::Effect()
          .emit(skia::Effect::shader(passThrough).then(dimming(0)))
          .resolvedImageFilter(nullptr),
      2);
  ASSERT_EQ(plain.size(), overShader.size());
  int worst = 0;
  for (size_t i = 0; i < plain.size(); ++i)
    worst = std::max(worst, std::abs((int)plain[i] - (int)overShader[i]));
  EXPECT_GT(worst, 1);
}

TEST(SkiaEffect, TheBrightPassKeepsTheLightAboveItsKneeAndNothingElse) {
  const sk_sp<SkImageFilter> pass =
      skia::Effect::brightPass().resolvedImageFilter(nullptr);
  // White is wholly above the knee and comes through whole.
  const std::vector<float> white = brightThrough(pass, {1, 1, 1, 1});
  EXPECT_NEAR(texel(white, 32, 32)[0], 1.0f, 0.01f);
  EXPECT_NEAR(texel(white, 32, 32)[3], 1.0f, 0.01f);
  // Nothing outside the source: the pass adds no light of its own.
  EXPECT_NEAR(texel(white, 4, 4)[3], 0.0f, 0.001f);

  // A middling grey is below the knee and is dropped entirely.
  const std::vector<float> grey = brightThrough(pass, {0.5f, 0.5f, 0.5f, 1});
  EXPECT_NEAR(texel(grey, 32, 32)[3], 0.0f, 0.01f);

  // And one inside the knee comes through at the smoothstep's own
  // fraction: 0.85 is 0.5667 of the way from 0.68 to 0.98, which the
  // cubic maps to 0.5994.
  const std::vector<float> partial =
      brightThrough(pass, {0.85f, 0.85f, 0.85f, 1});
  EXPECT_NEAR(texel(partial, 32, 32)[3], 0.5994f, 0.01f);
  EXPECT_NEAR(texel(partial, 32, 32)[0], 0.85f * 0.5994f, 0.01f);
}

TEST(SkiaEffect, TheBrightPassGatesOnColourRatherThanOnCoverage) {
  const sk_sp<SkImageFilter> pass =
      skia::Effect::brightPass().resolvedImageFilter(nullptr);
  // A white source at half coverage IS white, and blooms as white at
  // half coverage. Read premultiplied it looks like a middling grey,
  // which a gate on the premultiplied colour would refuse — and every
  // antialiased edge in a layer is exactly this pixel.
  const std::vector<float> veiled = brightThrough(pass, {1, 1, 1, 0.5f});
  EXPECT_NEAR(texel(veiled, 32, 32)[3], 0.5f, 0.02f);
  EXPECT_NEAR(texel(veiled, 32, 32)[0], 0.5f, 0.02f);
}

TEST(SkiaEffect, TheBrightPassIsComparableByItsThresholdAndKnee) {
  EXPECT_TRUE(skia::Effect::brightPass() == skia::Effect::brightPass(0.68f));
  EXPECT_FALSE(skia::Effect::brightPass() == skia::Effect::brightPass(0.5f));
  EXPECT_FALSE(skia::Effect::brightPass(0.68f, 0.30f) ==
               skia::Effect::brightPass(0.68f, 0.10f));
  EXPECT_FALSE(skia::Effect::brightPass().isAnimated());
  // A knee that would run past one is cut there, so the two spellings of
  // "everything above the threshold" are one effect.
  EXPECT_TRUE(skia::Effect::brightPass(0.9f, 0.2f) ==
              skia::Effect::brightPass(0.9f, 4.0f));
  // The pass is a colour map, so a consumer hangs it on a paint rather
  // than running it as a pass of its own.
  EXPECT_EQ(skia::Effect::brightPass().imageFilter(), nullptr);
  EXPECT_NE(skia::Effect::brightPass().colorFilter(), nullptr);
}

TEST(SkiaEffect, TheLightStagesCompareByTheirParameters) {
  // deepen() and whiten() are colour maps described by their numbers, so
  // a re-described equal stage prunes where an already-built colour
  // filter compared by pointer could not.
  EXPECT_TRUE(skia::Effect::deepen(2) == skia::Effect::deepen(2));
  EXPECT_FALSE(skia::Effect::deepen(2) == skia::Effect::deepen(1));
  EXPECT_TRUE(skia::Effect::whiten(0.5f) == skia::Effect::whiten(0.5f));
  EXPECT_FALSE(skia::Effect::whiten(0.5f) == skia::Effect::whiten(0.5f, 0.4f));
  EXPECT_NE(skia::Effect::deepen(2).colorFilter(), nullptr);
  EXPECT_EQ(skia::Effect::deepen(2).imageFilter(), nullptr);
  // An amount at or below zero is no stage at all.
  EXPECT_TRUE(skia::Effect::deepen(0) == skia::Effect{});
}

TEST(SkiaEffect, OpticalBloomSpreadsColourBeyondTheSourceAndSoftensIt) {
  const auto glow = skia::bloom({.sigma = 3, .strength = 1,
                                 .spread = 5, .tail = 2});
  ASSERT_NE(glow.imageFilter(), nullptr);
  EXPECT_TRUE(glow == skia::Effect(glow));
  const auto pixels = bloomThrough(glow.imageFilter(), {1, 0, 0, 1});
  EXPECT_GT(texel(pixels, 8, 32)[0], 0);
  EXPECT_GT(texel(pixels, 16, 32)[0], texel(pixels, 8, 32)[0]);
  EXPECT_FLOAT_EQ(texel(pixels, 8, 32)[1], 0);
  EXPECT_FLOAT_EQ(texel(pixels, 32, 32)[0], 1);
  const auto soft = bloomThrough(
      skia::bloom({.strength = 0, .tail = 0, .softness = 2}).imageFilter(),
      {1, 0, 0, 1});
  EXPECT_GT(texel(soft, 19, 32)[0], 0);
  EXPECT_LT(texel(soft, 20, 32)[0], 1);
}

namespace {

std::vector<float> bloomHalo(skia::BloomParameters parameters,
                             SkColor4f colour) {
  return bloomThrough(skia::bloom(parameters).imageFilter(), colour);
}

float greenOver(const float* rgba, int channel) {
  return rgba[1] / rgba[channel];
}

float overRed(const float* rgba, int channel) {
  return rgba[channel] / rgba[0];
}

}  // namespace

TEST(SkiaEffect, OpticalBloomDeepeningSinksAFadingHaloTowardItsStrongestChannel) {
  const skia::BloomParameters broad{.sigma = 3, .strength = 0, .spread = 5,
                                    .tail = 1};
  auto deep = broad;
  deep.deepening = 2;
  // Orange deepens toward red with its red channel held.
  const SkColor4f orange{1, 0.58f, 0.09f, 1};
  const auto plainOrange = bloomHalo(broad, orange);
  const auto deepOrange = bloomHalo(deep, orange);
  EXPECT_LT(overRed(texel(deepOrange, 8, 32), 1),
            overRed(texel(plainOrange, 8, 32), 1));
  EXPECT_NEAR(texel(deepOrange, 8, 32)[0], texel(plainOrange, 8, 32)[0], 1e-3f);
  // A blue-leaning cyan deepens toward blue.
  const SkColor4f cyan{0.55f, 0.94f, 1, 1};
  const auto plainCyan = bloomHalo(broad, cyan);
  const auto deepCyan = bloomHalo(deep, cyan);
  EXPECT_LT(greenOver(texel(deepCyan, 8, 32), 2),
            greenOver(texel(plainCyan, 8, 32), 2));
  // Fainter light is deeper: farther out, less green is left beside red.
  EXPECT_LT(overRed(texel(deepOrange, 4, 32), 1),
            overRed(texel(deepOrange, 12, 32), 1));
  // A single channel has nothing weaker to lose.
  const auto red = bloomHalo(deep, {1, 0, 0, 1});
  EXPECT_NEAR(texel(red, 8, 32)[0], texel(bloomHalo(broad, {1, 0, 0, 1}), 8, 32)[0],
              1e-3f);
  EXPECT_FLOAT_EQ(texel(red, 8, 32)[1], 0);
}

TEST(SkiaEffect, OpticalBloomWhiteningLightensTheLitCoreOnly) {
  const skia::BloomParameters none{.strength = 0, .tail = 0};
  auto white = none;
  white.whitening = 0.5f;
  const SkColor4f orange{1, 0.58f, 0.09f, 1};
  const auto plain = bloomHalo(none, orange);
  const auto lit = bloomHalo(white, orange);
  EXPECT_FLOAT_EQ(texel(lit, 32, 32)[0], 1);
  EXPECT_NEAR(texel(lit, 32, 32)[1], 0.79f, 1e-2f);
  EXPECT_GT(texel(lit, 32, 32)[2], texel(plain, 32, 32)[2]);
  // Below the threshold the colour is the source's.
  const auto dim = bloomHalo(white, {0.1f, 0.05f, 0, 1});
  EXPECT_NEAR(texel(dim, 32, 32)[1], 0.05f, 1e-3f);
}

TEST(SkiaEffect, OpticalBloomDilationCarriesTheColourPastTheSource) {
  // The block's edge is at x = 20; two pixels out, a dilated glow keeps
  // most of the colour the plain one has only just outside the edge.
  const skia::BloomParameters near{.sigma = 1, .tail = 0, .maxOpacity = 1};
  auto grown = near;
  grown.dilation = 3;
  const auto plain = bloomHalo(near, {1, 0, 0, 1});
  const auto dilated = bloomHalo(grown, {1, 0, 0, 1});
  EXPECT_GT(texel(dilated, 18, 32)[0], 2 * texel(plain, 18, 32)[0]);
  EXPECT_GT(texel(dilated, 18, 32)[0], 0.5f);
  EXPECT_FLOAT_EQ(texel(dilated, 32, 32)[0], 1);
}

TEST(SkiaEffect, OpticalBloomIsTheCompositionOfItsStages) {
  const skia::BloomParameters p{.sigma = 2, .strength = 1.2f, .spread = 3,
                                .tail = 0.8f, .softness = 1,
                                .whitening = 0.3f, .dilation = 2,
                                .deepening = 1.5f, .maxOpacity = 0.7f};
  const auto gain = [](float alpha) {
    const float m[20] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                         0, 0, 1, 0, 0, 0, 0, 0, alpha, 0};
    return skia::Effect::filter(SkColorFilters::Matrix(m));
  };
  std::array<uint8_t, 256> ceiling{};
  for (int i = 0; i < 256; ++i)
    ceiling[i] = static_cast<uint8_t>(std::min(i, int(0.7f * 255)));
  const auto light = skia::Effect::brightPass(p.threshold, p.knee)
                         .then(skia::Effect::dilate(p.dilation));
  const auto rung = [&](float sigma, float strength) {
    return light.then(skia::Effect::blur(sigma))
        .then(skia::Effect::deepen(p.deepening))
        .then(gain(strength));
  };
  const auto halo =
      rung(2, 1.2f)
          .emit(rung(6, 0.8f), SkBlendMode::kPlus)
          .then(skia::Effect::filter(SkColorFilters::TableARGB(
              ceiling.data(), nullptr, nullptr, nullptr)));
  const auto composed = skia::Effect::blur(1)
                            .then(skia::Effect::whiten(0.3f, p.threshold, p.knee))
                            .emit(halo);
  const SkColor4f amber{1, 0.58f, 0.09f, 1};
  const auto want = bloomThrough(composed.imageFilter(), amber);
  const auto got = bloomThrough(skia::bloom(p).imageFilter(), amber);
  ASSERT_EQ(want.size(), got.size());
  for (size_t i = 0; i < want.size(); ++i) ASSERT_EQ(want[i], got[i]) << i;
}

TEST(SkiaEffect, DilateGrowsEdgesByItsDistanceWithRoundCorners) {
  // The layer's ground is opaque black, so the spread is given the light
  // alone: the bright pass carries brightness as coverage.
  const auto grown = bloomThrough(
      skia::Effect::brightPass(0.2f, 0.2f).then(skia::Effect::dilate(3))
          .imageFilter(),
      {1, 0, 0, 1});
  // The block spans 20 to 44. Three pixels out from a side, most of the
  // coverage is back; the same distance out from a corner along the
  // diagonal is farther from the block, so less is.
  EXPECT_GT(texel(grown, 17, 32)[0], 0.5f);
  EXPECT_LT(texel(grown, 17, 17)[0], texel(grown, 17, 32)[0]);
  EXPECT_FLOAT_EQ(texel(grown, 32, 32)[0], 1);
  EXPECT_EQ(skia::Effect::dilate(0).imageFilter(), nullptr);
}

TEST(SkiaEffect, EmitStacksLightsOfTheLayerAndKeepsItWhereTheyAreDark) {
  const auto dim = [](float alpha) {
    const float m[20] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                         0, 0, 1, 0, 0, 0, 0, 0, alpha, 0};
    return skia::Effect::filter(SkColorFilters::Matrix(m));
  };
  const auto near = skia::Effect::blur(2).then(dim(0.3f));
  const auto wide = skia::Effect::blur(6).then(dim(0.3f));
  const SkColor4f red{1, 0, 0, 1};
  const auto layer = bloomThrough(nullptr, red);
  const auto withNear =
      bloomThrough(skia::Effect().emit(near, SkBlendMode::kPlus).imageFilter(), red);
  const auto withWide =
      bloomThrough(skia::Effect().emit(wide, SkBlendMode::kPlus).imageFilter(), red);
  const auto both = bloomThrough(skia::Effect()
                                     .emit(near, SkBlendMode::kPlus)
                                     .emit(wide, SkBlendMode::kPlus)
                                     .imageFilter(),
                                 red);
  // Outside the block the layer is black, so each light adds alone and
  // the second reads the layer, not the first light.
  for (int x : {12, 15, 18})
    EXPECT_NEAR(texel(both, x, 32)[0],
                texel(withNear, x, 32)[0] + texel(withWide, x, 32)[0], 2e-3f)
        << x;
  // Where the light is transparent the layer comes through unchanged.
  const auto unlit =
      bloomThrough(skia::Effect().emit(dim(0)).imageFilter(), red);
  ASSERT_EQ(unlit.size(), layer.size());
  for (size_t i = 0; i < layer.size(); ++i) ASSERT_EQ(unlit[i], layer[i]) << i;
}

TEST(SkiaEffect, EmitOfStaticSidesComparesByItsFilter) {
  // Static sides blend once into one filter, which compares by identity.
  const auto lit = skia::Effect().emit(skia::Effect::blur(2));
  EXPECT_TRUE(lit == lit);
  EXPECT_FALSE(lit == skia::Effect().emit(skia::Effect::blur(2)));
}
