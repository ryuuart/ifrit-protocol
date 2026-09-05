/** @file
 * Deposition: the tips, the dynamics, the batching, and what a stored
 * path carries to the executor.
 */

#include <gtest/gtest.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Sampler.h>

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

TEST(Deposit, TiltCanDriveTipPositionSizeAspectAndDirection) {
  Paper paper(120, 80, SK_ColorWHITE);
  paper.begin();
  brush::Tool tool = brush::marker(SkColors::kBlack, 12.0f);
  tool.opacity = 1.0f;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  tool.pressureOpacity = 0.0f;
  tool.markerTip = false;
  tool.rotation = brush::Rotation::Tilt;
  tool.aspect = 0.2f;
  tool.tiltSize = 0.5f;
  tool.tiltAspect = 1.0f;
  tool.tiltOffset = 1.0f;
  const std::array<brush::Dab, 2> marks{{
      {.position = {20, 40}, .pressure = 1.0f},
      {.position = {70, 40},
       .pressure = 1.0f,
       .tilt = 1.0f,
       .tiltDirection = HALF_PI},
  }};
  brush::deposit(paper.pen, tool, marks);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_NE(pixels.getColor(20, 40), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(70, 40), SK_ColorWHITE);
  EXPECT_NE(pixels.getColor(70, 52), SK_ColorWHITE);
}

TEST(Deposit, CustomTipReceivesEveryResampledDab) {
  Paper paper(80, 40, SK_ColorWHITE);
  paper.begin();
  auto recording = std::make_shared<Recording>();
  brush::Tool tool = recorder(recording);
  tool.customTip = [recording](Pen& tip, const brush::Dab& dab) {
    recording->dabs.push_back(dab);
    tip.noStroke();
    tip.rectMode(CENTER);
    tip.rect(0, 0, 1, 0.35f);
  };
  tool.width = 8.0f;
  tool.opacity = 1.0f;
  const std::vector<brush::Input> input = {
      {.position = {10, 20}, .seconds = 0.0},
      {.position = {70, 20}, .seconds = 0.1}};
  const std::vector<brush::Dab> sampled = brush::dabs(input, 10.0f);
  brush::deposit(paper.pen, tool, sampled);
  paper.end();

  EXPECT_EQ(recording->dabs.size(), sampled.size());
  EXPECT_NE(paper.pixel(40, 20), SK_ColorWHITE);
}

TEST(Deposit, ShapeTipUsesDarkArtworkAsTheDefaultMask) {
  SkBitmap mask;
  mask.allocN32Pixels(8, 8, true);
  mask.eraseColor(SK_ColorWHITE);
  for (int y = 2; y < 6; ++y)
    for (int x = 2; x < 6; ++x) *mask.getAddr32(x, y) = SK_ColorBLACK;

  Paper paper(80, 80, SK_ColorWHITE);
  paper.begin();
  brush::Tool tool = brush::marker(SkColors::kRed, 32.0f);
  tool.tip = brush::Tip::Image;
  tool.shape = brush::Shape{.image = SkImages::RasterFromBitmap(mask)};
  tool.opacity = 1.0f;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  tool.sizeJitter = 0.0f;
  tool.opacityJitter = 0.0f;
  tool.spacingJitter = 0.0f;
  tool.markerTip = false;
  const std::array<brush::Dab, 1> dabs{{{.position = {40, 40}, .pressure = 1.0f}}};
  brush::deposit(paper.pen, tool, dabs);

  SkBitmap alphaMask;
  alphaMask.allocN32Pixels(8, 8, true);
  alphaMask.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 2; y < 6; ++y)
    for (int x = 2; x < 6; ++x) *alphaMask.getAddr32(x, y) = SK_ColorWHITE;
  tool.shape = brush::Shape{.image = SkImages::RasterFromBitmap(alphaMask),
                            .mask = brush::ImageMask::Alpha};
  const std::array<brush::Dab, 1> alphaDabs{
      {{.position = {15, 40}, .pressure = 1.0f}}};
  brush::deposit(paper.pen, tool, alphaDabs);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_EQ(pixels.getColor(40, 40), SK_ColorRED);
  EXPECT_EQ(pixels.getColor(27, 27), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(15, 40), SK_ColorRED);
  EXPECT_EQ(pixels.getColor(2, 27), SK_ColorWHITE);
}

