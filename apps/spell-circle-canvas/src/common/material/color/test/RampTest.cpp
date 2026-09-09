/** @file
 * The ramp as a value: its ends round-trip, its domain reads the
 * caller's own numbers, reversing and easing move the position and not
 * the stops, each space walks its own path between two colours, and the
 * two crossings between a ramp and a fixed table agree.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/color/Ramp.h>

#include <string>
#include <tuple>

using namespace sigil::material;

namespace {

Ramp blackToWhite(RampSpace space = RampSpace::Srgb) {
  return Ramp{.stops = {{0.0f, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}},
              .space = space};
}

}  // namespace

TEST(Ramp, TheEndsAreTheStopsAndOutsideIsFlat) {
  const Ramp ramp = blackToWhite();
  EXPECT_FLOAT_EQ(ramp.at(0.0f).r, 0.0f);
  EXPECT_FLOAT_EQ(ramp.at(1.0f).r, 1.0f);
  // A ramp carries no answer for what lies beyond its ends, so the end
  // stop's flat colour is what an out-of-range read gets — never an
  // extrapolation.
  EXPECT_FLOAT_EQ(ramp.at(-4.0f).r, 0.0f);
  EXPECT_FLOAT_EQ(ramp.at(9.0f).r, 1.0f);
  EXPECT_EQ(Ramp{}.at(0.5f), (Color{0, 0, 0, 0}));
}

TEST(Ramp, TheDomainIsTheCallersOwnNumbers) {
  Ramp heat = blackToWhite();
  heat.domainLow = 20.0f;
  heat.domainHigh = 40.0f;
  EXPECT_FLOAT_EQ(heat.position(20.0f), 0.0f);
  EXPECT_FLOAT_EQ(heat.position(30.0f), 0.5f);
  EXPECT_FLOAT_EQ(heat.position(40.0f), 1.0f);
  // A range with no inside cannot say where a value falls in it; the
  // first stop is the only honest answer.
  Ramp degenerate = heat;
  degenerate.domainHigh = degenerate.domainLow;
  EXPECT_FLOAT_EQ(degenerate.position(35.0f), 0.0f);
}

TEST(Ramp, ReverseAndEasingMoveThePositionNotTheStops) {
  Ramp ramp = blackToWhite();
  ramp.reverse = true;
  EXPECT_FLOAT_EQ(ramp.at(0.0f).r, 1.0f);
  EXPECT_FLOAT_EQ(ramp.at(1.0f).r, 0.0f);

  Ramp eased = blackToWhite();
  eased.easing = {[](float t, const float*) { return t * t; }};
  EXPECT_NEAR(eased.position(0.5f), 0.25f, 1e-6f);
  EXPECT_NEAR(eased.at(0.5f).r, blackToWhite().at(0.25f).r, 1e-6f);
  // The stop list is untouched by either, which is what lets a reversed
  // or eased colormap compare against the one it came from.
  EXPECT_EQ(eased.stops, blackToWhite().stops);
}

TEST(Ramp, EachSpaceWalksItsOwnPathAndTheEndsStillRoundTrip) {
  const Color grey = blackToWhite(RampSpace::Srgb).at(0.5f);
  const Color light = blackToWhite(RampSpace::Linear).at(0.5f);
  // Half way in code values carries a fifth of white's light; half way
  // in linear light carries half of it, and the two must not be the same
  // number or the ramp is walking the wrong space.
  EXPECT_NEAR(grey.r, 0.5f, 1e-5f);
  EXPECT_GT(light.r, 0.7f);
  EXPECT_NEAR(luminance(light), 0.5f, 1e-3f);

  // Red to green through OKLCH goes the short way round the hue circle,
  // which is through orange and yellow; through OKLab it crosses the
  // middle of the space, where the chroma collapses.
  const Color red{1, 0, 0, 1}, green{0, 1, 0, 1};
  Ramp lab{.stops = {{0.0f, red}, {1.0f, green}}, .space = RampSpace::Oklab};
  Ramp lch = lab;
  lch.space = RampSpace::Oklch;
  EXPECT_GT(toOklch(lch.at(0.5f)).chroma, toOklch(lab.at(0.5f)).chroma);
  for (const Ramp& ramp : {lab, lch}) {
    EXPECT_NEAR(ramp.at(0.0f).r, 1.0f, 1e-4f);
    EXPECT_NEAR(ramp.at(1.0f).g, 1.0f, 1e-4f);
  }

  // The longer arc from red to green is the other way round the wheel,
  // so it passes through blue rather than through yellow.
  Ramp longer = lch;
  longer.arc = HueArc::Longer;
  EXPECT_GT(longer.at(0.5f).b, lch.at(0.5f).b);
}

TEST(Ramp, TwoStopsAtOnePositionAreAHardEdge) {
  const Ramp banded{.stops = {{0.0f, {0, 0, 0, 1}},
                              {0.5f, {0, 0, 0, 1}},
                              {0.5f, {1, 1, 1, 1}},
                              {1.0f, {1, 1, 1, 1}}},
                    .space = RampSpace::Srgb};
  EXPECT_FLOAT_EQ(banded.at(0.49f).r, 0.0f);
  EXPECT_FLOAT_EQ(banded.at(0.51f).r, 1.0f);
}

TEST(Ramp, OneStopIsFlatAndAnUnorderedListIsReported) {
  // One stop is a ramp with no walk in it: every position is that
  // colour, at both ends and outside them.
  const Ramp single{.stops = {{0.5f, {0.25f, 0.5f, 0.75f, 1}}},
                    .space = RampSpace::Srgb};
  for (float t : {0.0f, 0.5f, 1.0f, -2.0f, 3.0f}) {
    EXPECT_FLOAT_EQ(single.at(t).r, 0.25f) << t;
    EXPECT_FLOAT_EQ(single.at(t).b, 0.75f) << t;
  }

  // Stops out of position order are read in the order given, with the
  // first and last as the extremes, so the colours are not the ramp
  // anybody meant — and the caller who built the list from a map or a
  // set cannot see that from the picture. It is said once, on stderr.
  const Ramp jumbled{.stops = {{1.0f, {1, 0, 0, 1}},
                               {0.0f, {0, 0, 1, 1}},
                               {0.5f, {0, 1, 0, 1}}},
                     .space = RampSpace::Srgb};
  testing::internal::CaptureStderr();
  std::ignore = jumbled.at(0.25f);
  const std::string said = testing::internal::GetCapturedStderr();
  EXPECT_NE(said.find("position order"), std::string::npos) << said;

  // An ordered ramp says nothing, however many times it is read.
  testing::internal::CaptureStderr();
  for (int i = 0; i <= 10; ++i)
    std::ignore = blackToWhite().at((float)i / 10.0f);
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
}

TEST(Ramp, ATableIsReadAtBandCentresAndComesBackAsARamp) {
  const Palette table = palette(blackToWhite(), 4);
  ASSERT_EQ(table.size(), 4u);
  // Centres, not ends: four entries stand for four bands, so the first
  // is the colour an eighth of the way along and not the one at zero.
  EXPECT_NEAR(table.at(0).r, 0.125f, 1e-5f);
  EXPECT_NEAR(table.at(3).r, 0.875f, 1e-5f);

  const Ramp back = ramp(table);
  ASSERT_EQ(back.stops.size(), 4u);
  EXPECT_FLOAT_EQ(back.stops.front().pos, 0.0f);
  EXPECT_FLOAT_EQ(back.stops.back().pos, 1.0f);
  EXPECT_EQ(back.stops.front().color, table.at(0));
}

TEST(Ramp, ItIsAnInterpolatorAnythingCanCall) {
  const Ramp ramp = blackToWhite();
  // The call a scale makes: a unit position in, a colour out. Spelled as
  // a callable here so a mapping that knows nothing about colour can
  // still answer one.
  auto through = [](double position, const auto& interpolate) {
    return interpolate(position);
  };
  EXPECT_FLOAT_EQ(through(0.25, ramp).r, 0.25f);
}
