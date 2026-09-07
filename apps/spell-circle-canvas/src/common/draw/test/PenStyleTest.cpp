/** @file
 * What the pen draws WITH: the colour arguments, the blend modes, the
 * clip, the dash, a material as a fill and the paint a no-fill pen
 * hands out — and the shader table those materials are compiled from.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <sigildraw/Draw.h>
#include <sigilshaders/Draw.h>

#include <cmath>
#include <memory>
#include <vector>

#include "ShaderTable.h"
#include "support/Paper.h"

namespace {

using namespace sigil::draw;
using sigil::draw::testing::Paper;

TEST(Pen, StyleSurvivesFramesAndTheTransformDoesNot) {
  Paper paper;
  paper.begin(1);
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.translate(50, 50);
  paper.end();
  paper.begin(2);
  paper.pen.rect(0, 0, 10, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(paper.pixel(55, 55), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pen.strokePaint(), nullptr);
}

TEST(Pen, BackgroundCoversTheCanvasWhateverTheTransform) {
  Paper paper;
  paper.begin();
  paper.pen.translate(50, 50);
  paper.pen.scale(0.1f);
  paper.pen.background(255);
  paper.end();
  EXPECT_EQ(paper.pixel(2, 2), SK_ColorWHITE);
  EXPECT_EQ(paper.pixel(97, 97), SK_ColorWHITE);
}

TEST(Pen, ColourArgumentsReadAsP5Reads) {
  Pen pen;
  EXPECT_EQ(pen.color(255, 0, 0).toSkColor(), SK_ColorRED);
  EXPECT_EQ(pen.color("#00ff00").toSkColor(), SK_ColorGREEN);
  EXPECT_EQ(pen.color("#00f").toSkColor(), SK_ColorBLUE);
  EXPECT_EQ(pen.color("blue").toSkColor(), SK_ColorBLUE);
  EXPECT_EQ(pen.color(128).toSkColor(), SkColorSetARGB(255, 128, 128, 128));
  EXPECT_EQ(pen.color(0, 51).toSkColor(), SkColorSetARGB(51, 0, 0, 0));
  pen.colorMode(HSB);
  EXPECT_EQ(pen.color(0, 100, 100).toSkColor(), SK_ColorRED);
  EXPECT_EQ(pen.color(120, 100, 100).toSkColor(), SK_ColorGREEN);
  EXPECT_EQ(pen.color(240, 100, 100).toSkColor(), SK_ColorBLUE);
  pen.colorMode(RGB, 1);
  EXPECT_EQ(pen.color(1, 1, 1).toSkColor(), SK_ColorWHITE);
}

TEST(Pen, BlendModeAddPutsLightTogetherAndClamps) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(200, 0, 40);
  paper.pen.rect(10, 10, 40, 40);
  paper.pen.blendMode(ADD);
  paper.pen.fill(100, 0, 80);
  paper.pen.rect(10, 10, 40, 40);
  paper.end();
  const SkColor lit = paper.pixel(30, 30);
  EXPECT_EQ(SkColorGetR(lit), 255u);  // 200 + 100, clamped
  EXPECT_EQ(SkColorGetB(lit), 120u);  // 40 + 80
}

TEST(Pen, BlendModeReplaceOverwritesAlphaAndAll) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(10, 10, 40, 40);
  paper.pen.blendMode(REPLACE);
  paper.pen.fill(0, 0, 255, 128);
  paper.pen.rect(10, 10, 40, 40);
  paper.end();
  const SkColor over = paper.pixel(30, 30);
  // Laid over, this would be an opaque purple; replaced, the source's
  // own half alpha is what stands.
  EXPECT_EQ(SkColorGetA(over), 128u);
  EXPECT_EQ(SkColorGetR(over), 0u);
}

TEST(Pen, BlendModeRemoveTakesThePixelsAway) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(10, 10, 40, 40);
  paper.pen.blendMode(REMOVE);
  paper.pen.fill(0, 255, 0);
  paper.pen.rect(10, 10, 20, 20);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(20, 20)), 0u);
  EXPECT_EQ(paper.pixel(40, 40), SK_ColorRED);
}

TEST(Pen, BlendModeSubtractTakesLightAwayAndKeepsTheAlpha) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(200, 200, 200);
  paper.pen.rect(10, 10, 40, 40);
  paper.pen.blendMode(SUBTRACT);
  paper.pen.fill(50, 0, 0);
  paper.pen.rect(10, 10, 40, 40);
  paper.end();
  const SkColor dark = paper.pixel(30, 30);
  EXPECT_NEAR(SkColorGetR(dark), 150u, 1u);
  EXPECT_NEAR(SkColorGetG(dark), 200u, 1u);
  EXPECT_EQ(SkColorGetA(dark), 255u);
}

TEST(Pen, BlendModeReachesTheMeshTheImageAndTheGround) {
  // The per-corner mesh: one fill on each end of a triangle sends the
  // shape down the vertices route, which has a paint of its own.
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(120, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.pen.blendMode(ADD);
  paper.pen.beginShape(TRIANGLES);
  paper.pen.fill(60, 0, 0);
  paper.pen.vertex(10, 10);
  paper.pen.vertex(90, 10);
  paper.pen.fill(60, 0, 0);
  paper.pen.vertex(50, 90);
  paper.pen.endShape();
  paper.end();
  EXPECT_EQ(SkColorGetR(paper.pixel(50, 40)), 180u);

  // The image: the same source put down twice adds to itself.
  Paper sheet;
  sheet.begin();
  sheet.pen.noStroke();
  sheet.pen.fill(70, 0, 0);
  sheet.pen.rect(0, 0, 100, 100);
  sheet.end();
  const sk_sp<SkImage> stamp = sheet.surface->makeImageSnapshot();

  Paper page;
  page.begin();
  page.pen.image(stamp, 0, 0, 100, 100);
  page.pen.blendMode(ADD);
  page.pen.image(stamp, 0, 0, 100, 100);
  page.end();
  EXPECT_EQ(SkColorGetR(page.pixel(50, 50)), 140u);

  // The ground a background lays.
  Paper ground;
  ground.begin();
  ground.pen.background(30, 0, 0);
  ground.pen.blendMode(ADD);
  ground.pen.background(30, 0, 0);
  ground.end();
  EXPECT_EQ(SkColorGetR(ground.pixel(50, 50)), 60u);
}

TEST(Pen, BlendModeIsStyleSoPushAndPopCarryIt) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(100, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.pen.push();
  paper.pen.blendMode(ADD);
  paper.pen.fill(50, 0, 0);
  paper.pen.rect(0, 0, 50, 100);
  paper.pen.pop();
  // Back under BLEND, an opaque fill covers rather than adds.
  paper.pen.fill(50, 0, 0);
  paper.pen.rect(50, 0, 50, 100);
  paper.end();
  EXPECT_EQ(SkColorGetR(paper.pixel(25, 50)), 150u);
  EXPECT_EQ(SkColorGetR(paper.pixel(75, 50)), 50u);
}

TEST(Pen, ClipKeepsOnlyWhatTheShapeCovered) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.clip([&] { paper.pen.rect(20, 20, 40, 40); });
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.end();
  EXPECT_EQ(paper.pixel(30, 30), SK_ColorRED);
  EXPECT_EQ(paper.pixel(55, 55), SK_ColorRED);
  EXPECT_EQ(SkColorGetA(paper.pixel(10, 10)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(30, 80)), 0u);
}

TEST(Pen, NothingTheClipShapeDrawsLandsOnTheCanvas) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.clip([&] {
    paper.pen.fill(0, 255, 0);
    paper.pen.rect(20, 20, 40, 40);
    paper.pen.line(0, 0, 100, 100);
    paper.pen.text("mask", 4, 90);
  });
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(30, 30)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(5, 5)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(10, 86)), 0u);
}

TEST(Pen, ClipInvertedCutsTheShapeOut) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.clip([&] { paper.pen.rect(20, 20, 40, 40); }, {.invert = true});
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(40, 40)), 0u);
  EXPECT_EQ(paper.pixel(10, 10), SK_ColorRED);
}

TEST(Pen, ClipLastsUntilTheMatchingPop) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.push();
  paper.pen.clip([&] { paper.pen.rect(0, 0, 50, 100); });
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.pen.pop();
  paper.pen.fill(0, 0, 255);
  paper.pen.rect(60, 0, 40, 100);
  paper.end();
  EXPECT_EQ(paper.pixel(25, 50), SK_ColorRED);      // inside the mask
  EXPECT_EQ(SkColorGetA(paper.pixel(55, 50)), 0u);  // outside it
  EXPECT_EQ(paper.pixel(70, 50), SK_ColorBLUE);     // after the pop
}

TEST(Pen, TheMaskCarriesTheTransformItWasDrawnUnder) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.clip([&] {
    paper.pen.push();
    paper.pen.translate(50, 50);
    paper.pen.rect(0, 0, 20, 20);
    paper.pen.pop();
  });
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 100, 100);
  paper.end();
  EXPECT_EQ(paper.pixel(55, 55), SK_ColorRED);
  EXPECT_EQ(SkColorGetA(paper.pixel(10, 10)), 0u);
}

/** A pen set up to draw crisp two-pixel strokes, so a dash reads as
 *  whole inked and blank pixels. */
