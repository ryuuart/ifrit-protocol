/** @file
 * Images through the pen: what smoothing does to the sampler.
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

// ---- smoothing reaches the image sampler ------------------------------------

/** A two-by-two image, one colour per texel, for a blit to magnify. */
sk_sp<SkImage> quadrants() {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(2, 2));
  bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 1, 1), SK_ColorRED);
  bitmap.eraseArea(SkIRect::MakeXYWH(1, 0, 1, 1), SK_ColorGREEN);
  bitmap.eraseArea(SkIRect::MakeXYWH(0, 1, 1, 1), SK_ColorBLUE);
  bitmap.eraseArea(SkIRect::MakeXYWH(1, 1, 1, 1), SK_ColorWHITE);
  bitmap.setImmutable();
  return bitmap.asImage();
}

TEST(Pen, NoSmoothMakesImageDrawingNearestNeighbour) {
  Paper paper;
  paper.begin();
  paper.pen.noSmooth();
  paper.pen.image(quadrants(), 0, 0, 40, 40);
  paper.end();
  // The boundary between two texels is a step: the last column of the
  // first block is the whole first colour and the first column of the
  // second is the whole second one, with nothing between them.
  EXPECT_EQ(paper.pixel(19, 5), SK_ColorRED);
  EXPECT_EQ(paper.pixel(20, 5), SK_ColorGREEN);
  EXPECT_EQ(paper.pixel(5, 35), SK_ColorBLUE);
}

TEST(Pen, SmoothingOnBlendsAcrossTheImageBoundary) {
  Paper paper;
  paper.begin();
  paper.pen.image(quadrants(), 0, 0, 40, 40);
  paper.end();
  // The default is p5's smoothing: the same boundary is a ramp, so
  // neither side of it is either texel's own colour.
  const SkColor left = paper.pixel(19, 20);
  EXPECT_NE(left, SK_ColorRED);
  EXPECT_NE(left, SK_ColorGREEN);
}

}  // namespace
