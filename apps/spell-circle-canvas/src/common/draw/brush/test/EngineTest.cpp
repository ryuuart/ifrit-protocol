/** @file
 * The engine: selection and state, strokes through the pen's units and
 * clock, surfaces under one clip, and live input.
 */

#include <gtest/gtest.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Engine.h>

#include <array>
#include <cmath>
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

TEST(Engine, OwnsSelectionAndRelativeStrokeState) {
  brush::Engine engine;
  EXPECT_FALSE(engine.hasStroke());
  ASSERT_NE(engine.set("HB", SkColors::kRed, 2.0f), nullptr);
  EXPECT_TRUE(engine.hasStroke());
  EXPECT_EQ(engine.tool().color, SkColors::kRed);
  EXPECT_FLOAT_EQ(engine.tool().width,
                  brush::Catalogue::stock().find("HB")->width * 2.0f);
  EXPECT_EQ(engine.pick("missing"), nullptr);
  EXPECT_TRUE(engine.hasStroke());
  engine.push();
  engine.noStroke();
  EXPECT_FALSE(engine.hasStroke());
  engine.pop();
  EXPECT_TRUE(engine.hasStroke());
  engine.pop();
  EXPECT_TRUE(engine.hasStroke());

  Paper paper(100, 60, SK_ColorWHITE);
  paper.begin();
  engine.beginStroke(brush::PlotType::Segments, {10, 30});
  engine.move(paper.pen, 0.0f, 80.0f, 0.8f);
  const brush::Stroke result = engine.endStroke(paper.pen, 0.0f, 0.5f);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result.back().position, SkPoint::Make(90, 30));
  EXPECT_FLOAT_EQ(result.back().pressure, 0.5f);

  // A quarter turn in the pen's degrees heads down the page.
  paper.pen.angleMode(DEGREES);
  engine.beginStroke(brush::PlotType::Curve, {10, 20});
  engine.move(paper.pen, 0.0f, 20.0f);
  const brush::Stroke curved = engine.endStroke(paper.pen, 90.0f);
  paper.end();
  ASSERT_GE(curved.size(), 2u);
  EXPECT_GT(curved.back().position.fY, curved[curved.size() - 2].position.fY);
}

TEST(Engine, ComposesFillHatchFieldAndClipAsOwnedState) {
  brush::Engine engine;
  ASSERT_NE(engine.set("rotring", SkColors::kBlack, 1.0f), nullptr);
  engine.fill(SkColors::kBlue, 0.5f);
  engine.noWash();
  ASSERT_NE(engine.hatchStyle("HB", SkColors::kRed, 1.0f), nullptr);
  engine.hatch({.spacing = 8.0f, .angle = 0.3f});
  EXPECT_TRUE(engine.field("waves"));
  EXPECT_FALSE(engine.field("missing"));
  engine.clip(SkRect::MakeLTRB(20, 10, 80, 70));

  Paper paper(100, 80, SK_ColorWHITE);
  paper.begin();
  engine.rect(paper.pen, 5, 5, 90, 70);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_EQ(pixels.getColor(0, 79), SK_ColorWHITE);
  EXPECT_NE(pixels.getColor(50, 40), SK_ColorWHITE);
  EXPECT_EQ(engine.listFields().size(), 7u);
}

