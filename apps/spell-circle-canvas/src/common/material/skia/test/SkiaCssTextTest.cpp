/** @file
 * Gradients and filters read from their CSS text, against the examples
 * the CSS specifications give: each reads to what the CSS-shaped doors
 * build, and text they cannot hold reads as nothing.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/CssText.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>

#include <optional>
#include <string_view>
#include <vector>

using namespace sigil::material;

namespace {

const Color kYellow = *readColor("yellow");
const Color kBlue = *readColor("blue");
const Color kGreen = *readColor("green");
const Color kRed = *readColor("red");

std::optional<Material> read(std::string_view text) {
  const std::optional<Material> gradient = readGradient(text);
  EXPECT_TRUE(gradient) << text;
  return gradient;
}

}  // namespace

TEST(CssText, AValueIsCutAtItsTopLevelCommasAndBlankSpace) {
  const auto parts = css::commaParts(" a, rgb(1, 2, 3) ,b ");
  ASSERT_TRUE(parts);
  EXPECT_EQ(*parts, (std::vector<std::string_view>{"a", "rgb(1, 2, 3)", "b"}));
  EXPECT_FALSE(css::commaParts("rgb(1, 2"));
  EXPECT_EQ(
      css::components("0 4px rgb(0 0 0 / 30%)  inset"),
      (std::vector<std::string_view>{"0", "4px", "rgb(0 0 0 / 30%)", "inset"}));
  EXPECT_EQ(css::components("blur(4px)sepia(1)"),
            (std::vector<std::string_view>{"blur(4px)", "sepia(1)"}));
  const auto written = css::function(" Sepia( 50% ) ");
  ASSERT_TRUE(written);
  EXPECT_EQ(written->name, "Sepia");
  EXPECT_EQ(written->body, "50%");
  EXPECT_FALSE(css::function("blur(4px) sepia(1)"));
}

TEST(CssText, ANumberCarriesItsUnit) {
  EXPECT_EQ(css::percentage("40%"), 0.4f);
  EXPECT_EQ(css::pixels("12px"), 12.0f);
  EXPECT_EQ(css::pixels("3PT"), 4.0f);
  EXPECT_EQ(css::pixels("0"), 0.0f);
  EXPECT_FALSE(css::pixels("4"));
  EXPECT_FALSE(css::pixels("1em"));
  EXPECT_EQ(css::degrees("0.25turn"), 90.0f);
  EXPECT_EQ(css::degrees("0"), 0.0f);
  EXPECT_FALSE(css::degrees("45"));
}

TEST(GradientText, TheSpecificationsYellowToBlueReadsOneGradient) {
  // CSS Images: each of these is the same gradient, top to bottom.
  const Material placed =
      linearGradient(ColorStops({{0.0f, kYellow}, {1.0f, kBlue}}));
  EXPECT_EQ(read("linear-gradient(yellow, blue)"), placed);
  EXPECT_EQ(read("linear-gradient(180deg, yellow, blue)"), placed);
  EXPECT_EQ(read("linear-gradient(yellow 0%, blue 100%)"), placed);
  EXPECT_EQ(read("LINEAR-GRADIENT(0.5turn, Yellow, BLUE)"), placed);
  EXPECT_EQ(read("linear-gradient(to bottom, yellow, blue)"),
            linearGradient(ColorStops({{0.0f, kYellow}, {1.0f, kBlue}}),
                           Toward::Bottom));
  EXPECT_EQ(read("linear-gradient(to left top, blue, red)"),
            linearGradient(ColorStops({{0.0f, kBlue}, {1.0f, kRed}}),
                           Toward::TopLeft));
}

TEST(GradientText, AnUnplacedStopIsSpreadAsCssFixesItUp) {
  // linear-gradient(45deg, red, blue 40%): the red at the start.
  EXPECT_EQ(read("linear-gradient(45deg, red, blue 40%)"),
            linearGradient(ColorStops({{0.0f, kRed}, {0.4f, kBlue}}), 45.0f));
  // A run between two placed stops is spread evenly, and a stop placed
  // before one ahead of it is moved up to it.
  EXPECT_EQ(
      read("linear-gradient(red, yellow, green 60%, blue 20%)"),
      linearGradient(ColorStops(
          {{0.0f, kRed}, {0.3f, kYellow}, {0.6f, kGreen}, {0.6f, kBlue}})));
  // Two positions are the colour twice.
  EXPECT_EQ(read("linear-gradient(red 0 50%, blue 50% 100%)"),
            linearGradient(ColorStops(
                {{0.0f, kRed}, {0.5f, kRed}, {0.5f, kBlue}, {1.0f, kBlue}})));
  // A length along the line is the stop's pixels.
  EXPECT_EQ(read("radial-gradient(farthest-side at left bottom, red, "
                 "yellow 50px, green)"),
            radialGradient(
                ColorStops({{0.0f, kRed},
                            {0.0f, *readColor("yellow"), 50.0f},
                            {1.0f, kGreen}}),
                {.extent = RadialExtent::FarthestSide, .at = {0.0f, 1.0f}}));
}

TEST(GradientText, TheSpecificationsRadialFormsReadTheirShape) {
  const ColorStops stops({{0.0f, kYellow}, {1.0f, kGreen}});
  const Material centred = radialGradient(stops);
  EXPECT_EQ(read("radial-gradient(yellow, green)"), centred);
  EXPECT_EQ(read("radial-gradient(ellipse at center, yellow 0%, green 100%)"),
            centred);
  EXPECT_EQ(read("radial-gradient(farthest-corner at 50% 50%, yellow, green)"),
            centred);
  EXPECT_EQ(read("radial-gradient(circle closest-side at 30% 40%, yellow, "
                 "green)"),
            radialGradient(stops, {.shape = RadialShape::Circle,
                                   .extent = RadialExtent::ClosestSide,
                                   .at = {0.3f, 0.4f}}));
  EXPECT_EQ(read("radial-gradient(closest-corner circle at right, yellow, "
                 "green)"),
            radialGradient(stops, {.shape = RadialShape::Circle,
                                   .extent = RadialExtent::ClosestCorner,
                                   .at = {1.0f, 0.5f}}));
}

TEST(GradientText, TheSpecificationsConicFormsReadOneGradient) {
  const Color pink = *readColor("#f06");
  const Color gold = *readColor("gold");
  const Material plain =
      conicGradient(ColorStops({{0.0f, pink}, {1.0f, gold}}));
  for (const char* text :
       {"conic-gradient(#f06, gold)", "conic-gradient(at 50% 50%, #f06, gold)",
        "conic-gradient(from 0deg, #f06, gold)",
        "conic-gradient(from 0deg at center, #f06, gold)",
        "conic-gradient(#f06 0%, gold 100%)",
        "conic-gradient(#f06 0deg, gold 1turn)"}) {
    SCOPED_TRACE(text);
    EXPECT_EQ(read(text), plain);
  }
  EXPECT_EQ(read("conic-gradient(from 90deg at 25% 75%, red, blue 50%)"),
            conicGradient(ColorStops({{0.0f, kRed}, {0.5f, kBlue}}),
                          {.fromDegrees = 90.0f, .at = {0.25f, 0.75f}}));
}

TEST(GradientText, ARepeatingGradientRepeatsItsStops) {
  GradientOptions repeating;
  repeating.repeat = Repeat::Repeat;
  EXPECT_EQ(
      read("repeating-linear-gradient(red, blue 20%)"),
      linearGradient(ColorStops({{0.0f, kRed}, {0.2f, kBlue}}), {}, repeating));
  EXPECT_EQ(read("repeating-conic-gradient(red 0 15deg, blue 15deg 30deg)"),
            conicGradient(ColorStops({{0.0f, kRed},
                                      {15.0f / 360.0f, kRed},
                                      {15.0f / 360.0f, kBlue},
                                      {30.0f / 360.0f, kBlue}}),
                          {}, repeating));
}

TEST(GradientText, TextTheGradientsCannotHoldReadsAsNothing) {
  for (const char* text :
       {"linear-gradient(red)", "linear-gradient(red, 30%, blue)",
        "linear-gradient(in oklab, red, blue)",
        "linear-gradient(to middle, red, blue)",
        "linear-gradient(45, red, blue)", "linear-gradient(red 1em, blue)",
        "radial-gradient(40px, red, blue)",
        "radial-gradient(circle at 20px 20px, red, blue)",
        "conic-gradient(red 10px, blue)", "linear-gradient(red, blue",
        "linear-gradient(red, nonsense)", "sideways-gradient(red, blue)", "red",
        ""}) {
    SCOPED_TRACE(text);
    EXPECT_FALSE(readGradient(text));
  }
}

TEST(FilterText, TheSpecificationsFunctionsReadAsTheStockPasses) {
  EXPECT_EQ(readFilter("blur(5px)"), Filter::blur(5.0f));
  EXPECT_EQ(readFilter("sepia(100%)"), Filter::sepia(1.0f));
  EXPECT_EQ(readFilter("hue-rotate(90deg)"), Filter::hueRotate(90.0f));
  EXPECT_EQ(readFilter("contrast(200%)"), Filter::contrast(2.0f));
  EXPECT_EQ(readFilter("brightness(0.5)"), Filter::brightness(0.5f));
  EXPECT_EQ(readFilter("grayscale()"), Filter::grayscale(1.0f));
  EXPECT_EQ(readFilter("Invert(25%)"), Filter::invert(0.25f));
  EXPECT_EQ(readFilter("opacity(0.5)"), Filter::opacity(0.5f));
  EXPECT_EQ(readFilter("saturate(150%)"), Filter::saturate(1.5f));
  EXPECT_EQ(readFilter("none"), Filter{});
}

TEST(FilterText, AListIsAChainInTheOrderWritten) {
  EXPECT_EQ(readFilter("blur(4px) sepia(50%)"),
            Filter::blur(4.0f).then(Filter::sepia(0.5f)));
  EXPECT_NE(readFilter("sepia(50%) blur(4px)"),
            Filter::blur(4.0f).then(Filter::sepia(0.5f)));
}

TEST(FilterText, ADropShadowReadsItsOffsetBlurAndColour) {
  // Filter Effects: drop-shadow(16px 16px 20px blue).
  EXPECT_EQ(readFilter("drop-shadow(16px 16px 20px blue)"),
            Filter::dropShadow(kBlue, {.blur = 20.0f, .offset = {16, 16}}));
  EXPECT_EQ(
      readFilter("drop-shadow(rgb(0 0 0 / 50%) -2px 3px)"),
      Filter::dropShadow(*readColor("rgb(0 0 0 / 50%)"), {.offset = {-2, 3}}));
}

TEST(FilterText, ADropShadowWithNoColourIsInTheInkInForce) {
  // CSS's unstated shadow colour is currentcolor: the filter names it, and
  // the painter's ink stated over it is the shadow written in that ink.
  const std::optional<Filter> read = readFilter("drop-shadow(2px 2px 4px)");
  ASSERT_TRUE(read);
  EXPECT_TRUE(read->namesCurrentColor());
  EXPECT_NE(*read,
            Filter::dropShadow({0, 0, 0, 1}, {.blur = 4, .offset = {2, 2}}));
  EXPECT_EQ(read->withCurrentColor(kBlue),
            Filter::dropShadow(kBlue, {.blur = 4, .offset = {2, 2}}));
  EXPECT_FALSE(read->withCurrentColor(kBlue).namesCurrentColor());

  // Through a chain, each side stated.
  const std::optional<Filter> chained =
      readFilter("blur(1px) drop-shadow(2px 2px)");
  ASSERT_TRUE(chained);
  EXPECT_TRUE(chained->namesCurrentColor());
  EXPECT_EQ(
      chained->withCurrentColor(kBlue),
      Filter::blur(1.0f).then(Filter::dropShadow(kBlue, {.offset = {2, 2}})));
  // A filter that names no current colour is itself.
  EXPECT_FALSE(Filter::blur(2.0f).namesCurrentColor());
  EXPECT_EQ(Filter::blur(2.0f).withCurrentColor(kBlue), Filter::blur(2.0f));
}

TEST(FilterText, TextNoFilterHoldsReadsAsNothing) {
  for (const char* text :
       {"blur(-1px)", "blur(1em)", "sepia(-50%)", "hue-rotate(90)",
        "drop-shadow(2px)", "drop-shadow(2px blue 2px)", "url(#shadow)",
        "blur(4px), sepia(1)", "wobble(2)", ""}) {
    SCOPED_TRACE(text);
    EXPECT_FALSE(readFilter(text));
  }
}