void dashPen(Pen& pen) {
  pen.noSmooth();
  pen.noFill();
  pen.stroke(0);
  pen.strokeWeight(2);
  pen.strokeCap(SQUARE);
}

TEST(Pen, StrokeDashBreaksTheStrokeIntoItsRun) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.strokeDash({4, 4});
  paper.pen.line(10, 50, 90, 50);
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(11, 50)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(15, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(19, 50)), 0u);
}

TEST(Pen, AnOddDashRunRepeatsItself) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.strokeDash({4});
  paper.pen.line(10, 50, 90, 50);
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(11, 50)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(15, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(19, 50)), 0u);
}

TEST(Pen, TheDashPhaseStartsTheRunPartwayIn) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.strokeDash({4, 4}, 4);
  paper.pen.line(10, 50, 90, 50);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(11, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(15, 50)), 0u);
}

TEST(Pen, ABeginShapeOutlineWearsTheDash) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.strokeDash({4, 4});
  paper.pen.beginShape();
  paper.pen.vertex(20, 20);
  paper.pen.vertex(80, 20);
  paper.pen.vertex(80, 80);
  paper.pen.vertex(20, 80);
  paper.pen.endShape(CLOSE);
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(21, 20)), 0u);
  EXPECT_EQ(SkColorGetA(paper.pixel(25, 20)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(29, 20)), 0u);
}