TEST(Engine, TheClipConfinesEveryInteriorAndTheOutline) {
  const brush::Polygon square({{10, 10}, {90, 10}, {90, 90}, {10, 90}});
  const auto rightHalfIsClean = [&](brush::Engine& engine, auto paint) {
    Paper paper(100, 100, SK_ColorWHITE);
    paper.begin();
    paper.pen.randomSeed(4);
    paper.pen.noiseSeed(4);
    engine.clip(SkRect::MakeLTRB(0, 0, 50, 100));
    paint(engine, paper.pen);
    paper.end();
    const SkBitmap pixels = paper.pixels();
    int leftInk = 0;
    int rightInk = 0;
    for (int y = 12; y < 88; ++y)
      for (int x = 12; x < 88; ++x) {
        if (pixels.getColor(x, y) == SK_ColorWHITE) continue;
        (x < 50 ? leftInk : rightInk)++;
      }
    EXPECT_GT(leftInk, 0);
    EXPECT_EQ(rightInk, 0);
  };

  brush::Engine flat;
  flat.noStroke();
  flat.wash(SkColors::kBlue, 1.0f);
  rightHalfIsClean(flat, [&](brush::Engine& engine, Pen& pen) {
    engine.wash(pen, square);
  });

  brush::Engine pigment;
  pigment.noStroke();
  pigment.fill(SkColors::kBlue, 1.0f);
  rightHalfIsClean(pigment, [&](brush::Engine& engine, Pen& pen) {
    engine.fill(pen, square);
  });

  brush::Engine gestures;
  gestures.noStroke();
  ASSERT_NE(gestures.mass("crayon", SkColors::kBlack, {.strength = 1.0f}),
            nullptr);
  rightHalfIsClean(gestures, [&](brush::Engine& engine, Pen& pen) {
    engine.mass(pen, square);
  });

  brush::Engine whole;
  whole.set("marker", SkColors::kBlack, 2.0f);
  whole.wash(SkColors::kBlue, 1.0f);
  whole.fill(SkColors::kRed, 1.0f);
  whole.mass("crayon", SkColors::kBlack);
  whole.hatch({.spacing = 6.0f});
  rightHalfIsClean(whole, [&](brush::Engine& engine, Pen& pen) {
    engine.polygon(pen, square);
  });
}

TEST(Engine, KeepsWatercolorAndFlatWashIndependent) {
  brush::Engine engine;
  engine.noStroke();
  engine.fill(SkColors::kRed, 0.4f);
  engine.wash(SkColors::kBlue, 0.6f);
  const brush::Polygon polygon({{8, 8}, {56, 8}, {56, 56}, {8, 56}});

  Paper paper(64, 64, SK_ColorWHITE);
  paper.begin();
  engine.noFill();
  engine.wash(paper.pen, polygon);
  paper.end();
  const SkColor flat = paper.pixel(32, 32);
  EXPECT_NE(flat, SK_ColorWHITE);
  EXPECT_GT(SkColorGetB(flat), SkColorGetR(flat));

  paper.surface->getCanvas()->clear(SK_ColorWHITE);
  paper.begin();
  engine.fill(SkColors::kRed, 0.4f);
  engine.noWash();
  engine.wash(paper.pen, polygon);
  EXPECT_EQ(paper.pixel(32, 32), SK_ColorWHITE);
  engine.fill(paper.pen, polygon);
  paper.end();
  const SkColor pigment = paper.pixel(32, 32);
  EXPECT_NE(pigment, SK_ColorWHITE);
  EXPECT_GT(SkColorGetR(pigment), SkColorGetB(pigment));
}

TEST(Engine, HatchUsesTheCurrentToolUntilOverridden) {
  brush::Engine engine;
  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("recorder", recorder(recording)), nullptr);
  ASSERT_NE(engine.set("recorder", SkColors::kBlue), nullptr);
  engine.hatch({.spacing = 12.0f, .angle = 0.0f});
  const brush::Polygon polygon({{5, 5}, {55, 5}, {55, 55}, {5, 55}});

  Paper paper(64, 64);
  paper.begin();
  engine.hatch(paper.pen, polygon);
  ASSERT_FALSE(recording->fills.empty());
  EXPECT_FLOAT_EQ(recording->fills.back().fB, 1.0f);

  recording->fills.clear();
  ASSERT_NE(engine.hatchStyle("recorder", SkColors::kRed), nullptr);
  engine.hatch(paper.pen, polygon);
  ASSERT_FALSE(recording->fills.empty());
  EXPECT_FLOAT_EQ(recording->fills.back().fR, 1.0f);

  recording->fills.clear();
  engine.noHatch();
  engine.hatch({.spacing = 12.0f, .angle = 0.0f});
  engine.hatch(paper.pen, polygon);
  ASSERT_FALSE(recording->fills.empty());
  EXPECT_FLOAT_EQ(recording->fills.back().fB, 1.0f);
  paper.end();
}

