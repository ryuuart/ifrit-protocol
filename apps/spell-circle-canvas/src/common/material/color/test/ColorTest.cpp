/** @file
 * The colour feature: the sRGB curve round-trips and clamps, so does
 * OKLab, its midpoint is perceptual, the hue wheel folds whatever it is
 * handed, a four-float colour crosses in by shape, and a colour's CSS
 * text reads in every notation CSS Color 4 gives it.
 */

#include <gtest/gtest.h>
#include <sigilcore/report/Report.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Mix.h>

#include <algorithm>
#include <string>
#include <vector>

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
  // leaf every parameter struct includes must not include one.
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

  // Two equal channels stand for one quantity of light: the mix is that
  // channel exactly, with no round trip through the curve to round it.
  const Color grey{0.25f, 0.25f, 0.25f, 1};
  EXPECT_EQ(mixLinear(grey, Color{0.25f, 0.25f, 0.25f, 0}, 0.5f).r, 0.25f);
  EXPECT_FLOAT_EQ(mixLinear(grey, Color{0.25f, 0.25f, 0.25f, 0}, 0.5f).a, 0.5f);

  // Luminance is what the primaries weigh, not what the channels count:
  // full green carries three times full red's light.
  EXPECT_NEAR(luminance(white), 1.0f, 1e-4f);
  EXPECT_NEAR(luminance(Color{0, 1, 0, 1}), 0.7152f, 1e-4f);
  EXPECT_NEAR(luminance(Color{1, 0, 0, 1}), 0.2126f, 1e-4f);
}

TEST(Color, TheArithmeticVerbsAreConstexprAndSayWhichChannelsTheyTouch) {
  // A palette is a list of constants, so every verb an authored constant
  // is written through has to fold at compile time.
  constexpr Color rubric = hexColor(0x8C2F22);
  static_assert(hexColor(0xFFFFFF).r == 1.0f);
  static_assert(withAlpha(rubric, 0.4f).a == 0.4f);
  static_assert(withAlpha(rubric, 0.4f).r == rubric.r,
                "withAlpha touches the alpha and nothing else");
  static_assert(scale(rubric, 1.5f).a == rubric.a,
                "a negative alpha KEEPS the source's");
  static_assert(scale(rubric, 0.5f, 0.2f).a == 0.2f);
  static_assert(lighten(Color{0.9f, 0.9f, 0.9f, 1}, 0.5f).r == 1.0f,
                "the offset saturates at white where the scale has no "
                "ceiling");

  EXPECT_FLOAT_EQ(rubric.r, 0x8C / 255.0f);
  EXPECT_FLOAT_EQ(rubric.g, 0x2F / 255.0f);
  EXPECT_FLOAT_EQ(rubric.b, 0x22 / 255.0f);
  EXPECT_FLOAT_EQ(scale(rubric, 1.5f).r, rubric.r * 1.5f);
  // Deliberately unclamped: a float channel above 1 is meaningful.
  EXPECT_GT(scale(Color{0.9f, 0.9f, 0.9f, 1}, 2.0f).r, 1.0f);
  EXPECT_FLOAT_EQ(lighten(rubric, 0.1f).a, rubric.a);
}

