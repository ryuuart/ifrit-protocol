/** @file
 * The brush kit's pressure paths, direction fields and pen deposition.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSurface.h>
#include <sigildraw/Draw.h>
#include <sigildraw/kit/Brushwork.h>

#include <cmath>
#include <numbers>
#include <vector>

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;

TEST(BrushPressure, InterpolatesItsTwoHalves) {
  const brush::Pressure pressure{0.2f, 1.0f, 0.4f};
  EXPECT_FLOAT_EQ(pressure.at(0.0f), 0.2f);
  EXPECT_FLOAT_EQ(pressure.at(0.25f), 0.6f);
  EXPECT_FLOAT_EQ(pressure.at(0.5f), 1.0f);
  EXPECT_FLOAT_EQ(pressure.at(0.75f), 0.7f);
  EXPECT_FLOAT_EQ(pressure.at(1.0f), 0.4f);

  brush::Pressure custom;
  custom.curve = [](float progress) { return progress * progress; };
  EXPECT_FLOAT_EQ(custom.at(0.5f), 0.25f);

  const brush::Pressure gaussian =
      brush::Pressure::gaussianProfile(0.15f, 0.2f, 0.2f, 1.1f);
  EXPECT_NEAR(gaussian.at(0.5f), 1.1f, 1e-5f);
  EXPECT_LT(gaussian.at(0.0f), gaussian.at(0.5f));
}

TEST(BrushPath, SamplesSegmentsAndSplinesWithPressure) {
  const brush::Stroke line = brush::segment({0, 0}, {10, 0}, 2.0f, 0.25f, 1.0f);
  ASSERT_EQ(line.size(), 6u);
  EXPECT_EQ(line.front().position, SkPoint::Make(0, 0));
  EXPECT_EQ(line.back().position, SkPoint::Make(10, 0));
  EXPECT_FLOAT_EQ(line.front().pressure, 0.25f);
  EXPECT_FLOAT_EQ(line.back().pressure, 1.0f);

  const std::vector<brush::Sample> controls = {
      {{0, 0}, 0.4f}, {{10, 10}, 1.0f}, {{20, 0}, 0.6f}};
  const brush::Stroke curve = brush::spline(controls, 2.0f, 0.8f);
  ASSERT_GT(curve.size(), controls.size());
  EXPECT_EQ(curve.front().position, controls.front().position);
  EXPECT_EQ(curve.back().position, controls.back().position);
  EXPECT_FLOAT_EQ(curve.front().pressure, 0.4f);
  EXPECT_FLOAT_EQ(curve.back().pressure, 0.6f);
}

TEST(BrushSampler, CarriesSpacingAcrossUnevenInputEvents) {
  const std::vector<brush::Input> input = {
      {{0, 0}, 0.2f, 0, 0, 0.0},
      {{3, 0}, 0.4f, 0, 0, 0.01},
      {{11, 0}, 1.0f, 0, 0, 0.02},
  };
  const std::vector<brush::Dab> sampled = brush::dabs(input, 2.0f, 0.0f);
  ASSERT_EQ(sampled.size(), 7u);
  for (size_t i = 0; i < 6; ++i)
    EXPECT_NEAR(sampled[i].position.fX, (float)i * 2.0f, 1e-5f);
  EXPECT_FLOAT_EQ(sampled.back().position.fX, 11.0f);
  EXPECT_FLOAT_EQ(sampled.front().progress, 0.0f);
  EXPECT_FLOAT_EQ(sampled.back().progress, 1.0f);
  EXPECT_FLOAT_EQ(sampled.front().direction, sampled[1].direction);
  EXPECT_GT(sampled[2].speed, 0.0f);

  const std::array<brush::Input, 2> turning{{
      {.position = {0, 0},
       .barrelRotation = radians(170.0f),
       .seconds = 0.0,
       .tiltDirection = radians(170.0f)},
      {.position = {10, 0},
       .barrelRotation = radians(-170.0f),
       .seconds = 0.1,
       .tiltDirection = radians(-170.0f)},
  }};
  const std::vector<brush::Dab> turn = brush::dabs(turning, 5.0f, 0.0f);
  ASSERT_EQ(turn.size(), 3u);
  EXPECT_NEAR(std::abs(turn[1].barrelRotation), PI, 1e-5f);
  EXPECT_NEAR(std::abs(turn[1].tiltDirection), PI, 1e-5f);
}

TEST(BrushDabs, TiltCanDriveTipPositionSizeAspectAndDirection) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 120, .height = 80};
  pen.begin(*surface->getCanvas(), frame);

  brush::Brush tool = brush::marker(SkColors::kBlack, 12.0f);
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
  brush::deposit(pen, tool, marks);
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_NE(pixels.getColor(20, 40), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(70, 40), SK_ColorWHITE);
  EXPECT_NE(pixels.getColor(70, 52), SK_ColorWHITE);
}

TEST(BrushBox, OwnsTheFullStockNameSetAndScalesSpatialValues) {
  brush::Box box = brush::Box::stock();
  for (const char* name :
       {"2B", "HB", "2H", "cpencil", "pen", "rotring", "spray", "marker",
        "marker2", "charcoal", "hatch_brush", "pastel", "crayon"})
    EXPECT_TRUE(box.contains(name)) << name;
  EXPECT_EQ(box.find("HB")->tip, brush::Tip::Grain);
  EXPECT_EQ(box.find("marker")->tip, brush::Tip::Nib);
  EXPECT_FLOAT_EQ(box.find("marker")->width, 2.0f);
  EXPECT_FLOAT_EQ(box.find("marker")->spacing, 0.03f);
  EXPECT_FLOAT_EQ(box.find("charcoal")->width, 0.35f);
  EXPECT_FLOAT_EQ(box.find("charcoal")->spacing, 0.03f);
  const float before = box.find("HB")->width;
  const float scatter = box.find("HB")->scatter;
  box.scale(2.0f);
  EXPECT_FLOAT_EQ(box.find("HB")->width, before * 2.0f);
  EXPECT_FLOAT_EQ(box.find("HB")->scatter, scatter * 2.0f);
}

TEST(BrushUtilities, WeightedRandomUsesThePensDeterministicStream) {
  Pen pen;
  pen.randomSeed(74);
  EXPECT_EQ(brush::wRand<int>(pen, {{3, 0.0f}, {7, 1.0f}}), 7);
  EXPECT_FALSE(brush::wRand<int>(pen, {}).has_value());

  pen.randomSeed(74);
  const auto first = brush::wRand<int>(pen, {{1, 1.0f}, {2, 3.0f}});
  pen.randomSeed(74);
  const auto repeated = brush::wRand<int>(pen, {{1, 1.0f}, {2, 3.0f}});
  EXPECT_EQ(first, repeated);
}

TEST(BrushDabs, CustomTipReceivesEveryResampledDab) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 40));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 80, .height = 40};
  pen.begin(*surface->getCanvas(), frame);

  int calls = 0;
  brush::Brush tool = brush::marker(SkColors::kBlue, 8.0f);
  tool.tip = brush::Tip::Custom;
  tool.opacity = 1.0f;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  tool.sizeJitter = 0.0f;
  tool.opacityJitter = 0.0f;
  tool.spacingJitter = 0.0f;
  tool.markerTip = false;
  tool.customTip = [&calls](Pen& tip, const brush::Dab&) {
    ++calls;
    tip.noStroke();
    tip.rectMode(CENTER);
    tip.rect(0, 0, 1, 0.35f);
  };
  const std::vector<brush::Input> input = {{{10, 20}, 1, 0, 0, 0.0},
                                           {{70, 20}, 1, 0, 0, 0.1}};
  const std::vector<brush::Dab> sampled = brush::dabs(input, 10.0f);
  brush::deposit(pen, tool, sampled);
  pen.end();

  EXPECT_EQ(calls, (int)sampled.size());
  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_NE(pixels.getColor(40, 20), SK_ColorWHITE);
}

TEST(BrushDabs, ImageTipUsesDarkArtworkAsTheDefaultMask) {
  SkBitmap mask;
  mask.allocN32Pixels(8, 8, true);
  mask.eraseColor(SK_ColorWHITE);
  for (int y = 2; y < 6; ++y)
    for (int x = 2; x < 6; ++x) *mask.getAddr32(x, y) = SK_ColorBLACK;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 80, .height = 80};
  pen.begin(*surface->getCanvas(), frame);
  brush::Brush tool = brush::marker(SkColors::kRed, 32.0f);
  tool.tip = brush::Tip::Image;
  tool.imageTip = SkImages::RasterFromBitmap(mask);
  tool.opacity = 1.0f;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  tool.sizeJitter = 0.0f;
  tool.opacityJitter = 0.0f;
  tool.spacingJitter = 0.0f;
  tool.markerTip = false;
  const std::array<brush::Dab, 1> dabs{
      {{.position = {40, 40}, .pressure = 1.0f}}};
  brush::deposit(pen, tool, dabs);

  SkBitmap alphaMask;
  alphaMask.allocN32Pixels(8, 8, true);
  alphaMask.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 2; y < 6; ++y)
    for (int x = 2; x < 6; ++x) *alphaMask.getAddr32(x, y) = SK_ColorWHITE;
  tool.imageTip = SkImages::RasterFromBitmap(alphaMask);
  tool.imageMask = brush::ImageMask::Alpha;
  const std::array<brush::Dab, 1> alphaDabs{
      {{.position = {15, 40}, .pressure = 1.0f}}};
  brush::deposit(pen, tool, alphaDabs);
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(40, 40), SK_ColorRED);
  EXPECT_EQ(pixels.getColor(27, 27), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(15, 40), SK_ColorRED);
  EXPECT_EQ(pixels.getColor(2, 27), SK_ColorWHITE);
}

TEST(BrushEngine, OwnsSelectionAndRelativeStrokeState) {
  brush::Engine engine;
  EXPECT_FALSE(engine.hasStroke());
  EXPECT_TRUE(engine.set("HB", SkColors::kRed, 2.0f));
  EXPECT_TRUE(engine.hasStroke());
  EXPECT_EQ(engine.brush().color, SkColors::kRed);
  EXPECT_FLOAT_EQ(engine.brush().width,
                  brush::Box::stock().find("HB")->width * 2.0f);
  EXPECT_FALSE(engine.pick("missing"));
  engine.push();
  engine.noStroke();
  EXPECT_FALSE(engine.hasStroke());
  EXPECT_TRUE(engine.pop());
  EXPECT_TRUE(engine.hasStroke());
  EXPECT_FALSE(engine.pop());

  EXPECT_TRUE(engine.angleMode(DEGREES));
  EXPECT_EQ(engine.angleMode(), DEGREES);
  EXPECT_FALSE(engine.angleMode(CENTER));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 60));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 100, .height = 60};
  pen.begin(*surface->getCanvas(), frame);
  EXPECT_FALSE(engine.line(pen, {10, 10}, {10, 10}));
  engine.beginStroke(brush::StrokeKind::Segments, {10, 30});
  EXPECT_TRUE(engine.move(0.0f, 80.0f, 0.8f));
  const brush::Stroke result = engine.endStroke(pen, 0.0f, 0.5f);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result.back().position, SkPoint::Make(90, 30));
  EXPECT_FLOAT_EQ(result.back().pressure, 0.5f);

  engine.beginStroke(brush::StrokeKind::Curve, {10, 50});
  EXPECT_TRUE(engine.move(0.0f, 20.0f));
  const brush::Stroke curved = engine.endStroke(pen, 90.0f);
  pen.end();
  ASSERT_GE(curved.size(), 2u);
  EXPECT_LT(curved.back().position.fY, curved[curved.size() - 2].position.fY);
}

TEST(BrushEngine, ComposesFillHatchFieldAndClipAsOwnedState) {
  brush::Engine engine;
  EXPECT_FALSE(engine.refreshField());
  EXPECT_TRUE(engine.set("rotring", SkColors::kBlack, 1.0f));
  engine.fill(SkColors::kBlue, 0.5f);
  engine.noWash();
  EXPECT_TRUE(engine.hatchStyle("HB", SkColors::kRed, 1.0f));
  EXPECT_TRUE(engine.hatch({.spacing = 8.0f, .angle = 0.3f}));
  EXPECT_TRUE(engine.field("waves"));
  EXPECT_FALSE(engine.field("missing"));
  EXPECT_TRUE(engine.refreshField(0.5f));
  engine.clip(SkRect::MakeLTRB(20, 10, 80, 70));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 100, .height = 80};
  pen.begin(*surface->getCanvas(), frame);
  EXPECT_TRUE(engine.rect(pen, 5, 5, 90, 70));
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(0, 79), SK_ColorWHITE);
  EXPECT_NE(pixels.getColor(50, 40), SK_ColorWHITE);
  EXPECT_EQ(engine.listFields().size(), 7u);
}

TEST(BrushEngine, KeepsWatercolorAndFlatWashIndependent) {
  brush::Engine engine;
  engine.noStroke();
  engine.fill(SkColors::kRed, 0.4f);
  engine.wash(SkColors::kBlue, 0.6f);
  const brush::Polygon polygon({{8, 8}, {56, 8}, {56, 56}, {8, 56}});

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 64));
  Pen pen;
  Frame frame{.width = 64, .height = 64};
  pen.begin(*surface->getCanvas(), frame);
  EXPECT_TRUE(engine.fill(pen, polygon));
  EXPECT_TRUE(engine.wash(pen, polygon));
  engine.noWash();
  EXPECT_TRUE(engine.fill(pen, polygon));
  EXPECT_FALSE(engine.wash(pen, polygon));
  engine.wash(SkColors::kBlue, 0.6f);
  engine.noFill();
  EXPECT_FALSE(engine.fill(pen, polygon));
  EXPECT_TRUE(engine.wash(pen, polygon));
  pen.end();
}

TEST(BrushEngine, HatchUsesTheCurrentBrushUntilOverridden) {
  brush::Engine engine;
  std::vector<SkColor4f> colors;
  brush::Brush recorder = brush::marker(SkColors::kBlack, 2.0f);
  recorder.tip = brush::Tip::Custom;
  recorder.spacing = 4.0f;
  recorder.scatter = 0.0f;
  recorder.markerTip = false;
  recorder.pressure = {1, 1, 1};
  recorder.pressure.variation.reset();
  recorder.customTip = [&colors](Pen& pen, const brush::Dab&) {
    if (const SkPaint* paint = pen.fillPaint())
      colors.push_back(paint->getColor4f());
  };
  ASSERT_TRUE(engine.add("recorder", recorder));
  ASSERT_TRUE(engine.set("recorder", SkColors::kBlue));
  ASSERT_TRUE(engine.hatch({.spacing = 12.0f, .angle = 0.0f}));
  const brush::Polygon polygon({{5, 5}, {55, 5}, {55, 55}, {5, 55}});

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 64));
  Pen pen;
  Frame frame{.width = 64, .height = 64};
  pen.begin(*surface->getCanvas(), frame);
  ASSERT_TRUE(engine.hatch(pen, polygon));
  ASSERT_FALSE(colors.empty());
  EXPECT_FLOAT_EQ(colors.back().fB, 1.0f);

  colors.clear();
  ASSERT_TRUE(engine.hatchStyle("recorder", SkColors::kRed));
  ASSERT_TRUE(engine.hatch(pen, polygon));
  ASSERT_FALSE(colors.empty());
  EXPECT_FLOAT_EQ(colors.back().fR, 1.0f);

  colors.clear();
  engine.noHatch();
  ASSERT_TRUE(engine.hatch({.spacing = 12.0f, .angle = 0.0f}));
  ASSERT_TRUE(engine.hatch(pen, polygon));
  ASSERT_FALSE(colors.empty());
  EXPECT_FLOAT_EQ(colors.back().fB, 1.0f);
  pen.end();
}

TEST(BrushEngine, CapturesTheClipTransformAtTheCall) {
  brush::Engine engine;
  brush::Brush solid = brush::marker(SkColors::kBlue, 4.0f);
  solid.opacity = 1.0f;
  solid.scatter = 0.0f;
  solid.markerTip = false;
  solid.pressure = {1, 1, 1};
  solid.pressure.variation.reset();
  ASSERT_TRUE(engine.add("solid", solid));
  ASSERT_TRUE(engine.set("solid", SkColors::kBlue));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 40));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 80, .height = 40};
  pen.begin(*surface->getCanvas(), frame);
  pen.translate(20, 0);
  engine.clip(pen, SkRect::MakeXYWH(0, 0, 20, 40));
  pen.resetMatrix();
  ASSERT_TRUE(engine.line(pen, {0, 20}, {80, 20}));
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(10, 20), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(30, 20), SK_ColorBLUE);
  EXPECT_EQ(pixels.getColor(50, 20), SK_ColorWHITE);
}

TEST(BrushEngine, IntegratesGeometryThroughTheSelectedField) {
  brush::Engine engine;
  std::vector<SkPoint> deposited;
  brush::Brush recorder = brush::marker(SkColors::kBlack, 1.0f);
  recorder.tip = brush::Tip::Custom;
  recorder.spacing = 2.0f;
  recorder.scatter = 0.0f;
  recorder.markerTip = false;
  recorder.pressure = {1, 1, 1};
  recorder.customTip = [&deposited](Pen&, const brush::Dab& dab) {
    deposited.push_back(dab.position);
  };
  ASSERT_TRUE(engine.add("recorder", recorder));
  ASSERT_TRUE(engine.set("recorder", SkColors::kBlack));
  ASSERT_TRUE(
      engine.addField("quarter-turn", [](SkPoint, float) { return HALF_PI; }));
  ASSERT_TRUE(engine.field("quarter-turn"));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 80));
  Pen pen;
  Frame frame{.width = 80, .height = 80};
  pen.begin(*surface->getCanvas(), frame);
  EXPECT_TRUE(engine.line(pen, {10, 10}, {50, 10}));
  pen.end();

  ASSERT_GT(deposited.size(), 2u);
  EXPECT_NEAR(deposited.back().fX, 10.0f, 1e-3f);
  EXPECT_NEAR(deposited.back().fY, 50.0f, 1e-3f);
}

TEST(BrushEngine, ShapeBuilderUsesTheSameSurfacePipeline) {
  brush::Engine engine;
  engine.noStroke();
  engine.wash(SkColors::kGreen, 1.0f);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 80, .height = 80};
  pen.begin(*surface->getCanvas(), frame);
  engine.beginShape();
  engine.vertex(15, 15);
  engine.vertex(65, 20);
  engine.vertex(40, 65);
  EXPECT_TRUE(engine.endShape(pen, true));
  EXPECT_TRUE(engine.rect(pen, 5, 5, 20, 15, 4));
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(40, 35), SK_ColorGREEN);
}

TEST(BrushEngine, ReturnsReusableGeometryAndUsesRadiusForCircles) {
  brush::Engine engine;
  engine.noStroke();
  engine.wash(SkColors::kRed, 1.0f);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 100));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 120, .height = 100};
  pen.begin(*surface->getCanvas(), frame);

  const brush::Polygon polygon =
      engine.polygon(pen, std::array<SkPoint, 3>{{{5, 5}, {20, 5}, {5, 20}}});
  EXPECT_TRUE(polygon);
  const brush::PlacedPlot circle = engine.circle(pen, 70, 50, 20);
  EXPECT_TRUE(circle);
  EXPECT_GT(circle.plot.length(), 100.0f);
  EXPECT_FALSE(engine.arc(pen, 70, 50, 20, 0.5f, 0.5f));
  EXPECT_FALSE(engine.arc(pen, 70, 50, 20, 0.0f, TWO_PI));
  const std::optional<brush::Plot> clockwise =
      engine.arc(pen, 70, 50, 10, HALF_PI, 0.0f);
  ASSERT_TRUE(clockwise);
  EXPECT_NEAR(clockwise->length(), 1.5f * PI * 10.0f, 0.1f);
  const std::array<brush::Sample, 3> controls{
      {{{10, 80}, 0.2f}, {{45, 60}, 1.0f}, {{90, 82}, 0.4f}}};
  const brush::Plot curve = engine.spline(pen, controls, 0.7f);
  EXPECT_TRUE(curve);
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(70, 50), SK_ColorRED);
  EXPECT_NE(pixels.getColor(89, 50), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(94, 50), SK_ColorWHITE);
}

TEST(BrushEngine, KeepsLiveStrokeStateAcrossInputEventBatches) {
  brush::Engine engine;
  int calls = 0;
  brush::Brush custom = brush::marker(SkColors::kBlue, 6.0f);
  custom.tip = brush::Tip::Custom;
  custom.spacing = 10.0f;
  custom.scatter = 0.0f;
  custom.pressure = {1, 1, 1};
  custom.markerTip = false;
  custom.customTip = [&calls](Pen&, const brush::Dab&) { ++calls; };
  ASSERT_TRUE(engine.add("live", custom));
  ASSERT_TRUE(engine.set("live", SkColors::kBlue));

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 40));
  Pen pen;
  Frame frame{.width = 100, .height = 40};
  pen.begin(*surface->getCanvas(), frame);
  EXPECT_TRUE(engine.beginInput(pen, {{10, 20}, 1, 0, 0, 0.0}));
  EXPECT_TRUE(engine.moveInput(pen, {{30, 20}, 1, 0, 0, 0.01}));
  EXPECT_TRUE(engine.moveInput(pen, {{50, 20}, 1, 0, 0, 0.02}));
  EXPECT_TRUE(engine.endInput(pen, {{70, 20}, 1, 0, 0, 0.03}));
  pen.end();

  EXPECT_EQ(calls, 7);
}

TEST(BrushGeometry, PolygonIntersectsAndTranslates) {
  const brush::Polygon polygon({{10, 10}, {50, 10}, {50, 40}, {10, 40}});
  const std::vector<SkPoint> hits = polygon.intersect({{0, 25}, {80, 25}});
  ASSERT_EQ(hits.size(), 2u);
  EXPECT_EQ(hits[0], SkPoint::Make(10, 25));
  EXPECT_EQ(hits[1], SkPoint::Make(50, 25));
  const brush::Polygon moved = polygon.translated(4, -3);
  EXPECT_EQ(moved.vertices.front(), SkPoint::Make(14, 7));
  EXPECT_EQ(moved.sides.size(), moved.vertices.size());
}

TEST(BrushGeometry, PlotAndPositionRemainReusableValues) {
  brush::Plot plot(brush::PlotType::Segments);
  plot.addSegment(0.0f, 20.0f, 0.4f);
  plot.addSegment(HALF_PI, 10.0f, 0.8f);
  plot.endPlot(HALF_PI, 0.2f);
  brush::Stroke path = plot.path({5, 6});
  ASSERT_EQ(path.size(), 3u);
  EXPECT_NEAR(path.back().position.fX, 25.0f, 1e-5f);
  EXPECT_NEAR(path.back().position.fY, -4.0f, 1e-5f);
  EXPECT_FLOAT_EQ(path.back().pressure, 0.2f);

  plot.rotate(HALF_PI);
  path = plot.path({0, 0});
  EXPECT_NEAR(path[1].position.fX, 0.0f, 1e-5f);
  EXPECT_NEAR(path[1].position.fY, -20.0f, 1e-5f);
  const brush::Stroke scaled = plot.path({0, 0}, 1.0f, 0.5f, 2.0f);
  EXPECT_NEAR(scaled[1].position.fY, -40.0f, 1e-5f);

  brush::Position cursor(2, 3);
  const brush::Stroke moved = cursor.moveTo(0.0f, 9.0f, 2.0f);
  EXPECT_EQ(moved.size(), 6u);
  EXPECT_FLOAT_EQ(cursor.x, 11.0f);
  EXPECT_FLOAT_EQ(cursor.plotted, 9.0f);
  cursor.reset();
  EXPECT_FLOAT_EQ(cursor.plotted, 0.0f);

  brush::Engine engine;
  ASSERT_TRUE(engine.addField("down", [](SkPoint, float) { return HALF_PI; }));
  ASSERT_TRUE(engine.field("down"));
  brush::Position fieldCursor = engine.position(3, 4);
  const brush::Stroke firstMove = fieldCursor.moveTo(0.0f, 8.0f, 2.0f);
  const brush::Stroke secondMove = fieldCursor.moveTo(0.0f, 4.0f, 2.0f);
  EXPECT_GT(firstMove.size(), 1u);
  EXPECT_GT(secondMove.size(), 1u);
  EXPECT_NEAR(fieldCursor.x, 3.0f, 1e-4f);
  EXPECT_NEAR(fieldCursor.y, 16.0f, 1e-4f);
  EXPECT_NEAR(fieldCursor.angle(), HALF_PI, 1e-6f);

  brush::Plot wrapped(brush::PlotType::Curve);
  wrapped.addSegment(radians(170.0f), 10.0f);
  wrapped.endPlot(radians(-170.0f));
  EXPECT_NEAR(std::abs(wrapped.angle(5.0f)), PI, 1e-5f);
  wrapped.rotate(90.0f, DEGREES);
  EXPECT_NEAR(std::remainder(wrapped.angle(0.0f), TWO_PI), radians(-100.0f),
              1e-5f);
  wrapped.rotate(180.0f, DEGREES);
  EXPECT_NEAR(std::remainder(wrapped.angle(0.0f), TWO_PI), radians(-10.0f),
              1e-5f);

  const std::array<brush::Sample, 2> absolute{
      {{{4, 7}, 1.0f}, {{14, 7}, 1.0f}}};
  const brush::Plot placed =
      brush::Plot::fromStroke(absolute, brush::PlotType::Segments);
  const brush::Stroke fixedOrigin = placed.path({50, 50}, 1.0f, 0.5f, 3.0f);
  EXPECT_EQ(fixedOrigin.front().position, SkPoint::Make(4, 7));
  EXPECT_NEAR(fixedOrigin.back().position.fX, 14.0f, 1e-5f);

  brush::Engine degreeField;
  ASSERT_TRUE(degreeField.addField(
      "down-degrees", [](SkPoint, float) { return 90.0f; }, DEGREES));
  ASSERT_TRUE(degreeField.field("down-degrees"));
  EXPECT_NEAR(degreeField.position().angle(), HALF_PI, 1e-6f);

  ASSERT_TRUE(degreeField.angleMode(DEGREES));
  brush::Position degreeCursor = degreeField.position(4, 5);
  degreeCursor.field({}, 0.0f);
  const brush::Stroke degreeMove = degreeCursor.moveTo(90.0f, 10.0f, 2.0f);
  EXPECT_GT(degreeMove.size(), 1u);
  EXPECT_NEAR(degreeCursor.x, 4.0f, 1e-5f);
  EXPECT_NEAR(degreeCursor.y, -5.0f, 1e-5f);

  brush::Plot degreePlot = degreeField.plot(brush::PlotType::Segments);
  degreePlot.addSegment(90.0f, 10.0f);
  degreePlot.endPlot(90.0f);
  const brush::Stroke degreePath = degreePlot.path();
  EXPECT_NEAR(degreePath.back().position.fX, 0.0f, 1e-5f);
  EXPECT_NEAR(degreePath.back().position.fY, -10.0f, 1e-5f);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 80));
  Pen pen;
  Frame frame{.width = 100, .height = 80};
  pen.begin(*surface->getCanvas(), frame);
  brush::Position bounded = engine.position(pen, 10, 10);
  EXPECT_TRUE(bounded.isIn());
  bounded.update(-60, 10);
  EXPECT_FALSE(bounded.isInCanvas());
  const brush::Stroke stopped = bounded.moveTo(0.0f, 20.0f);
  EXPECT_EQ(stopped.size(), 1u);
  EXPECT_FLOAT_EQ(bounded.x, -60.0f);
  EXPECT_FLOAT_EQ(bounded.plotted, 1.0f);

  brush::Plot scaledPlot(brush::PlotType::Segments);
  scaledPlot.addSegment(0.0f, 20.0f);
  scaledPlot.endPlot(0.0f);
  brush::Position plotting(10, 10);
  const brush::Stroke plotted = plotting.plotTo(scaledPlot, 10.0f, 2.0f, 2.0f);
  EXPECT_GT(plotted.size(), 1u);
  EXPECT_FLOAT_EQ(plotting.x, 20.0f);
  EXPECT_FLOAT_EQ(plotting.plotted, 5.0f);
  pen.end();
}

TEST(BrushGeometry, ArrayEffectsUseInnerPolygonsAsEvenOddHoles) {
  const std::array<brush::Polygon, 2> polygons{
      brush::Polygon({{10, 10}, {90, 10}, {90, 90}, {10, 90}}),
      brush::Polygon({{35, 35}, {65, 35}, {65, 65}, {35, 65}})};
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 100));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 100, .height = 100};
  pen.begin(*surface->getCanvas(), frame);
  pen.randomSeed(22);
  brush::Brush tool = brush::marker(SkColors::kBlack, 2.0f);
  tool.opacity = 1.0f;
  tool.bristles = 1;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  brush::hatchArray(pen, tool, polygons, {.spacing = 5.0f, .angle = 0.0f});
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  int outerInk = 0;
  int holeInk = 0;
  for (int y = 15; y < 85; ++y) {
    for (int x = 15; x < 85; ++x) {
      if (pixels.getColor(x, y) == SK_ColorWHITE) continue;
      if (x > 37 && x < 63 && y > 37 && y < 63)
        ++holeInk;
      else
        ++outerInk;
    }
  }
  EXPECT_GT(outerInk, 100);
  EXPECT_EQ(holeInk, 0);

  const std::array<brush::Polygon, 2> islands{
      brush::Polygon({{5, 5}, {30, 5}, {30, 30}, {5, 30}}),
      brush::Polygon({{70, 70}, {95, 70}, {95, 95}, {70, 95}})};
  surface->getCanvas()->clear(SK_ColorWHITE);
  pen.begin(*surface->getCanvas(), frame);
  pen.randomSeed(22);
  brush::hatchArray(pen, tool, islands, {.spacing = 3.0f, .angle = HALF_PI});
  pen.end();
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  bool secondIslandPainted = false;
  for (int y = 70; y < 95; ++y)
    for (int x = 70; x < 95; ++x)
      secondIslandPainted |= pixels.getColor(x, y) != SK_ColorWHITE;
  EXPECT_TRUE(secondIslandPainted);
}

TEST(BrushFields, TraceFollowsAnyCallableDirectionValue) {
  const brush::fields::Wave horizontal{.direction = 0.0f, .amplitude = 0.0f};
  const brush::Stroke path =
      brush::trace(SkPoint::Make(2, 3), 12.0f, 2.0f, 0.0f, horizontal);
  ASSERT_EQ(path.size(), 7u);
  EXPECT_NEAR(path.back().position.fX, 14.0f, 1e-5f);
  EXPECT_NEAR(path.back().position.fY, 3.0f, 1e-5f);

  const brush::fields::Vortex clockwise{.center = {0, 0}};
  EXPECT_NEAR(clockwise({10, 0}, 0), std::numbers::pi_v<float> * 0.5f, 1e-6f);
  const brush::fields::Curl a(42), b(42);
  EXPECT_EQ(a, b);
  EXPECT_FLOAT_EQ(a({14, 28}, 0.5f), b({14, 28}, 0.5f));
}

TEST(BrushPaint, DepositsPigmentAndRestoresThePenStyle) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 80));
  surface->getCanvas()->clear(SK_ColorWHITE);

  Pen pen;
  Frame frame;
  frame.width = 120;
  frame.height = 80;
  pen.begin(*surface->getCanvas(), frame);
  pen.randomSeed(7);
  pen.noFill();
  pen.stroke(255, 0, 0);
  pen.strokeWeight(3);

  brush::Brush tool = brush::marker(SkColors::kBlue, 10.0f);
  tool.opacity = 1.0f;
  tool.scatter = 0.0f;
  tool.bristles = 1;
  tool.pressure = {1, 1, 1};
  brush::line(pen, tool, {10, 20}, {110, 20});
  pen.line(10, 60, 110, 60);
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(60, 20), SK_ColorBLUE);
  EXPECT_EQ(pixels.getColor(60, 60), SK_ColorRED);
}

TEST(BrushPaint, RecordsRoundDabsAsBoundedSpriteBatches) {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(420, 80);
  Pen pen;
  pen.begin(*canvas, {.width = 420, .height = 80});
  pen.randomSeed(17);
  brush::Brush tool = brush::charcoal(SkColors::kBlack, 0.35f);
  tool.spacing = 0.03f;
  brush::line(pen, tool, {10, 40}, {410, 40});
  pen.end();
  const sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

  ASSERT_TRUE(picture);
  EXPECT_LT(picture->approximateOpCount(), 16);
}

TEST(BrushShape, HatchingStaysInsideItsPolygon) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 100));
  surface->getCanvas()->clear(SK_ColorWHITE);

  Pen pen;
  Frame frame;
  frame.width = 120;
  frame.height = 100;
  pen.begin(*surface->getCanvas(), frame);
  pen.randomSeed(9);
  brush::Brush tool = brush::marker(SkColors::kBlack, 2.5f);
  tool.opacity = 1.0f;
  tool.bristles = 1;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  const std::vector<SkPoint> polygon = {{25, 20}, {98, 28}, {88, 82}, {18, 72}};
  brush::hatch(pen, tool, polygon, {.spacing = 7.0f, .angle = 0.35f});
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  int interiorInk = 0;
  int distantInk = 0;
  for (int y = 0; y < pixels.height(); ++y) {
    for (int x = 0; x < pixels.width(); ++x) {
      if (pixels.getColor(x, y) == SK_ColorWHITE) continue;
      if (x > 30 && x < 80 && y > 30 && y < 65) ++interiorInk;
      if (x < 8 || x > 112 || y < 8 || y > 92) ++distantInk;
    }
  }
  EXPECT_GT(interiorInk, 100);
  EXPECT_EQ(distantInk, 0);
}

TEST(BrushShape, WashBuildsATexturedTranslucentInterior) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 100));
  surface->getCanvas()->clear(SK_ColorWHITE);

  Pen pen;
  Frame frame;
  frame.width = 100;
  frame.height = 100;
  pen.begin(*surface->getCanvas(), frame);
  pen.randomSeed(12);
  pen.noiseSeed(12);
  const std::vector<SkPoint> polygon = {{24, 22}, {78, 27}, {82, 75}, {20, 80}};
  brush::wash(pen,
              {.color = SkColors::kBlue,
               .opacity = 0.5f,
               .bleed = 0.3f,
               .texture = 0.4f,
               .border = 0.3f,
               .layers = 12},
              polygon);
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_NE(pixels.getColor(50, 50), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(2, 2), SK_ColorWHITE);
}

TEST(BrushShape, MassStaysInsideItsSurfaceAndWarpClosesItsPath) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 100));
  surface->getCanvas()->clear(SK_ColorWHITE);
  Pen pen;
  Frame frame{.width = 120, .height = 100};
  pen.begin(*surface->getCanvas(), frame);
  pen.randomSeed(88);
  const std::vector<SkPoint> polygon = {{30, 20}, {94, 28}, {86, 78}, {24, 70}};
  brush::Brush tool = brush::pencil(SkColors::kBlack, 2.0f);
  brush::mass(pen, tool, polygon,
              {.precision = 0.35f,
               .strength = 0.4f,
               .gradient = 0.2f,
               .outline = false});
  pen.end();

  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(pixels.pixmap(), 0, 0));
  EXPECT_NE(pixels.getColor(60, 50), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(4, 4), SK_ColorWHITE);

  const brush::fields::Wave field{.direction = 0.0f, .amplitude = 0.0f};
  const brush::Stroke warped = brush::warp(polygon, 7.0f, 5.0f, 0.0f, field);
  ASSERT_GT(warped.size(), polygon.size());
  EXPECT_EQ(warped.front(), warped.back());
  EXPECT_NEAR(warped.front().position.fX, polygon.front().fX + 5.0f, 1e-5f);
}

}  // namespace