TEST(Engine, CapturesTheClipTransformAtTheCall) {
  brush::Engine engine;
  brush::Tool solid = brush::marker(SkColors::kBlue, 4.0f);
  solid.opacity = 1.0f;
  solid.scatter = 0.0f;
  solid.markerTip = false;
  solid.pressure = {1, 1, 1};
  solid.pressure.variation.reset();
  ASSERT_NE(engine.add("solid", solid), nullptr);
  ASSERT_NE(engine.set("solid", SkColors::kBlue), nullptr);

  Paper paper(80, 40, SK_ColorWHITE);
  paper.begin();
  paper.pen.translate(20, 0);
  engine.clip(paper.pen, SkRect::MakeXYWH(0, 0, 20, 40));
  paper.pen.resetMatrix();
  engine.line(paper.pen, {0, 20}, {80, 20});
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_EQ(pixels.getColor(10, 20), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(30, 20), SK_ColorBLUE);
  EXPECT_EQ(pixels.getColor(50, 20), SK_ColorWHITE);
}

TEST(Engine, IntegratesGeometryThroughTheSelectedField) {
  brush::Engine engine;
  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("recorder", recorder(recording, 2.0f)), nullptr);
  ASSERT_NE(engine.set("recorder", SkColors::kBlack), nullptr);
  ASSERT_TRUE(
      engine.addField("quarter-turn", [](SkPoint, float) { return HALF_PI; }));
  ASSERT_TRUE(engine.field("quarter-turn"));

  Paper paper(80, 80);
  paper.begin();
  engine.line(paper.pen, {10, 10}, {50, 10});
  paper.end();

  ASSERT_GT(recording->dabs.size(), 2u);
  EXPECT_NEAR(recording->dabs.back().position.fX, 10.0f, 1e-3f);
  EXPECT_NEAR(recording->dabs.back().position.fY, 50.0f, 1e-3f);
}

TEST(Engine, ReadsTheFieldAtThePensClock) {
  brush::Engine engine;
  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("recorder", recorder(recording, 2.0f)), nullptr);
  ASSERT_NE(engine.set("recorder", SkColors::kBlack), nullptr);
  ASSERT_TRUE(engine.addField(
      "clock", [](SkPoint, float seconds) { return seconds; }));
  ASSERT_TRUE(engine.field("clock"));

  Paper paper(80, 80);
  paper.begin(1, HALF_PI);
  engine.line(paper.pen, {10, 10}, {50, 10});
  paper.end();

  ASSERT_GT(recording->dabs.size(), 2u);
  EXPECT_NEAR(recording->dabs.back().position.fX, 10.0f, 1e-3f);
  EXPECT_NEAR(recording->dabs.back().position.fY, 50.0f, 1e-3f);
  EXPECT_NEAR(engine.position(paper.pen).angle(), HALF_PI, 1e-6f);
  EXPECT_NEAR(engine.position(5, 5).angle(), 0.0f, 1e-6f);
}

TEST(Engine, ShapeBuilderUsesTheSameSurfacePipeline) {
  brush::Engine engine;
  engine.noStroke();
  engine.wash(SkColors::kGreen, 1.0f);
  Paper paper(80, 80, SK_ColorWHITE);
  paper.begin();
  engine.beginShape();
  engine.vertex(15, 15);
  engine.vertex(65, 20);
  engine.vertex(40, 65);
  const std::optional<brush::PlacedPlot> shape = engine.endShape(paper.pen, true);
  ASSERT_TRUE(shape);
  EXPECT_EQ(shape->origin, SkPoint::Make(15, 15));
  engine.rect(paper.pen, 5, 5, 20, 15, 4);
  paper.end();
  EXPECT_EQ(paper.pixel(40, 35), SK_ColorGREEN);
}

