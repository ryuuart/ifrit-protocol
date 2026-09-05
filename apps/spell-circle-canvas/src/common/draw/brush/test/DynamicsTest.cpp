/** @file
 * The response curve and the three drives that read it.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Dynamics.h>

#include <array>
#include <memory>
#include <vector>

#include "Recorder.h"
#include "support/Paper.h"

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;
using sigil::draw::brush::testing::Recording;
using sigil::draw::brush::testing::recorder;
using sigil::draw::testing::Paper;

TEST(Dynamics, ACurveRampsBetweenItsTwoEndsAndBendsInBetween) {
  const brush::Curve straight{.minimum = 0.5f, .maximum = 1.5f};
  EXPECT_FLOAT_EQ(straight.at(0.0f), 0.5f);
  EXPECT_FLOAT_EQ(straight.at(1.0f), 1.5f);
  EXPECT_FLOAT_EQ(straight.at(0.5f), 1.0f);

  // Above one holds near the minimum until late in the range.
  const brush::Curve late{.minimum = 0.0f, .maximum = 1.0f, .bend = 2.0f};
  EXPECT_LT(late.at(0.5f), straight.at(0.5f) - 0.5f);
  EXPECT_FLOAT_EQ(late.at(1.0f), 1.0f);

  EXPECT_FLOAT_EQ(brush::Curve::flat(0.25f).at(0.9f), 0.25f);
}

TEST(Dynamics, ACurveOfTheCallersOwnReplacesTheThreeNumbers) {
  const brush::Curve mine{.minimum = 0.0f,
                          .maximum = 1.0f,
                          .curve = [](float unit) { return 3.0f - unit; }};
  EXPECT_FLOAT_EQ(mine.at(0.0f), 3.0f);
  EXPECT_FLOAT_EQ(mine.at(1.0f), 2.0f);
}

TEST(Dynamics, TheInputIsClampedToTheUnitRange) {
  const brush::Curve curve{.minimum = 0.0f, .maximum = 1.0f};
  EXPECT_FLOAT_EQ(curve.at(-4.0f), 0.0f);
  EXPECT_FLOAT_EQ(curve.at(9.0f), 1.0f);
}

TEST(Dynamics, EachDriveReadsItsOwnPartOfTheDab) {
  const brush::Curve identity{.minimum = 0.0f, .maximum = 1.0f};
  const brush::Dab dab{.tilt = 0.25f, .speed = 300.0f};

  const brush::Response byPressure{.drive = brush::Drive::Pressure,
                                   .curve = identity};
  const brush::Response byTilt{.drive = brush::Drive::Tilt,
                               .curve = identity};
  const brush::Response bySpeed{.drive = brush::Drive::Velocity,
                                .curve = identity};

  EXPECT_FLOAT_EQ(byPressure.at(dab, 0.75f, 1200.0f), 0.75f);
  EXPECT_FLOAT_EQ(byTilt.at(dab, 1.0f, 1200.0f), 0.25f);
  // One at the reference speed and above it.
  EXPECT_FLOAT_EQ(bySpeed.at(dab, 1.0f, 1200.0f), 0.25f);
  EXPECT_FLOAT_EQ(bySpeed.at(dab, 1.0f, 100.0f), 1.0f);
}

TEST(Dynamics, NoResponseIsNoChange) {
  brush::Tool tool = brush::marker(SkColors::kBlack, 8.0f);
  EXPECT_TRUE(tool.dynamics.empty());
  tool.dynamics.size = brush::Response{};
  EXPECT_FALSE(tool.dynamics.empty());
}

/** The size each stamp was scaled to, taken from the pen's transform at
 *  the moment a custom tip is called. */
std::vector<float> stampSizes(Pen& pen, const brush::Tool& source,
                              float pressure) {
  auto sizes = std::make_shared<std::vector<float>>();
  brush::Tool tool = source;
  tool.customTip = [sizes](Pen& tip, const brush::Dab&) {
    sizes->push_back(tip.canvas()->getLocalToDevice().asM33().getScaleX());
  };
  const std::array<brush::Dab, 1> dabs{{{.position = {40, 20},
                                         .pressure = pressure}}};
  brush::deposit(pen, tool, dabs);
  return *sizes;
}

TEST(Dynamics, ASizeResponseMultipliesWhatTheToolAlreadyDecided) {
  Paper paper(80, 40);
  paper.begin();
  brush::Tool tool = recorder(std::make_shared<Recording>());
  tool.width = 20.0f;
  tool.pressureSize = 0.0f;  // the scalar response out of the way
  tool.markerTip = false;

  const std::vector<float> plainLight = stampSizes(paper.pen, tool, 0.25f);
  const std::vector<float> plainFull = stampSizes(paper.pen, tool, 1.0f);

  tool.dynamics.size =
      brush::Response{.drive = brush::Drive::Pressure,
                      .curve = {.minimum = 0.1f, .maximum = 1.0f}};
  const std::vector<float> drivenLight = stampSizes(paper.pen, tool, 0.25f);
  const std::vector<float> drivenFull = stampSizes(paper.pen, tool, 1.0f);
  paper.end();

  ASSERT_EQ(plainLight.size(), 1u);
  ASSERT_EQ(drivenLight.size(), 1u);
  // With the scalar response off, pressure alone changes nothing.
  EXPECT_FLOAT_EQ(plainLight[0], plainFull[0]);
  // The curve answers its maximum at full pressure and much less light.
  EXPECT_FLOAT_EQ(drivenFull[0], plainFull[0]);
  EXPECT_LT(drivenLight[0], plainLight[0] * 0.5f);
}

TEST(Dynamics, AFlowResponseThinsTheOneDabsLoad) {
  const auto inkAt = [](bool driven) {
    Paper paper(80, 40, SK_ColorWHITE);
    paper.begin();
    brush::Tool tool = brush::marker(SkColors::kBlack, 20.0f);
    tool.tip = brush::Tip::Nib;
    tool.opacity = 1.0f;
    tool.markerTip = false;
    tool.scatter = 0.0f;
    tool.pressure = {1, 1, 1};
    tool.pressure.variation.reset();
    tool.sizeJitter = 0.0f;
    tool.opacityJitter = 0.0f;
    tool.pressureOpacity = 0.0f;
    tool.noise = 0.0f;
    if (driven)
      tool.dynamics.flow = brush::Response{.curve = brush::Curve::flat(0.25f)};
    const std::array<brush::Dab, 1> dabs{{{.position = {40, 20}}}};
    brush::deposit(paper.pen, tool, dabs);
    paper.end();
    return SkColorGetR(paper.pixels().getColor(40, 20));
  };

  EXPECT_EQ(inkAt(false), 0u);
  EXPECT_GT(inkAt(true), 100u);
}

}  // namespace
