/** @file
 * The shape source: what an imported tip states against its own stamp —
 * the spacing between two stamps, the scatter off the centreline and the
 * turn each stamp takes.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Deposit.h>

#include <cmath>
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

/** A solid square of coverage, the simplest artwork a tip can be. */
sk_sp<SkImage> block(int side) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(side, side, true);
  bitmap.eraseColor(SK_ColorWHITE);
  bitmap.setImmutable();
  return SkImages::RasterFromBitmap(bitmap);
}

/** Where each stamp landed and how far it turned, taken from the pen's
 *  transform at the moment a custom tip is called: the executor arrives
 *  translated to the stamp, turned to its angle and scaled to its size,
 *  so the transform is the whole of what the dynamics decided. */
struct Placements {
  std::vector<SkPoint> positions;
  std::vector<float> angles;
  std::vector<float> sizes;
};

brush::Tool placementRecorder(std::shared_ptr<Placements> placed,
                              float width) {
  brush::Tool tool = recorder(std::make_shared<Recording>());
  tool.width = width;
  tool.opacity = 1.0f;
  tool.pressureSize = 0.0f;
  tool.pressureOpacity = 0.0f;
  tool.customTip = [placed](Pen& pen, const brush::Dab&) {
    const SkMatrix matrix = pen.canvas()->getLocalToDevice().asM33();
    placed->positions.push_back({matrix.getTranslateX(),
                                 matrix.getTranslateY()});
    placed->angles.push_back(std::atan2(matrix.getSkewY(),
                                        matrix.getScaleX()));
    placed->sizes.push_back(std::hypot(matrix.getScaleX(),
                                       matrix.getSkewY()));
  };
  return tool;
}

TEST(Shape, SpacingIsAFractionOfTheStampAndFollowsTheWidth) {
  Paper paper(200, 40);
  paper.begin();
  auto placed = std::make_shared<Placements>();
  brush::Tool tool = placementRecorder(placed, 20.0f);
  tool.shape = brush::Shape{.image = block(8), .spacing = 0.5f};

  brush::line(paper.pen, tool, {20, 20}, {180, 20});
  paper.end();

  // Ten canvas units between stamps over a hundred and sixty of travel.
  EXPECT_EQ(brush::spacingOf(tool), 10.0f);
  EXPECT_EQ(placed->positions.size(), 17u);
  ASSERT_GE(placed->positions.size(), 2u);
  EXPECT_NEAR(placed->positions[1].fX - placed->positions[0].fX, 10.0f, 0.01f);

  tool.width = 40.0f;
  EXPECT_EQ(brush::spacingOf(tool), 20.0f);
}

TEST(Shape, WithoutAShapeTheToolKeepsItsCanvasUnitSpacing) {
  brush::Tool tool = brush::marker(SkColors::kBlack, 20.0f);
  tool.spacing = 3.0f;
  EXPECT_EQ(brush::spacingOf(tool), 3.0f);
}

TEST(Shape, ScatterThrowsStampsOffTheCentrelineInBothAxes) {
  Paper paper(200, 80);
  paper.begin();
  auto steady = std::make_shared<Placements>();
  brush::Tool tool = placementRecorder(steady, 20.0f);
  tool.shape = brush::Shape{.image = block(8), .spacing = 0.5f};
  brush::line(paper.pen, tool, {20, 40}, {180, 40});

  auto thrown = std::make_shared<Placements>();
  brush::Tool scattered = placementRecorder(thrown, 20.0f);
  scattered.shape =
      brush::Shape{.image = block(8), .spacing = 0.5f, .scatter = 0.5f};
  brush::line(paper.pen, scattered, {20, 40}, {180, 40});
  paper.end();

  ASSERT_EQ(steady->positions.size(), thrown->positions.size());
  float steadySpread = 0.0f;
  float thrownSpread = 0.0f;
  for (size_t i = 0; i < steady->positions.size(); ++i) {
    steadySpread =
        std::max(steadySpread, std::abs(steady->positions[i].fY - 40.0f));
    thrownSpread =
        std::max(thrownSpread, std::abs(thrown->positions[i].fY - 40.0f));
  }
  EXPECT_LT(steadySpread, 0.01f);
  EXPECT_GT(thrownSpread, 1.0f);
  // Half the width either way, in both axes.
  EXPECT_LE(thrownSpread, 10.0f);
}

TEST(Shape, AngleJitterTurnsEachStampOnTopOfTheHeading) {
  Paper paper(200, 40);
  paper.begin();
  auto steady = std::make_shared<Placements>();
  brush::Tool tool = placementRecorder(steady, 20.0f);
  tool.shape = brush::Shape{.image = block(8), .spacing = 0.5f};
  brush::line(paper.pen, tool, {20, 20}, {180, 20});

  auto turned = std::make_shared<Placements>();
  brush::Tool jittered = placementRecorder(turned, 20.0f);
  jittered.shape =
      brush::Shape{.image = block(8), .spacing = 0.5f, .angleJitter = 0.5f};
  brush::line(paper.pen, jittered, {20, 20}, {180, 20});
  paper.end();

  ASSERT_EQ(steady->angles.size(), turned->angles.size());
  float steadySwing = 0.0f;
  float turnedSwing = 0.0f;
  for (size_t i = 0; i < steady->angles.size(); ++i) {
    steadySwing = std::max(steadySwing, std::abs(steady->angles[i]));
    turnedSwing = std::max(turnedSwing, std::abs(turned->angles[i]));
  }
  EXPECT_LT(steadySwing, 0.001f);
  EXPECT_GT(turnedSwing, 0.05f);
  EXPECT_LE(turnedSwing, 0.5f);
}

TEST(Shape, TheStampCoversTheWidthItIsGiven) {
  Paper paper(120, 120, SK_ColorWHITE);
  paper.begin();
  brush::Tool tool = brush::marker(SkColors::kBlack, 40.0f);
  tool.tip = brush::Tip::Image;
  tool.opacity = 1.0f;
  tool.markerTip = false;
  tool.pressure = {1, 1, 1};
  tool.pressure.variation.reset();
  tool.sizeJitter = 0.0f;
  tool.opacityJitter = 0.0f;
  tool.shape = brush::Shape{.image = block(16), .mask = brush::ImageMask::Alpha};
  const std::array<brush::Dab, 1> dabs{{{.position = {60, 60}}}};
  brush::deposit(paper.pen, tool, dabs);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_NE(pixels.getColor(60, 60), SK_ColorWHITE);
  EXPECT_NE(pixels.getColor(44, 44), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(38, 38), SK_ColorWHITE);
}

}  // namespace