TEST(Engine, AClosedShapesOutlineSitsOnItsBentInterior) {
  brush::Engine engine;
  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("recorder", recorder(recording, 3.0f)), nullptr);
  ASSERT_NE(engine.set("recorder", SkColors::kBlack), nullptr);
  engine.wash(SkColors::kGreen, 1.0f);
  ASSERT_TRUE(engine.addField("lean", [](SkPoint, float) { return 0.35f; }));
  ASSERT_TRUE(engine.field("lean"));

  Paper paper(160, 160, SK_ColorWHITE);
  paper.begin();
  engine.beginShape();
  engine.vertex(30, 30);
  engine.vertex(130, 40);
  engine.vertex(110, 130);
  engine.vertex(40, 120);
  ASSERT_TRUE(engine.endShape(paper.pen, true));
  paper.end();

  // Every outline dab has the wash on one side and the bare canvas on
  // the other: it sits on the interior's edge, bent the same way.
  const SkBitmap pixels = paper.pixels();
  ASSERT_GT(recording->dabs.size(), 10u);
  int onTheEdge = 0;
  for (const brush::Dab& dab : recording->dabs) {
    bool green = false;
    bool white = false;
    for (int dy = -3; dy <= 3; ++dy)
      for (int dx = -3; dx <= 3; ++dx) {
        const int x = (int)std::lround(dab.position.fX) + dx;
        const int y = (int)std::lround(dab.position.fY) + dy;
        if (x < 0 || y < 0 || x >= 160 || y >= 160) continue;
        const SkColor color = pixels.getColor(x, y);
        green |= color == SK_ColorGREEN;
        white |= color == SK_ColorWHITE;
      }
    onTheEdge += green && white;
  }
  EXPECT_EQ(onTheEdge, (int)recording->dabs.size());
}

TEST(Engine, ReturnsRelativePlotsPlacedWhereTheyWereDrawn) {
  brush::Engine engine;
  engine.noStroke();
  engine.wash(SkColors::kRed, 1.0f);
  Paper paper(120, 100, SK_ColorWHITE);
  paper.begin();

  const brush::Polygon polygon =
      engine.polygon(paper.pen, std::array<SkPoint, 3>{{{5, 5}, {20, 5}, {5, 20}}});
  EXPECT_FALSE(polygon.empty());
  const brush::PlacedPlot circle = engine.circle(paper.pen, 70, 50, 20);
  EXPECT_FALSE(circle.empty());
  EXPECT_GT(circle.plot.length(), 100.0f);
  EXPECT_EQ(circle.origin, SkPoint::Make(90, 50));
  const brush::Stroke replayed = circle.plot.path(circle.origin);
  EXPECT_NEAR(replayed.back().position.fX, 90.0f, 1e-3f);
  const brush::Stroke elsewhere = circle.plot.path({0, 0}, 1.0f, 0.5f, 0.5f);
  EXPECT_NEAR(elsewhere[24].position.fX, -10.0f, 1e-3f);
  EXPECT_NEAR(elsewhere[24].position.fY, 10.0f, 1e-3f);

  EXPECT_FALSE(engine.arc(paper.pen, 70, 50, 20, 0.5f, 0.5f));
  EXPECT_FALSE(engine.arc(paper.pen, 70, 50, 20, 0.0f, TWO_PI));
  const std::optional<brush::PlacedPlot> clockwise =
      engine.arc(paper.pen, 70, 50, 10, HALF_PI, 0.0f);
  ASSERT_TRUE(clockwise);
  EXPECT_NEAR(clockwise->plot.length(), 1.5f * PI * 10.0f, 0.1f);
  EXPECT_NEAR(clockwise->origin.fY, 60.0f, 1e-3f);
  const std::array<brush::Sample, 3> controls{
      {{{10, 80}, 0.2f}, {{45, 60}, 1.0f}, {{90, 82}, 0.4f}}};
  const brush::PlacedPlot curve = engine.spline(paper.pen, controls, 0.7f);
  EXPECT_FALSE(curve.empty());
  EXPECT_EQ(curve.origin, SkPoint::Make(10, 80));
  paper.end();

  const SkBitmap pixels = paper.pixels();
  EXPECT_EQ(pixels.getColor(70, 50), SK_ColorRED);
  EXPECT_NE(pixels.getColor(89, 50), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(94, 50), SK_ColorWHITE);
}

