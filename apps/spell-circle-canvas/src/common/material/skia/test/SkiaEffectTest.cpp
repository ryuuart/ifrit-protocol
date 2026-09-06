/** @file
 * The post-processing value: what a filter's identity is, what makes an
 * effect live, how a chain composes, and the two blurs and the phosphor
 * bloom painted out and compared against the arithmetic they promise.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace sigil::material;

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

TEST(SkiaEffect, ChainingKeepsTheNodesAContextNeedingChildLivesIn) {
  // Precomposing two static sides into one filter is what makes a chain
  // cost nothing per paint — but a child that needs the paint context is
  // not static: a sigma map reading uResolution, an image fitted to the
  // box. Frozen into the null-context snapshot it would paint the box it
  // was first described in for ever, and the composed effect would answer
  // usesWorldSpace() with false because the children are gone.
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "uniform shader tint;\n"
               "half4 main(float2 p) { return content.eval(p) * "
               "tint.eval(p); }"));
  ASSERT_NE(effect, nullptr);
  skia::Paint anchored = skia::Paint::solid({1, 0, 0, 1});
  anchored.worldSpace();
  EXPECT_TRUE(anchored.geometryDependent());

  skia::Effect shaded = skia::Effect::shader(effect);
  shaded.child("tint", anchored);
  EXPECT_FALSE(shaded.isAnimated());
  EXPECT_TRUE(shaded.usesWorldSpace());

  const skia::Effect chained = shaded.then(skia::Effect::glow({0, 1, 1, 1}, 4));
  EXPECT_TRUE(chained.usesWorldSpace());
  EXPECT_NE(chained.resolvedImageFilter(nullptr), nullptr);
  // …and the other way round, since either side may hold the child.
  EXPECT_TRUE(
      skia::Effect::glow({0, 1, 1, 1}, 4).then(shaded).usesWorldSpace());
}

TEST(SkiaEffect, SettingOneUniformTwiceReplacesItRatherThanStacking) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;\n"
               "uniform float uK;\n"
               "half4 main(float2 p) { return content.eval(p) * half(uK); }"));
  ASSERT_NE(effect, nullptr);
  skia::Effect twice = skia::Effect::shader(effect);
  twice.uniform("uK", 0.25f);
  twice.uniform("uK", 0.75f);
  // Last write wins, as child() does: the same effect described once at
  // the final value is the same recipe, so a re-described node prunes.
  skia::Effect once = skia::Effect::shader(effect);
  once.uniform("uK", 0.75f);
  EXPECT_TRUE(twice == once);
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

TEST(SkiaEffect, AParameterBlurReachesItsOwnBoxAndNotTheClip) {
  // A runtime shader may write any pixel, so Skia treats a filter built
  // from one as covering the whole clip and hands it a clip-sized layer.
  // A parameter blur declares its reach instead: the box the map is
  // defined over, grown by the support of the largest level. Asked what
  // it touches under two clips of different sizes, the resolved filter
  // answers the same rectangle — the box and its reach — rather than
  // either clip.
  // A bound sigma is what makes the resolve build against the frame; an
  // unbound blur answers its store-time snapshot, which knows no box.
  skia::PaintFrame frame;
  frame.size = SkSize::Make(120, 120);
  choreograph::Output<float> sigma(5.0f);
  skia::Effect blur =
      skia::Effect::blur(skia::Paint::solid({1, 1, 1, 1}), 8.0f);
  blur.uniform("maxSigma", &sigma);
  const sk_sp<SkImageFilter> filter = blur.resolvedImageFilter(&frame);
  ASSERT_NE(filter, nullptr);
  const SkIRect small =
      filter->filterBounds(SkIRect::MakeWH(300, 300), SkMatrix::I(),
                           SkImageFilter::kForward_MapDirection, nullptr);
  const SkIRect large =
      filter->filterBounds(SkIRect::MakeWH(1200, 1200), SkMatrix::I(),
                           SkImageFilter::kForward_MapDirection, nullptr);
  EXPECT_EQ(small, large) << "the reach depends on the clip";
  EXPECT_NE(large, SkIRect::MakeWH(1200, 1200)) << "the reach is the clip";
  const SkIRect declared =
      SkRect::MakeWH(120, 120).makeOutset(24, 24).roundOut();
  EXPECT_EQ(large, declared);
}

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

namespace {

/** The plain three-kernel program at @p radius, as one effect. */
skia::Effect plainPhosphor(const sk_sp<SkRuntimeEffect>& program,
                           float radius) {
  return skia::Effect::shader(program, {{"uRadius", radius},
                                        {"uThreshold", 0.52f},
                                        {"uIntensity", 0.46f},
                                        {"uChroma", 0.80f}});
}

}  // namespace

TEST(SkiaEffect, PhosphorBloomIsThePlainFalloffWithinTheResample) {
  // The hue drift and the tail default to zero, and zero is the falloff
  // without them — so the recipe is held to the plain three-kernel
  // program written out in full above. It meets it EXACTLY wherever the
  // halo is gathered at the layer's own resolution, and within a stated
  // tolerance where the gather is reduced, which is what buys the speed.
  auto [plain, error] =
      SkRuntimeEffect::MakeForShader(SkString(kPlainPhosphor));
  ASSERT_NE(plain, nullptr) << error.c_str();
  const SkColor4f amber{1.0f, 0.72f, 0.1f, 1.0f};

  // A REACH TOO SMALL TO REDUCE is gathered whole, and then the split
  // into a halo pass and a composite pass changes nothing at all: every
  // float of the picture is the plain program's own.
  {
    const std::vector<float> want = bloomThrough(
        plainPhosphor(plain, 6.0f).resolvedImageFilter(nullptr), amber);
    const std::vector<float> got =
        bloomThrough(skia::Effect::phosphorBloom(6.0f, 0.52f, 0.46f, 0.80f)
                         .resolvedImageFilter(nullptr),
                     amber);
    ASSERT_EQ(want.size(), got.size());
    for (size_t i = 0; i < want.size(); ++i)
      ASSERT_EQ(want[i], got[i]) << "float " << i;
  }

  // AT THE DEFAULTS the halo is gathered over a halved layer and resampled
  // back, so the picture is the plain program's blurred by that round
  // trip. The bound is on the halo's own scale — a channel runs 0..1
  // here — and it is a tolerance about a RESAMPLE, so it is spent at the
  // steep edge of the falloff and almost nowhere else: the mean error
  // over the whole picture stays an order of magnitude under the worst
  // texel's.
  const std::vector<float> want = bloomThrough(
      plainPhosphor(plain, 9.0f).resolvedImageFilter(nullptr), amber);
  const std::vector<float> got = bloomThrough(
      skia::Effect::phosphorBloom().resolvedImageFilter(nullptr), amber);
  ASSERT_EQ(want.size(), got.size());
  double sum = 0, worst = 0;
  for (size_t i = 0; i < want.size(); ++i) {
    const double difference = std::abs((double)want[i] - (double)got[i]);
    sum += difference;
    worst = std::max(worst, difference);
  }
  EXPECT_LT(worst, 0.05) << "worst texel of the resampled halo";
  EXPECT_LT(sum / (double)want.size(), 0.003) << "mean over the picture";

  // COVERAGE IS NOT RESAMPLED. The composite reads the source at full
  // resolution and returns its alpha untouched, so a halo adds light and
  // never opacity — every alpha is the plain program's to the bit.
  for (size_t i = 3; i < want.size(); i += 4)
    ASSERT_EQ(want[i], got[i]) << "alpha " << i / 4;
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