TEST(Pen, APointIsADiscAndNeverDashed) {
  Paper paper;
  paper.begin();
  paper.pen.noSmooth();
  paper.pen.stroke(0);
  paper.pen.strokeWeight(10);
  paper.pen.strokeDash({2, 2});
  paper.pen.point(50, 50);
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(50, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(52, 52)), 0u);
}

TEST(Pen, NoDashPutsTheSolidStrokeBackAndPushPopCarriesIt) {
  Paper paper;
  paper.begin();
  dashPen(paper.pen);
  paper.pen.push();
  paper.pen.strokeDash({4, 4});
  paper.pen.line(10, 30, 90, 30);
  paper.pen.pop();
  // Outside the push the stroke was never dashed.
  paper.pen.line(10, 50, 90, 50);
  paper.pen.strokeDash({4, 4});
  paper.pen.noDash();
  paper.pen.line(10, 70, 90, 70);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(15, 30)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(15, 50)), 0u);
  EXPECT_GT(SkColorGetA(paper.pixel(15, 70)), 0u);
}

// ---- this library's own --------------------------------------------------

TEST(Pen, AMaterialIsAFill) {
  using sigil::material::skia::Paint;
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(Paint::linear({0, 0}, {100, 0},
                               {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}));
  paper.pen.rect(0, 0, 100, 100);
  paper.end();
  EXPECT_GT(SkColorGetR(paper.pixel(2, 50)), 200u);
  EXPECT_GT(SkColorGetB(paper.pixel(97, 50)), 200u);
}

