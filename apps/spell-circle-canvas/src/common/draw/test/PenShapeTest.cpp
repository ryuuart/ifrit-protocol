/** @file
 * The shapes the pen draws: p5's rect, ellipse and arc modes, a closed
 * begin/end outline, a silhouette in the rect mode's box, and the meshes
 * a fill governs corner by corner.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <sigildraw/Draw.h>

#include <cmath>
#include <memory>
#include <vector>

#include "support/Paper.h"

namespace {

using namespace sigil::draw;
using sigil::draw::testing::Paper;

// ---- p5's semantics ---------------------------------------------------------

TEST(Pen, RectModeCenterLandsWhereP5Says) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rectMode(CENTER);
  paper.pen.rect(50, 50, 20, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(50, 50), SK_ColorRED);
  EXPECT_EQ(paper.pixel(41, 46), SK_ColorRED);
  EXPECT_EQ(paper.pixel(58, 53), SK_ColorRED);
  EXPECT_EQ(paper.pixel(38, 50), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pixel(50, 44), SK_ColorTRANSPARENT);
}

TEST(Pen, RectModeCornerIsTheDefault) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(10, 10, 20, 20);
  paper.end();
  EXPECT_EQ(paper.pixel(12, 12), SK_ColorRED);
  EXPECT_EQ(paper.pixel(28, 28), SK_ColorRED);
  EXPECT_EQ(paper.pixel(8, 8), SK_ColorTRANSPARENT);
}

TEST(Pen, EllipseModeCenterIsTheDefaultAndRadiusDoubles) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(0, 255, 0);
  paper.pen.circle(50, 50, 20);
  paper.end();
  EXPECT_EQ(paper.pixel(50, 50), SK_ColorGREEN);
  EXPECT_EQ(paper.pixel(50, 42), SK_ColorGREEN);
  EXPECT_EQ(paper.pixel(50, 38), SK_ColorTRANSPARENT);

  Paper radius;
  radius.begin();
  radius.pen.noStroke();
  radius.pen.fill(0, 255, 0);
  radius.pen.ellipseMode(RADIUS);
  radius.pen.ellipse(50, 50, 20, 20);
  radius.end();
  EXPECT_EQ(radius.pixel(50, 32), SK_ColorGREEN);
  EXPECT_EQ(radius.pixel(50, 28), SK_ColorTRANSPARENT);
}

TEST(Pen, ArcFillsThePieUnlessChord) {
  // A quarter from 12 o'clock to 3 o'clock, drawn OPEN: the pie reaches
  // the centre. Drawn CHORD, the centre is outside the segment.
  Paper open;
  open.begin();
  open.pen.noStroke();
  open.pen.fill(0, 0, 255);
  open.pen.arc(50, 50, 80, 80, -HALF_PI, 0);
  open.end();
  EXPECT_EQ(open.pixel(55, 45), SK_ColorBLUE);
  EXPECT_EQ(open.pixel(45, 55), SK_ColorTRANSPARENT);

  Paper chord;
  chord.begin();
  chord.pen.noStroke();
  chord.pen.fill(0, 0, 255);
  chord.pen.arc(50, 50, 80, 80, -HALF_PI, 0, CHORD);
  chord.end();
  EXPECT_EQ(chord.pixel(55, 45), SK_ColorTRANSPARENT);
  EXPECT_EQ(chord.pixel(80, 30), SK_ColorBLUE);
}

TEST(Pen, BeginShapeCloseFillsThePolygon) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.beginShape();
  paper.pen.vertex(10, 10);
  paper.pen.vertex(90, 10);
  paper.pen.vertex(50, 90);
  paper.pen.endShape(CLOSE);
  paper.end();
  EXPECT_EQ(paper.pixel(50, 30), SK_ColorRED);
  EXPECT_EQ(paper.pixel(10, 80), SK_ColorTRANSPARENT);
}

TEST(Pen, AMeshIsDrawnWithThePensFillWhereItGoverns) {
  const SkPoint corners[3] = {{10, 10}, {90, 10}, {10, 90}};
  const SkColor reds[3] = {SK_ColorRED, SK_ColorRED, SK_ColorRED};

  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(0, 255, 0);
  paper.pen.vertices(SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 3,
                                          corners, nullptr, reds));
  paper.end();
  // The mesh carries its own corner colours and the fill is a plain one,
  // so the corners paint — the rule the per-corner form of `vertex`
  // follows. It lands in the pen's space, and only inside the triangle.
  EXPECT_EQ(paper.pixel(20, 20), SK_ColorRED);
  EXPECT_EQ(paper.pixel(80, 80), SK_ColorTRANSPARENT);

  // The pen's transform carries it, and `noFill()` means there is nothing
  // to draw it with.
  Paper moved;
  moved.begin();
  moved.pen.noStroke();
  moved.pen.noFill();
  moved.pen.vertices(SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 3,
                                          corners, nullptr, reds));
  EXPECT_EQ(moved.inked(), SkIRect::MakeEmpty());
  moved.pen.fill(0, 0, 255);
  moved.pen.translate(0, 5);
  moved.pen.vertices(SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 3,
                                          corners, nullptr, nullptr));
  moved.end();
  // No corner colours, so the fill's own colour paints, five rows down.
  EXPECT_EQ(moved.pixel(20, 25), SK_ColorBLUE);
  EXPECT_EQ(moved.pixel(20, 12), SK_ColorTRANSPARENT);
}

TEST(Pen, AMeshTakesAFittedMaterialOverItsOwnBounds) {
  using sigil::material::skia::Paint;
  using sigil::material::skia::Stop;
  const std::vector<Stop> ramp{{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  // A triangle occupying the left half of the paper. Fitted, its ramp runs
  // red to blue across THAT, not across the frame.
  const SkPoint corners[3] = {{0, 0}, {50, 0}, {0, 100}};
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(Paint::linearUnit({0, 0}, {1, 0}, ramp), SHAPE);
  paper.pen.vertices(SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 3,
                                          corners, nullptr, nullptr));
  paper.end();
  EXPECT_GT(SkColorGetR(paper.pixel(1, 1)), 200u);
  EXPECT_GT(SkColorGetB(paper.pixel(46, 1)), 180u);
}

struct Ring {
  float inset = 0;
  SkPath path(SkSize size) const {
    return SkPath::Oval(
        SkRect::MakeWH(size.width(), size.height()).makeInset(inset, inset));
  }
};

TEST(Pen, ASilhouetteIsAShapeInTheRectModesBox) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rectMode(CENTER);
  paper.pen.shape(Ring{}, 50, 50, 40, 40);
  paper.end();
  EXPECT_EQ(paper.pixel(50, 50), SK_ColorRED);
  EXPECT_EQ(paper.pixel(50, 33), SK_ColorRED);
  EXPECT_EQ(paper.pixel(50, 28), SK_ColorTRANSPARENT);
}

// ---- a fill between two vertices colours the corners ------------------------

TEST(Pen, FillBetweenVerticesColoursEachCorner) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.beginShape(QUADS);
  paper.pen.fill(255, 0, 0);
  paper.pen.vertex(0, 0);
  paper.pen.vertex(0, 99);
  paper.pen.fill(0, 0, 255);
  paper.pen.vertex(99, 99);
  paper.pen.vertex(99, 0);
  paper.pen.endShape();
  paper.end();
  const SkColor left = paper.pixel(2, 50);
  const SkColor right = paper.pixel(97, 50);
  const SkColor middle = paper.pixel(50, 50);
  EXPECT_GT(SkColorGetR(left), 200u);
  EXPECT_LT(SkColorGetB(left), 60u);
  EXPECT_GT(SkColorGetB(right), 200u);
  EXPECT_LT(SkColorGetR(right), 60u);
  // Interpolated across the quad, so the middle is neither corner.
  EXPECT_GT(SkColorGetR(middle), 60u);
  EXPECT_GT(SkColorGetB(middle), 60u);
}

TEST(Pen, AlphaRampsAcrossAQuadTheSameWay) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.beginShape(QUADS);
  paper.pen.fill(255, 255, 255, 255);
  paper.pen.vertex(0, 0);
  paper.pen.vertex(0, 99);
  paper.pen.fill(255, 255, 255, 0);
  paper.pen.vertex(99, 99);
  paper.pen.vertex(99, 0);
  paper.pen.endShape();
  paper.end();
  EXPECT_GT(SkColorGetA(paper.pixel(2, 50)), 200u);
  EXPECT_LT(SkColorGetA(paper.pixel(97, 50)), 60u);
}

TEST(Pen, OneFillAcrossTheShapeStaysAPathAndStrokes) {
  Paper paper;
  paper.begin();
  paper.pen.fill(0, 255, 0);
  paper.pen.stroke(255, 0, 0);
  paper.pen.strokeWeight(6);
  paper.pen.beginShape(QUADS);
  paper.pen.vertex(20, 20);
  paper.pen.vertex(20, 80);
  paper.pen.vertex(80, 80);
  paper.pen.vertex(80, 20);
  paper.pen.endShape();
  paper.end();
  EXPECT_EQ(paper.pixel(50, 50), SK_ColorGREEN);
  EXPECT_EQ(paper.pixel(50, 20), SK_ColorRED);
}

TEST(Pen, AStrokedMeshStillWearsItsOutline) {
  Paper paper;
  paper.begin();
  paper.pen.stroke(255, 255, 255);
  paper.pen.strokeWeight(6);
  paper.pen.beginShape(QUADS);
  paper.pen.fill(255, 0, 0);
  paper.pen.vertex(20, 20);
  paper.pen.vertex(20, 80);
  paper.pen.fill(0, 0, 255);
  paper.pen.vertex(80, 80);
  paper.pen.vertex(80, 20);
  paper.pen.endShape();
  paper.end();
  EXPECT_EQ(paper.pixel(50, 20), SK_ColorWHITE);
  EXPECT_GT(SkColorGetR(paper.pixel(24, 50)), 200u);
}

}  // namespace