TEST(Deposit, PaintDepositsPigmentAndRestoresThePenStyle) {
  Paper paper(120, 80, SK_ColorWHITE);
  paper.begin();
  paper.pen.randomSeed(7);
  paper.pen.noFill();
  paper.pen.stroke(255, 0, 0);
  paper.pen.strokeWeight(3);

  brush::Tool tool = brush::marker(SkColors::kBlue, 10.0f);
  tool.opacity = 1.0f;
  tool.scatter = 0.0f;
  tool.bristles = 1;
  tool.pressure = {1, 1, 1};
  brush::line(paper.pen, tool, {10, 20}, {110, 20});
  paper.pen.line(10, 60, 110, 60);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_EQ(pixels.getColor(60, 20), SK_ColorBLUE);
  EXPECT_EQ(pixels.getColor(60, 60), SK_ColorRED);
}

TEST(Deposit, RecordsRoundDabsAsBatchesRatherThanOnePerDab) {
  // The same stroke laid down at two spacings: the dense one deposits
  // five times the dabs. A recording that spent an op per dab would be
  // five times the size; batched, the count is governed by how many
  // batches a run needs and not by what is in them.
  const auto opsAtSpacing = [](float spacing) {
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(420, 80);
    Pen pen;
    pen.begin(*canvas, {.width = 420, .height = 80});
    pen.randomSeed(17);
    brush::Tool tool = brush::charcoal(SkColors::kBlack, 0.35f);
    tool.spacing = spacing;
    brush::line(pen, tool, {10, 40}, {410, 40});
    pen.end();
    const sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
    return picture ? picture->approximateOpCount() : -1;
  };
  const int sparse = opsAtSpacing(0.15f);
  const int dense = opsAtSpacing(0.03f);
  ASSERT_GT(sparse, 0);
  EXPECT_LT(dense, sparse * 2);
}

TEST(Deposit, AStoredPathCarriesNoSpeedSoSpeedDynamicsLeaveItAlone) {
  Paper paper(120, 40, SK_ColorWHITE);
  paper.begin();
  auto recording = std::make_shared<Recording>();
  brush::Tool tool = recorder(recording, 5.0f);
  tool.speedSize = 1.0f;
  tool.speedOpacity = 1.0f;
  brush::line(paper.pen, tool, {10, 20}, {110, 20});
  paper.end();

  ASSERT_GT(recording->dabs.size(), 2u);
  for (const brush::Dab& dab : recording->dabs) EXPECT_FLOAT_EQ(dab.speed, 0.0f);
  // Every dab reached the tip: a third of the size would still be a mark,
  // but a speed of zero is what the executor is handed.
  EXPECT_EQ(recording->dabs.size(), recording->fills.size());
}

TEST(Deposit, ACustomTipGetsThePigmentAndTheDefaultModesEveryDab) {
  Paper paper(120, 40, SK_ColorWHITE);
  paper.begin();
  auto recording = std::make_shared<Recording>();
  brush::Tool tool = recorder(recording, 10.0f);
  tool.color = SkColors::kBlue;
  tool.opacity = 1.0f;
  tool.pressureOpacity = 0.0f;
  tool.customTip = [recording](Pen& tip, const brush::Dab& dab) {
    recording->dabs.push_back(dab);
    if (const SkPaint* paint = tip.fillPaint())
      recording->fills.push_back(paint->getColor4f());
    tip.noFill();
    tip.noStroke();
  };
  brush::line(paper.pen, tool, {10, 20}, {110, 20});
  paper.end();

  ASSERT_GT(recording->fills.size(), 2u);
  EXPECT_EQ(recording->fills.size(), recording->dabs.size());
  for (const SkColor4f& fill : recording->fills) {
    EXPECT_FLOAT_EQ(fill.fB, 1.0f);
    EXPECT_FLOAT_EQ(fill.fA, 1.0f);
  }
}

}  // namespace
