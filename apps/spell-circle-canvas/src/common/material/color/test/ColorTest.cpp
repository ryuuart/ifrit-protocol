/** @file
 * The colour feature: the sRGB curve round-trips and clamps, so does
 * OKLab, its midpoint is perceptual, the hue wheel folds whatever it is
 * handed, and a four-float colour crosses in by shape.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/color/Color.h>

#include <algorithm>

using namespace sigil::material;

TEST(Color, TheTransferFunctionRoundTripsAndClampsAtWhite) {
  for (float v : {0.0f, 0.001f, 0.02f, 0.04045f, 0.25f, 0.5f, 0.75f, 1.0f})
    EXPECT_NEAR(linearToSrgb(srgbToLinear(v)), v, 1e-5f) << v;
  EXPECT_FLOAT_EQ(srgbToLinear(1.0f), 1.0f);
  // Mid-grey is a fifth of the light, which is the whole reason a blend
  // that lerps encoded channels is the wrong blend.
  EXPECT_NEAR(srgbToLinear(0.5f), 0.2140f, 1e-3f);
  // Past white there is no more light to encode.
  EXPECT_FLOAT_EQ(linearToSrgb(2.0f), 1.0f);
}

TEST(Color, TheOklabRoundTripIsTheColourItStartedFrom) {
  const Color mid{0.5f, 0.5f, 0.5f, 1};
  const Color back = fromOklab(toOklab(mid));
  EXPECT_NEAR(back.r, 0.5f, 1e-4f);
  EXPECT_NEAR(back.g, 0.5f, 1e-4f);
  // Lightness runs from black at zero to white at one, so the two ends
  // of the axis are the two colours a caller names them by.
  EXPECT_NEAR(toOklab({1, 1, 1, 1}).L, 1.0f, 1e-3f);
  EXPECT_NEAR(toOklab({0, 0, 0, 1}).L, 0.0f, 1e-6f);
  const Color a{0.9f, 0.1f, 0.2f, 1.0f}, b{0.1f, 0.3f, 0.8f, 0.5f};
  const Color start = lerpOklab(a, b, 0.0f), end = lerpOklab(a, b, 1.0f);
  EXPECT_NEAR(start.r, a.r, 1e-4f);
  EXPECT_NEAR(start.a, a.a, 1e-6f);
  EXPECT_NEAR(end.b, b.b, 1e-4f);
  EXPECT_NEAR(end.a, b.a, 1e-6f);
}

TEST(Color, HsvWalksTheWheelAndFoldsWhateverItIsGiven) {
  // The six landmarks, exactly.
  const Color red = hsv(0, 1, 1);
  EXPECT_FLOAT_EQ(red.r, 1.0f);
  EXPECT_FLOAT_EQ(red.g, 0.0f);
  EXPECT_FLOAT_EQ(red.b, 0.0f);
  EXPECT_FLOAT_EQ(hsv(120, 1, 1).g, 1.0f);
  EXPECT_FLOAT_EQ(hsv(240, 1, 1).b, 1.0f);
  EXPECT_FLOAT_EQ(hsv(300, 1, 1).r, 1.0f);
  EXPECT_FLOAT_EQ(hsv(300, 1, 1).b, 1.0f);
  EXPECT_FLOAT_EQ(hsv(300, 1, 1).g, 0.0f);

  // The fold is the reason this is a library verb: the sextant ladder
  // answers magenta for anything it does not recognise, so an unwrapped
  // hue — a golden-angle walk, an angle in degrees — is silently wrong
  // everywhere past a full turn and everywhere below zero.
  for (float turn : {-720.0f, -360.0f, 360.0f, 1080.0f}) {
    const Color same = hsv(150.0f + turn, 0.7f, 0.6f);
    const Color once = hsv(150.0f, 0.7f, 0.6f);
    EXPECT_NEAR(same.r, once.r, 1e-5f) << "turn " << turn;
    EXPECT_NEAR(same.g, once.g, 1e-5f) << "turn " << turn;
    EXPECT_NEAR(same.b, once.b, 1e-5f) << "turn " << turn;
  }

  // Value is the largest channel, saturation the distance from grey, and
  // both are clamped — outside the unit range the ladder answers a colour
  // and it is the wrong one.
  const Color grey = hsv(200, 0, 0.4f);
  EXPECT_FLOAT_EQ(grey.r, 0.4f);
  EXPECT_FLOAT_EQ(grey.g, 0.4f);
  EXPECT_FLOAT_EQ(grey.b, 0.4f);
  EXPECT_FLOAT_EQ(hsv(200, 5.0f, 2.0f).b, 1.0f);
  EXPECT_FLOAT_EQ(hsv(200, -1.0f, -1.0f).r, 0.0f);
  const Color tone = hsv(30, 0.5f, 0.8f);
  EXPECT_FLOAT_EQ(std::max({tone.r, tone.g, tone.b}), 0.8f);
  EXPECT_FLOAT_EQ(std::min({tone.r, tone.g, tone.b}), 0.8f * 0.5f);

  // Alpha rides through, straight, like every other colour here.
  EXPECT_FLOAT_EQ(hsv(10, 1, 1, 0.25f).a, 0.25f);
}

TEST(Color, TakesAFourFloatColourFieldForFieldWithoutNamingItsLibrary) {
  // The shape of a Skia colour, declared here so this test names no
  // renderer either — which is the whole point of matching by shape: the
  // leaf every params struct includes must not include one.
  struct FourFloats {
    float fR, fG, fB, fA;
  };
  static_assert(FourFloatColor<FourFloats>);
  static_assert(!FourFloatColor<Color>, "this library names its own channels");
  // Same order, straight alpha, and no clamp: a channel above 1 survives,
  // because a clamp here would silently change a colour that was correct
  // for a wide gamut.
  const Color crossed = FourFloats{1.4f, 0.25f, 0.0f, 0.5f};
  EXPECT_FLOAT_EQ(crossed.r, 1.4f);
  EXPECT_FLOAT_EQ(crossed.g, 0.25f);
  EXPECT_FLOAT_EQ(crossed.b, 0.0f);
  EXPECT_FLOAT_EQ(crossed.a, 0.5f);
  // Implicit: it crosses at an argument and at a field, which is the
  // reason it is a conversion rather than a named call.
  const auto takesAColour = [](Color c) { return c.g; };
  EXPECT_FLOAT_EQ(takesAColour(FourFloats{0, 1, 0, 1}), 1.0f);
}

TEST(Color, TheOklabMidpointBetweenBlackAndWhiteIsPerceptual) {
  // OKLab L is cube-root lightness, so its black-to-white midpoint is
  // linear luminance 0.125 — sRGB about 0.389 — well below a naive sRGB
  // lerp's 0.5 and far below a linear-light lerp's 0.735. Anything that
  // interpolates two colours through this leaf inherits that, which is the
  // whole reason a blend does not lerp channels.
  const Color mid = lerpOklab({0, 0, 0, 1}, {1, 1, 1, 1}, 0.5f);
  EXPECT_NEAR(mid.r, 0.389f, 0.03f);
  EXPECT_NEAR(mid.r, mid.g, 0.01f);
  EXPECT_NEAR(mid.g, mid.b, 0.01f);
  // The ends are the keys themselves, so a blend's first and last step are
  // the colours the caller named.
  EXPECT_NEAR(lerpOklab({1, 0, 0, 1}, {0, 0, 1, 1}, 0.0f).r, 1.0f, 1e-3f);
  EXPECT_NEAR(lerpOklab({1, 0, 0, 1}, {0, 0, 1, 1}, 1.0f).b, 1.0f, 1e-3f);
}

TEST(Color, MixingInLinearLightIsADifferentAnswerFromMixingTheCodeValues) {
  const Color black{0, 0, 0, 1}, white{1, 1, 1, 1};
  // Half the light is not half the code value, and the gap is the whole
  // reason the two verbs are named apart: a mix of quantities that walked
  // the numbers a file stores comes out a fifth as bright as it claims.
  const Color light = mixLinear(black, white, 0.5f);
  const Color codes = mixToward(black, white, 0.5f, 1.0f);
  EXPECT_NEAR(luminance(light), 0.5f, 1e-3f);
  EXPECT_NEAR(luminance(codes), 0.214f, 1e-3f);
  EXPECT_GT(light.r, codes.r);
  EXPECT_NEAR(light.r, 0.7354f, 1e-3f);  // near #BC, not #80

  // The ends are the ends, and alpha rides along without the curve.
  EXPECT_NEAR(mixLinear(black, white, 0.0f).r, 0.0f, 1e-5f);
  EXPECT_NEAR(mixLinear(black, white, 1.0f).r, 1.0f, 1e-5f);
  const Color clear{1, 0, 0, 0};
  EXPECT_NEAR(mixLinear(clear, white, 0.25f).a, 0.25f, 1e-5f);

  // Luminance is what the primaries weigh, not what the channels count:
  // full green carries three times full red's light.
  EXPECT_NEAR(luminance(white), 1.0f, 1e-4f);
  EXPECT_NEAR(luminance(Color{0, 1, 0, 1}), 0.7152f, 1e-4f);
  EXPECT_NEAR(luminance(Color{1, 0, 0, 1}), 0.2126f, 1e-4f);
}

TEST(Color, CielabMeasuresWhereOklabInterpolates) {
  // The round trip is the colour it started from.
  for (const Color& c : {Color{0.2f, 0.6f, 0.9f, 1}, Color{0.9f, 0.1f, 0.3f, 0.5f},
                         Color{0, 0, 0, 1}, Color{1, 1, 1, 1}}) {
    const Color back = fromLab(toLab(c));
    EXPECT_NEAR(back.r, c.r, 2e-3f);
    EXPECT_NEAR(back.g, c.g, 2e-3f);
    EXPECT_NEAR(back.b, c.b, 2e-3f);
    EXPECT_NEAR(back.a, c.a, 1e-5f);
  }

  // The scale is the published one: black is 0, white is 100, and a
  // neutral has no chroma on either axis.
  EXPECT_NEAR(toLab(Color{0, 0, 0, 1}).L, 0.0f, 1e-3f);
  EXPECT_NEAR(toLab(Color{1, 1, 1, 1}).L, 100.0f, 1e-2f);
  const Lab grey = toLab(Color{0.5f, 0.5f, 0.5f, 1});
  EXPECT_NEAR(grey.a, 0.0f, 1e-2f);
  EXPECT_NEAR(grey.b, 0.0f, 1e-2f);

  // A difference is quoted in this space, and a colour indistinguishable
  // from another is a small number rather than a matching pair of codes.
  EXPECT_NEAR(deltaE(Color{0.5f, 0.5f, 0.5f, 1}, Color{0.5f, 0.5f, 0.5f, 1}),
              0.0f, 1e-4f);
  EXPECT_LT(deltaE(Color{0.5f, 0.5f, 0.5f, 1}, Color{0.503f, 0.5f, 0.5f, 1}),
            2.3f);
  EXPECT_GT(deltaE(Color{0, 0, 0, 1}, Color{1, 1, 1, 1}), 99.0f);
}

TEST(Color, ARampReadsOnTheCpuTheWayAGradientDraws) {
  const RampStop stops[] = {{0.0f, Color{0, 0, 0, 1}},
                            {0.5f, Color{1, 0, 0, 1}},
                            {1.0f, Color{1, 1, 1, 1}}};
  EXPECT_EQ(sampleRamp(stops, 0.5f).r, 1.0f);
  EXPECT_NEAR(sampleRamp(stops, 0.25f).r, 0.5f, 1e-5f);
  EXPECT_NEAR(sampleRamp(stops, 0.75f).g, 0.5f, 1e-5f);

  // Outside the ramp is the end stop's flat colour, never an
  // extrapolation into a channel the ramp never named.
  EXPECT_EQ(sampleRamp(stops, -5.0f), stops[0].color);
  EXPECT_EQ(sampleRamp(stops, 5.0f), stops[2].color);

  // Two stops at one position are a hard edge, which is how a ramp says
  // a band boundary.
  const RampStop banded[] = {{0.0f, Color{1, 0, 0, 1}},
                             {0.5f, Color{1, 0, 0, 1}},
                             {0.5f, Color{0, 0, 1, 1}},
                             {1.0f, Color{0, 0, 1, 1}}};
  EXPECT_EQ(sampleRamp(banded, 0.49f), (Color{1, 0, 0, 1}));
  EXPECT_EQ(sampleRamp(banded, 0.51f), (Color{0, 0, 1, 1}));

  EXPECT_EQ(sampleRamp({}, 0.5f), (Color{0, 0, 0, 0}));
}

TEST(Color, APaletteIsReadExactlyWhereARampIsReadBetween) {
  const Palette pal{{Color{1, 0, 0, 1}, Color{0, 1, 0, 1}, Color{0, 0, 1, 1},
                     Color{1, 1, 1, 0.5f}}};
  EXPECT_EQ(pal.size(), 4u);
  EXPECT_EQ(pal.at(0), (Color{1, 0, 0, 1}));
  EXPECT_EQ(pal.at(2), (Color{0, 0, 1, 1}));
  EXPECT_EQ(pal.at(3).a, 0.5f);  // an entry's own alpha is the entry's

  // Out of range clamps rather than wrapping: a mistake upstream shows as
  // a flat band at the end of the table, not as a plausible colour from
  // the other end of it.
  EXPECT_EQ(pal.at(-3), pal.at(0));
  EXPECT_EQ(pal.at(99), pal.at(3));

  // A unit position falls IN a band and answers that entry whole. Nothing
  // here ever returns a colour the table does not contain, which is the
  // one thing a fixed palette exists for.
  EXPECT_EQ(pal.nearest(0.0f), pal.at(0));
  EXPECT_EQ(pal.nearest(0.2f), pal.at(0));
  EXPECT_EQ(pal.nearest(0.26f), pal.at(1));
  EXPECT_EQ(pal.nearest(0.99f), pal.at(3));
  EXPECT_EQ(pal.nearest(1.0f), pal.at(3));
  EXPECT_EQ(pal.nearest(-1.0f), pal.at(0));

  // An empty table has one honest answer and gives it rather than reading
  // past its own end.
  const Palette none;
  EXPECT_TRUE(none.empty());
  EXPECT_EQ(none.at(0), (Color{0, 0, 0, 0}));
  EXPECT_EQ(none.nearest(0.5f), (Color{0, 0, 0, 0}));
}
