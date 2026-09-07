/** @file
 * p5's createGraphics: a canvas of its own, drawn once and put down.
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

// ---- p5's createGraphics ----------------------------------------------------

TEST(Graphics, DrawnOnceAndPutDownWhereTheFrameSaysInCanvasUnits) {
  Paper paper;
  Graphics buffer{20, 20};
  paper.begin();
  Pen& g = buffer.begin(paper.pen);
  g.noStroke();
  g.fill(0, 255, 0);
  g.rect(0, 0, 20, 20);
  buffer.end();
  paper.pen.image(buffer, 10, 10);
  paper.end();
  EXPECT_EQ(paper.pixel(15, 15), SK_ColorGREEN);
  EXPECT_EQ(SkColorGetA(paper.pixel(5, 5)), 0u);
  EXPECT_EQ(paper.inked(), SkIRect::MakeXYWH(10, 10, 20, 20));
}

TEST(Graphics, KeepsItsPixelsAndItsStyleBetweenFrames) {
  Paper paper;
  Graphics buffer{20, 20};
  paper.begin(1);
  Pen& first = buffer.begin(paper.pen);
  first.noStroke();
  first.fill(255, 0, 0);
  first.rect(0, 0, 10, 20);
  buffer.end();
  paper.end();

  paper.begin(2);
  // No fill set this time: the buffer's own style held from the last
  // frame, as a pen's does, and the first frame's block is still there.
  Pen& second = buffer.begin(paper.pen);
  second.rect(10, 0, 10, 20);
  buffer.end();
  paper.pen.image(buffer, 0, 0);
  paper.end();
  EXPECT_EQ(paper.pixel(5, 10), SK_ColorRED);
  EXPECT_EQ(paper.pixel(15, 10), SK_ColorRED);
}

TEST(Graphics, IsFormedAtTheHostsDensityAndDrawnInCanvasUnits) {
  Paper paper;
  paper.surface->getCanvas()->scale(2, 2);
  Graphics buffer{20, 20};
  paper.begin();
  Pen& g = buffer.begin(paper.pen);
  g.noStroke();
  g.fill(0, 0, 255);
  g.circle(10, 10, 20);
  buffer.end();
  paper.pen.image(buffer, 0, 0);
  paper.end();
  EXPECT_EQ(buffer.extent(), SkISize::Make(40, 40));
  // Placed by its canvas size: twenty units on a doubled canvas is
  // forty pixels across, and the circle's centre lands at (20, 20).
  EXPECT_EQ(paper.pixel(20, 20), SK_ColorBLUE);
  EXPECT_EQ(SkColorGetA(paper.pixel(45, 45)), 0u);
}

}  // namespace
