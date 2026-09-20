/** @file
 * THE PHOSPHOR BLOOM: the spectral post-process a tube's glow is made
 * of, compared by its parameters and then painted out against the same
 * falloff written by hand — the three kernels with no hue drift and no
 * tail, to the bit — and the hue drift turning the halo while leaving
 * the source where it was.
 */

#include <gtest/gtest.h>
#include <include/core/SkColor.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Effect.h>

#include <cmath>
#include <memory>
#include <vector>

#include "support/EffectRead.h"

using namespace sigil::material;
using sigil::material::test::bloomThrough;
using sigil::material::test::texel;

TEST(SkiaEffect, PhosphorBloomIsAComparableSpectralPostProcess) {
  const skia::Effect bloom =
      skia::Effect::phosphorBloom(8.0f, 0.6f, 0.4f, 0.75f);
  EXPECT_NE(bloom.resolvedImageFilter(nullptr), nullptr);
  EXPECT_FALSE(bloom.isAnimated());
  EXPECT_TRUE(bloom == skia::Effect::phosphorBloom(8.0f, 0.6f, 0.4f, 0.75f));
  EXPECT_FALSE(bloom == skia::Effect::phosphorBloom(10.0f, 0.6f, 0.4f, 0.75f));
}

namespace {

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
