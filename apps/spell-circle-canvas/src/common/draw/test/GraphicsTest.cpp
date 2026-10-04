/** @file
 * p5's createGraphics: a canvas of its own, drawn once and put down.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Pen.h>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
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

TEST(Graphics, FormedNoCoarserThanItsDensityFloor) {
  Paper paper;
  Graphics buffer{20, 20};
  buffer.setDensityFloor(2.0f);
  paper.begin();
  Pen& g = buffer.begin(paper.pen);
  EXPECT_FLOAT_EQ(g.contentScale(), 2.0f) << "the buffer's pen says so";
  buffer.end();
  paper.end();
  EXPECT_EQ(buffer.extent(), SkISize::Make(40, 40))
      << "twice the host's density, which is one";
}

TEST(Graphics, NonFiniteInputsLeaveTheLastValidSizeAndDensity) {
  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  EXPECT_THROW((Graphics{infinity, 20}), std::invalid_argument);
  EXPECT_THROW((Graphics{20, nan}), std::invalid_argument);

  Paper paper;
  Graphics buffer{20, 30};
  buffer.setDensityFloor(2);
  EXPECT_THROW(buffer.resize(40, nan), std::invalid_argument);
  EXPECT_THROW(buffer.setDensityFloor(infinity), std::invalid_argument);
  EXPECT_FLOAT_EQ(buffer.width(), 20);
  EXPECT_FLOAT_EQ(buffer.height(), 30);
  paper.begin();
  buffer.begin(paper.pen);
  buffer.end();
  paper.end();
  EXPECT_EQ(buffer.extent(), SkISize::Make(40, 60));
}

TEST(Graphics, AnUnrepresentableExtentKeepsItsPixelsAndCanRecover) {
  Paper paper;
  Graphics buffer{20, 20};
  paper.begin();
  Pen& g = buffer.begin(paper.pen);
  g.background(255, 0, 0);
  buffer.end();
  const sk_sp<SkImage> image = buffer.image();

  buffer.resize(std::numeric_limits<float>::max(), 20);
  EXPECT_THROW(buffer.begin(paper.pen), std::length_error);
  EXPECT_EQ(buffer.pen.canvas(), nullptr);
  EXPECT_EQ(buffer.extent(), SkISize::Make(20, 20));
  EXPECT_EQ(buffer.image()->uniqueID(), image->uniqueID());

  buffer.resize(20, 20);
  EXPECT_NO_THROW(buffer.begin(paper.pen));
  buffer.end();
  paper.pen.image(buffer, 0, 0);
  paper.end();
  EXPECT_EQ(paper.pixel(10, 10), SK_ColorRED);
}

TEST(Graphics, FractionalSizePaintsToBothRoundedEdges) {
  Paper paper;
  Graphics buffer{20.49f, 20.51f};
  paper.begin();
  Pen& g = buffer.begin(paper.pen);
  g.noSmooth();
  g.noStroke();
  g.fill(255, 0, 0);
  g.rect(0, 0, buffer.width(), buffer.height());
  buffer.end();
  paper.end();

  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(buffer.extent()));
  ASSERT_TRUE(buffer.image()->readPixels(nullptr, pixels.pixmap(), 0, 0));
  EXPECT_EQ(pixels.getColor(19, 20), SK_ColorRED);
}

TEST(Graphics, ReopeningClosesThePreviousFrameBeforeAResize) {
  Paper paper;
  Graphics buffer{20, 20};
  paper.begin();
  Pen& first = buffer.begin(paper.pen);
  first.noStroke();
  first.fill(255, 0, 0);
  first.rect(0, 0, 5, 5);
  first.push();
  first.fill(0, 255, 0);
  first.translate(10, 10);

  buffer.resize(40, 40);
  Pen& second = buffer.begin(paper.pen);
  second.rect(0, 0, 5, 5);
  buffer.end();
  paper.pen.image(buffer, 0, 0);
  paper.end();
  EXPECT_EQ(second.canvas(), nullptr);
  EXPECT_EQ(paper.pixel(2, 2), SK_ColorRED);
  EXPECT_EQ(paper.pixel(12, 12), SK_ColorTRANSPARENT);
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

TEST(Graphics, AResizeKeepsWhatWasDrawnOnIt) {
  Paper paper;
  Graphics buffer{20, 20};
  paper.begin(1);
  Pen& first = buffer.begin(paper.pen);
  first.noStroke();
  first.fill(255, 0, 0);
  first.rect(0, 0, 20, 20);
  buffer.end();
  paper.end();

  // TWICE THE CANVAS SIZE, so the surface has to be replaced: what stood
  // on it is carried into the replacement rather than cleared out of it,
  // scaled from the extent it was drawn at to the new one.
  buffer.resize(40, 40);
  paper.begin(2);
  buffer.begin(paper.pen);
  buffer.end();
  paper.pen.image(buffer, 0, 0);
  paper.end();
  EXPECT_EQ(buffer.extent(), SkISize::Make(40, 40));
  EXPECT_EQ(paper.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(paper.pixel(35, 35), SK_ColorRED);
  EXPECT_EQ(SkColorGetA(paper.pixel(45, 45)), 0u);
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