TEST(Engine, KeepsLiveStrokeStateAcrossInputEventBatches) {
  brush::Engine engine;
  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("live", recorder(recording, 10.0f)), nullptr);
  ASSERT_NE(engine.set("live", SkColors::kBlue), nullptr);

  Paper paper(100, 40);
  paper.begin();
  engine.beginInput(paper.pen, {.position = {10, 20}, .seconds = 0.0});
  EXPECT_TRUE(recording->dabs.empty());
  engine.moveInput(paper.pen, {.position = {30, 20}, .seconds = 0.01});
  engine.moveInput(paper.pen, {.position = {50, 20}, .seconds = 0.02});
  engine.endInput(paper.pen, {.position = {70, 20}, .seconds = 0.03});
  paper.end();

  EXPECT_EQ(recording->dabs.size(), 7u);
  EXPECT_FLOAT_EQ(recording->dabs.front().distance, 0.0f);
}

TEST(Engine, TheFirstLiveDabFollowsTheHeading) {
  brush::Engine engine;
  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("live", recorder(recording, 10.0f)), nullptr);
  ASSERT_NE(engine.set("live", SkColors::kBlue), nullptr);

  Paper paper(100, 100);
  paper.begin();
  engine.beginInput(paper.pen, {.position = {20, 20}, .seconds = 0.0});
  engine.moveInput(paper.pen, {.position = {20, 60}, .seconds = 0.05});
  engine.endInput(paper.pen, {.position = {20, 60}, .seconds = 0.06});
  paper.end();

  ASSERT_GE(recording->dabs.size(), 2u);
  EXPECT_EQ(recording->dabs.front().position, SkPoint::Make(20, 20));
  EXPECT_NEAR(recording->dabs.front().direction, HALF_PI, 1e-5f);
}

TEST(Engine, CancelInputDropsTheStroke) {
  brush::Engine engine;
  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("live", recorder(recording, 10.0f)), nullptr);
  ASSERT_NE(engine.set("live", SkColors::kBlue), nullptr);

  Paper paper(100, 40);
  paper.begin();
  engine.beginInput(paper.pen, {.position = {10, 20}, .seconds = 0.0});
  engine.cancelInput();
  engine.moveInput(paper.pen, {.position = {50, 20}, .seconds = 0.01});
  engine.endInput(paper.pen, {.position = {70, 20}, .seconds = 0.02});
  EXPECT_TRUE(recording->dabs.empty());

  engine.beginInput(paper.pen, {.position = {10, 20}, .seconds = 0.1});
  engine.endInput(paper.pen, {.position = {30, 20}, .seconds = 0.2});
  paper.end();
  EXPECT_EQ(recording->dabs.size(), 3u);
}

TEST(Engine, PopRestoresTheFieldTheClipAndTheInteriors) {
  brush::Engine engine;
  engine.push();
  ASSERT_TRUE(engine.field("waves"));
  engine.clip(SkRect::MakeWH(10, 10));
  engine.fill(SkColors::kRed);
  engine.hatch({.spacing = 3.0f});
  engine.pop();

  auto recording = std::make_shared<Recording>();
  ASSERT_NE(engine.add("recorder", recorder(recording, 2.0f)), nullptr);
  ASSERT_NE(engine.set("recorder", SkColors::kBlack), nullptr);
  Paper paper(80, 80, SK_ColorWHITE);
  paper.begin();
  engine.line(paper.pen, {10, 40}, {70, 40});
  const brush::Polygon square({{20, 20}, {60, 20}, {60, 60}, {20, 60}});
  engine.fill(paper.pen, square);
  engine.hatch(paper.pen, square);
  paper.end();

  // No field bent the line, no clip cut it, and no interior painted.
  ASSERT_GT(recording->dabs.size(), 2u);
  for (const brush::Dab& dab : recording->dabs)
    EXPECT_NEAR(dab.position.fY, 40.0f, 1e-4f);
  EXPECT_NEAR(recording->dabs.back().position.fX, 70.0f, 1e-3f);
  EXPECT_EQ(paper.pixel(40, 30), SK_ColorWHITE);
}

}  // namespace