TEST(Pen, AMaterialFitsTheCanvasUnlessTheFillSaysTheShape) {
  using sigil::material::skia::Paint;
  using sigil::material::skia::Stop;
  const std::vector<Stop> ramp{{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  // ONE unit-square ramp, TWO boxes far apart. The only difference between
  // the two papers is the word on the fill.
  const auto draw = [&](Paper& paper, bool fitted) {
    paper.begin();
    paper.pen.noStroke();
    if (fitted)
      paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp), SHAPE);
    else
      paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp));
    paper.pen.rect(0, 0, 40, 40);
    paper.pen.rect(60, 0, 40, 40);
    paper.end();
  };

  Paper canvasFit;
  draw(canvasFit, false);
  // One ramp across the frame: the left box sits at its red end, the right
  // box at its blue one, and neither box has a ramp of its own.
  EXPECT_GT(SkColorGetR(canvasFit.pixel(2, 20)), 200u);
  EXPECT_LT(SkColorGetB(canvasFit.pixel(37, 20)), 150u);
  EXPECT_GT(SkColorGetB(canvasFit.pixel(97, 20)), 200u);

  Paper shapeFit;
  draw(shapeFit, true);
  // A ramp per box: each runs the whole way from red to blue inside its
  // own bounds, wherever those bounds are.
  EXPECT_GT(SkColorGetR(shapeFit.pixel(2, 20)), 200u);
  EXPECT_GT(SkColorGetB(shapeFit.pixel(37, 20)), 200u);
  EXPECT_GT(SkColorGetR(shapeFit.pixel(62, 20)), 200u);
  EXPECT_GT(SkColorGetB(shapeFit.pixel(97, 20)), 200u);
}

TEST(Pen, AFitIsSaidOnTheFillThatSetsIt) {
  using sigil::material::skia::Paint;
  using sigil::material::skia::Stop;
  const std::vector<Stop> ramp{{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp), SHAPE);
  // A fill set without the word goes back to the canvas rather than
  // inheriting the fit of the fill before it.
  paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp));
  paper.pen.rect(0, 0, 40, 40);
  paper.end();
  EXPECT_LT(SkColorGetB(paper.pixel(37, 20)), 150u);
}

TEST(Pen, TheFillPaintIsNullUnderNoFillAndSoIsTheStroke) {
  Paper paper;
  paper.begin();
  paper.pen.fill(255, 0, 0);
  paper.pen.stroke(0, 0, 255);
  paper.pen.blendMode(ADD);
  const SkPaint* fill = paper.pen.fillPaint();
  ASSERT_NE(fill, nullptr);
  EXPECT_EQ(fill->asBlendMode(), SkBlendMode::kPlus);

  // `noFill()` means there is NO FILL to hand over, not a colourless one:
  // the answer is null, and the pen's blend has to be read off the stroke
  // instead. Every verb in the class checks before it dereferences and a
  // caller through the canvas door has to as well.
  paper.pen.noFill();
  EXPECT_EQ(paper.pen.fillPaint(), nullptr);
  const SkPaint* stroke = paper.pen.strokePaint();
  ASSERT_NE(stroke, nullptr);
  EXPECT_EQ(stroke->asBlendMode(), SkBlendMode::kPlus);

  paper.pen.noStroke();
  EXPECT_EQ(paper.pen.strokePaint(), nullptr);
  // A zero weight is the other way a stroke stops existing.
  paper.pen.stroke(0, 0, 255);
  paper.pen.strokeWeight(0);
  EXPECT_EQ(paper.pen.strokePaint(), nullptr);
  paper.end();
}

}  // namespace

// ---- the embedded shader table --------------------------------------------

TEST(Pen, TheShaderTableHoldsEveryFileTheDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(shaderSources(),
                                                 SIGIL_DRAW_SHADER_DIR);
}