TEST(Color, CielabMeasuresWhereOklabInterpolates) {
  // The round trip is the colour it started from.
  for (const Color& c :
       {Color{0.2f, 0.6f, 0.9f, 1}, Color{0.9f, 0.1f, 0.3f, 0.5f},
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
  const ColorStop stops[] = {{0.0f, Color{0, 0, 0, 1}},
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
  const ColorStop banded[] = {{0.0f, Color{1, 0, 0, 1}},
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

// A colour's text reads to the colour and alpha its digits say, whichever
// of the four hex lengths spells it, and a named colour reads to its CSS
// value whatever its case.
TEST(Color, ACssColourReadsToTheColourItsDigitsSay) {
  const Color six = parseColor("#e3d7b6");
  EXPECT_EQ(six, hexColor(0xe3d7b6));
  EXPECT_EQ(parseColor("#e3d7b6ff"), six);
  EXPECT_EQ(parseColor("#E3D7B6"), six);
  EXPECT_EQ(parseColor("  #e3d7b6\n"), six);
  EXPECT_EQ(parseColor("#ed7"), hexColor(0xeedd77));
  EXPECT_EQ(parseColor("#ed78"), hexColor(0xeedd77, (float)0x88 / 255.0f));
  EXPECT_EQ(parseColor("#3a3125b8"), hexColor(0x3a3125, (float)0xb8 / 255.0f));

  EXPECT_EQ(parseColor("steelblue"), hexColor(0x4682b4));
  EXPECT_EQ(parseColor("Steelblue"), hexColor(0x4682b4));
  EXPECT_EQ(parseColor("grey"), hexColor(0x808080));
  EXPECT_EQ(parseColor("rebeccapurple"), hexColor(0x663399));
  EXPECT_EQ(parseColor("LightGoldenrodYellow"), hexColor(0xfafad2));
  EXPECT_EQ(parseColor("transparent"), (Color{0, 0, 0, 0}));
  EXPECT_EQ(namedColor("peru"), hexColor(0xcd853f));
  EXPECT_FALSE(namedColor("transparent"));
}

// A name is read whatever its case and wherever it stands in CSS's list;
// a word that is only part of a name, or longer than any, names none.
TEST(Color, ANameIsReadWhateverItsCaseAndPlace) {
  EXPECT_EQ(namedColor("aliceblue"), hexColor(0xf0f8ff));
  EXPECT_EQ(namedColor("yellowgreen"), hexColor(0x9acd32));
  EXPECT_EQ(namedColor("YellowGreen"), hexColor(0x9acd32));
  EXPECT_EQ(namedColor("lightgoldenrodyellow"), hexColor(0xfafad2));
  EXPECT_EQ(namedColor("aqua"), hexColor(0x00ffff));
  EXPECT_EQ(namedColor("aquamarine"), hexColor(0x7fffd4));
  EXPECT_FALSE(namedColor("aqu"));
  EXPECT_FALSE(namedColor("lightgoldenrodyellows"));
  EXPECT_FALSE(namedColor(""));
  EXPECT_FALSE(namedColor("steel blue"));
}

// Text no CSS colour grammar reads is nothing to the strict reading, and
// opaque black to the forgiving one, which says so once per text.
TEST(Color, TextNoGrammarReadsIsNothingAndSaidOnce) {
  for (const char* unreadable : {"",
                                 "#",
                                 "#12",
                                 "#12345",
                                 "#1234567",
                                 "#e3d7bg",
                                 "e3d7b6",
                                 "notacolour",
                                 "currentcolor",
                                 "var(--accent)",
                                 "rgb(1 2)",
                                 "rgb(1, 2 3)",
                                 "rgb(1 2 3 /)",
                                 "rgb(1 2 3 / 4 / 5)",
                                 "rgb(none, 0, 0)",
                                 "hsl(120, 50, 50)",
                                 "lab(50, 0, 0)",
                                 "hwb(0, 0%, 0%)",
                                 "rgb(10deg 0 0)",
                                 "rgb(1 2 3",
                                 "color(lab 1 2 3)",
                                 "color(srgb 1 0)",
                                 "color-mix(in srgb, red)",
                                 "color-mix(in srgb, red 120%, blue)",
                                 "color-mix(srgb, red, blue)",
                                 "color-mix(in srgb longer hue, red, blue)"})
    EXPECT_FALSE(readColor(unreadable)) << unreadable;

  std::vector<std::string> heard;
  const sigil::core::Reporter previous = sigil::core::installReporter(
      [&heard](std::string_view, std::string_view message) {
        heard.emplace_back(message);
      });
  EXPECT_EQ(parseColor("not a colour, twice"), (Color{0, 0, 0, 1}));
  EXPECT_EQ(parseColor("not a colour, twice"), (Color{0, 0, 0, 1}));
  sigil::core::installReporter(previous);
  ASSERT_EQ(heard.size(), 1u);
  EXPECT_NE(heard.front().find("not a colour, twice"), std::string::npos);
}

void expectNear(const Color& actual, const Color& expected, float tolerance,
                const char* written) {
  EXPECT_NEAR(actual.r, expected.r, tolerance) << written;
  EXPECT_NEAR(actual.g, expected.g, tolerance) << written;
  EXPECT_NEAR(actual.b, expected.b, tolerance) << written;
  EXPECT_NEAR(actual.a, expected.a, tolerance) << written;
}

// Every RGB spelling CSS has, legacy commas and modern spaces, numbers
// and percentages, reads to one colour; channels clamp and `none` is zero.
TEST(Color, EveryRgbSpellingReadsToOneColour) {
  const Color red{1, 0, 0, 1};
  for (const char* written :
       {"rgb(255 0 0)", "rgb(255, 0, 0)", "rgba(255,0,0,1)", "RGB(255 0 0)",
        "rgb(100% 0% 0%)", "rgb(100%, 0%, 0%)", "#f00", "red", "rgb(300 -5 0)",
        "rgb(255 none none)", "rgb(255 0 0 / 1)", "rgb(255 0 0/100%)",
        "rgb(2.55e2 0 0)"})
    EXPECT_EQ(readColor(written), red) << written;
  // Divided as the hex digits are, so the two spellings are one colour.
  EXPECT_EQ(readColor("rgb(110 153 187)"), hexColor(0x6e99bb));
  EXPECT_EQ(rgb(110, 153, 187), hexColor(0x6e99bb));
  const Color translucent = rgb(29, 164, 192, 0.95f);
  for (const char* written :
       {"rgb(29 164 192 / 95%)", "rgba(29, 164, 192, 0.95)",
        "rgb(29 164 192 / 0.95)", "rgba(29 164 192 / 95%)"})
    expectNear(*readColor(written), translucent, 1e-6f, written);
  EXPECT_EQ(rgb(300, -5, 0, 2), red);
}

// hsl() and hwb() read their CSS forms, hue units included, and the
// builders take CSS's 0..100 for every fraction.
TEST(Color, HslAndHwbReadAsCssDefinesThem) {
  EXPECT_EQ(readColor("hsl(0 100% 50%)"), (Color{1, 0, 0, 1}));
  EXPECT_EQ(readColor("hsl(120, 100%, 50%)"), (Color{0, 1, 0, 1}));
  EXPECT_EQ(readColor("hsl(120 100 50)"), (Color{0, 1, 0, 1}));
  EXPECT_EQ(readColor("hsla(240, 100%, 50%, 0.5)"), (Color{0, 0, 1, 0.5f}));
  expectNear(*readColor("hsl(0.5turn 100% 50%)"), {0, 1, 1, 1}, 1e-6f, "turn");
  expectNear(*readColor("hsl(200grad 100% 50%)"), {0, 1, 1, 1}, 1e-6f, "grad");
  expectNear(*readColor("hsl(3.14159265rad 100% 50%)"), {0, 1, 1, 1}, 1e-6f,
             "rad");
  expectNear(*readColor("hsl(-240deg 100% 50%)"), {0, 1, 0, 1}, 1e-6f, "wrap");
  EXPECT_EQ(hsl(0, 100, 50), (Color{1, 0, 0, 1}));
  expectNear(hsl(210, 40, 60), *readColor("hsl(210 40% 60%)"), 1e-6f, "hsl");

  EXPECT_EQ(readColor("hwb(0 0% 0%)"), (Color{1, 0, 0, 1}));
  expectNear(*readColor("hwb(120 0% 50%)"), {0, 0.5f, 0, 1}, 1e-6f, "hwb");
  // Whiteness and blackness past a whole scale back to a grey.
  expectNear(*readColor("hwb(0 60% 60%)"), {0.5f, 0.5f, 0.5f, 1}, 1e-6f,
             "grey");
  expectNear(hwb(200, 10, 20, 0.5f), *readColor("hwb(200 10% 20% / 0.5)"),
             1e-6f, "hwb builder");
}

// The specification's own example: one colour written in every notation
// CSS Color 4 gives it reads back as that colour.
TEST(Color, OneColourWrittenInEveryCssNotationReadsAsOne) {
  const Color written = hexColor(0x7654cd);
  for (const char* css :
       {"rgb(46.27% 32.94% 80.39%)", "lab(44.36% 36.05 -58.99)",
        "lch(44.36% 69.13 301.43)", "oklab(54.43% 0.0682 -0.1657)",
        "oklch(54.43% 0.1791 292.37)", "color(xyz-d50 0.2005 0.14089 0.4472)",
        "color(xyz-d65 0.21661 0.14602 0.59452)",
        "color(xyz 0.21661 0.14602 0.59452)",
        "color(srgb 0.4627 0.3294 0.8039)"}) {
    const std::optional<Color> read = readColor(css);
    ASSERT_TRUE(read) << css;
    expectNear(*read, written, 2.0f / 255.0f, css);
  }
  expectNear(lab(44.36f, 36.05f, -58.99f), written, 2.0f / 255.0f, "lab");
  expectNear(lch(44.36f, 69.13f, 301.43f), written, 2.0f / 255.0f, "lch");
  expectNear(oklab(0.5443f, 0.0682f, -0.1657f), written, 2.0f / 255.0f,
             "oklab");
  expectNear(oklch(0.5443f, 0.1791f, 292.37f), written, 2.0f / 255.0f, "oklch");
  // CSS's red, in the two spaces a stylesheet most often names it in.
  expectNear(*readColor("lab(54.29 80.82 69.91)"), {1, 0, 0, 1}, 2.0f / 255.0f,
             "lab red");
  expectNear(*readColor("oklch(62.8% 0.2577 29.23)"), {1, 0, 0, 1},
             2.0f / 255.0f, "oklch red");
  // Percentages are read against the reference CSS gives each component.
  expectNear(*readColor("oklch(100% 0% 0)"), {1, 1, 1, 1}, 1e-4f, "white");
  expectNear(*readColor("lab(100% 0% 0%)"), {1, 1, 1, 1}, 1e-3f, "lab white");
}

// The predefined RGB spaces reach sRGB through their own primaries and
// curves; white is white in each, and srgb alone keeps a channel past 1.
TEST(Color, ThePredefinedSpacesReadThroughTheirOwnPrimaries) {
  for (const char* white :
       {"color(srgb 1 1 1)", "color(srgb-linear 1 1 1)",
        "color(display-p3 1 1 1)", "color(rec2020 1 1 1)",
        "color(a98-rgb 1 1 1)", "color(prophoto-rgb 1 1 1)",
        "color(xyz-d65 0.95046 1 1.08906)", "color(xyz-d50 0.96422 1 0.82521)"})
    expectNear(*readColor(white), {1, 1, 1, 1}, 1e-3f, white);
  EXPECT_EQ(readColor("color(srgb 1.5 0 0)"), (Color{1.5f, 0, 0, 1}));
  expectNear(*readColor("color(srgb-linear 0.214 0.214 0.214 / 50%)"),
             {0.5f, 0.5f, 0.5f, 0.5f}, 1e-3f, "linear grey");
  expectNear(*readColor("color(display-p3 0.5 0.5 0.5)"), {0.5f, 0.5f, 0.5f, 1},
             1e-4f, "p3 grey");
}

// A colour outside sRGB keeps its OKLCH lightness and hue and gives up
// chroma until it fits, which is CSS's gamut mapping and `fitToSrgb`.
TEST(Color, AColourOutsideSrgbIsMappedByReducingChroma) {
  const Color mapped = *readColor("color(display-p3 1 0 0)");
  for (float channel : {mapped.r, mapped.g, mapped.b}) {
    EXPECT_GE(channel, 0.0f);
    EXPECT_LE(channel, 1.0f);
  }
  // Display P3's red, measured in OKLCH, is lightness 0.6486 at hue 28.96
  // with a chroma sRGB cannot show.
  const Oklch fitted = toOklch(mapped);
  EXPECT_NEAR(fitted.L, 0.6486f, 2e-3f);
  EXPECT_NEAR(fitted.hueDegrees, 28.96f, 1.0f);
  EXPECT_LT(fitted.chroma, 0.2995f);
  EXPECT_GT(fitted.chroma, 0.2f);

  expectNear(oklch(0.7f, 0.4f, 150), fitToSrgb({0.7f, 0.4f, 150, 1}), 2e-3f,
             "far outside");
  expectNear(*readColor("oklch(0.7 0.4 150)"), oklch(0.7f, 0.4f, 150), 1e-5f,
             "text and builder");
  // Lightness past white leaves no chroma to keep.
  expectNear(*readColor("oklch(1.2 0.1 30)"), {1, 1, 1, 1}, 2e-3f,
             "past white");
  expectNear(*readColor("lch(50 200 30)"), fitToSrgb(toOklch(lch(50, 200, 30))),
             2e-3f, "lch outside");
}

// color-mix() reads as `colorMix` mixes: shares normalised as CSS does,
// premultiplied by alpha, along the hue arc it names.
TEST(Color, ColorMixReadsAsCssMixes) {
  EXPECT_EQ(readColor("color-mix(in srgb, red, blue)"),
            (Color{0.5f, 0, 0.5f, 1}));
  EXPECT_EQ(readColor("color-mix(in srgb, red 25%, blue)"),
            (Color{0.25f, 0, 0.75f, 1}));
  EXPECT_EQ(readColor("color-mix(in srgb, 25% red, blue 75%)"),
            (Color{0.25f, 0, 0.75f, 1}));
  EXPECT_EQ(colorMix({1, 0, 0, 1}, {0, 0, 1, 1}, 0.75f, ColorSpace::Srgb),
            (Color{0.25f, 0, 0.75f, 1}));
  // Two shares short of a whole leave the shortfall as transparency.
  expectNear(*readColor("color-mix(in srgb, red 20%, blue 20%)"),
             {0.5f, 0, 0.5f, 0.4f}, 1e-6f, "short");
  // A transparent end lends its alpha and none of its colour.
  expectNear(*readColor("color-mix(in srgb, rgb(255 0 0 / 0), blue)"),
             {0, 0, 1, 0.5f}, 1e-6f, "premultiplied");
  expectNear(*readColor("color-mix(in oklab, white, black)"),
             colorMix({1, 1, 1, 1}, {0, 0, 0, 1}, 0.5f, ColorSpace::Oklab),
             1e-6f, "oklab");
  // The specification's example, mixed in CSS's LCH.
  expectNear(*readColor("color-mix(in lch, peru 40%, palegoldenrod)"),
             lch(79.7256f, 40.448f, 84.771f), 2.0f / 255.0f, "lch");
  // The long way round passes through the hues the short way skips.
  const Color shorter = *readColor("color-mix(in oklch, red, blue)");
  const Color longer = *readColor("color-mix(in oklch longer hue, red, blue)");
  EXPECT_GT(toOklch(shorter).hueDegrees, 300.0f);
  EXPECT_LT(toOklch(longer).hueDegrees, 200.0f);
  // A grey has no hue of its own, so the mix keeps the other's.
  const Color tinted =
      colorMix({1, 1, 1, 1}, hexColor(0x0000ff), 0.5f, ColorSpace::Oklch);
  EXPECT_NEAR(toOklch(tinted).hueDegrees,
              toOklch(hexColor(0x0000ff)).hueDegrees, 1.0f);
  EXPECT_EQ(readColorSpace("Display-P3"), ColorSpace::DisplayP3);
  EXPECT_EQ(readColorSpace("xyz"), ColorSpace::XyzD65);
  EXPECT_FALSE(readColorSpace("cmyk"));
}
