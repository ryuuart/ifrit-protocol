/** @file
 * The pen's transform: p5's angle mode, what push and pop restore, what
 * a frame keeps, the order the pen and a borrowed canvas share, and the
 * scale a frame began on.
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

TEST(Pen, AngleModeDegreesReadsRotateInDegrees) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.angleMode(DEGREES);
  paper.pen.translate(50, 50);
  paper.pen.rotate(90);
  paper.pen.rect(0, 0, 30, 4);  // a bar along +x turns to point down +y
  paper.end();
  EXPECT_EQ(paper.pixel(48, 65), SK_ColorRED);
  EXPECT_EQ(paper.pixel(65, 52), SK_ColorTRANSPARENT);
}

TEST(Pen, PushPopRestoresFillAndTransform) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.push();
  paper.pen.fill(0, 0, 255);
  paper.pen.translate(50, 50);
  paper.pen.rect(0, 0, 10, 10);
  paper.pen.pop();
  paper.pen.rect(0, 0, 10, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(55, 55), SK_ColorBLUE);
  EXPECT_EQ(paper.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(paper.pixel(15, 15), SK_ColorTRANSPARENT);
}

// ---- the canvas itself, for another library's drawing -----------------------

TEST(Pen, TheCanvasCarriesThePensTransform) {
  Paper paper;
  paper.begin();
  paper.pen.translate(30, 40);
  SkPaint paint;
  paint.setColor(SK_ColorRED);
  paint.setAntiAlias(false);
  ASSERT_NE(paper.pen.canvas(), nullptr);
  paper.pen.canvas()->drawRect(SkRect::MakeWH(10, 10), paint);
  paper.end();
  // Drawn in the pen's space: the rect landed where pen.rect would have
  // put it, not at the canvas origin.
  EXPECT_EQ(paper.inked(), SkIRect::MakeXYWH(30, 40, 10, 10));
}

TEST(Pen, TheCanvasAndThePensOwnVerbsShareOneOrder) {
  Paper paper;
  paper.begin();
  paper.pen.noStroke();
  paper.pen.fill(0, 255, 0);
  paper.pen.rect(0, 0, 50, 50);
  SkPaint paint;
  paint.setColor(SK_ColorRED);
  paper.pen.canvas()->drawRect(SkRect::MakeWH(50, 50), paint);
  paper.end();
  EXPECT_EQ(paper.pixel(25, 25), SK_ColorRED);
}

TEST(Pen, TheContentScaleIsWhatTheFrameBeganOn) {
  Paper paper;
  paper.surface->getCanvas()->scale(2, 2);
  paper.begin();
  EXPECT_FLOAT_EQ(paper.pen.contentScale(), 2.0f);
  paper.end();
}

}  // namespace
