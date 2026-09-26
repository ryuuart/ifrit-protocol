/** @file
 * Images through the pen: what smoothing does to the sampler, and a
 * picture source's frame meeting its box under each fit.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <sigildraw/Pen.h>

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

// ---- a frame meets its box under a fit --------------------------------------

/** A frame twice as wide as it is tall: red on the left half, green on
 *  the right. */
sigil::media::Frame wideFrame() {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(20, 10));
  bitmap.eraseArea(SkIRect::MakeXYWH(0, 0, 10, 10), SK_ColorRED);
  bitmap.eraseArea(SkIRect::MakeXYWH(10, 0, 10, 10), SK_ColorGREEN);
  bitmap.setImmutable();
  sigil::media::Frame frame;
  frame.image = bitmap.asImage();
  return frame;
}

TEST(Pen, AFrameMeetsItsBoxUnderEachFit) {
  const auto drawn = [](sigil::material::Fit fit) {
    Paper paper(100, 100, SK_ColorBLACK);
    paper.begin();
    paper.pen.noSmooth();
    paper.pen.image(wideFrame(), 0, 0, 40, 40, fit);
    paper.end();
    return paper.pixels();
  };
  // Contain keeps the whole frame and letterboxes it: the bands above and
  // below stay bare, and both halves show.
  const SkBitmap contained = drawn(sigil::material::Fit::Contain);
  EXPECT_EQ(contained.getColor(20, 2), SK_ColorBLACK);
  EXPECT_EQ(contained.getColor(5, 20), SK_ColorRED);
  EXPECT_EQ(contained.getColor(35, 20), SK_ColorGREEN);
  // Cover fills the box and crops the sides: the middle column of the
  // frame fills it, so the box's corners are red and green.
  const SkBitmap covered = drawn(sigil::material::Fit::Cover);
  EXPECT_EQ(covered.getColor(1, 1), SK_ColorRED);
  EXPECT_EQ(covered.getColor(38, 38), SK_ColorGREEN);
  // Stretch fills both axes.
  const SkBitmap stretched = drawn(sigil::material::Fit::Stretch);
  EXPECT_EQ(stretched.getColor(5, 35), SK_ColorRED);
  EXPECT_EQ(stretched.getColor(35, 2), SK_ColorGREEN);
  // Native draws the frame's own pixels from the box's corner.
  const SkBitmap native = drawn(sigil::material::Fit::Native);
  EXPECT_EQ(native.getColor(15, 5), SK_ColorGREEN);
  EXPECT_EQ(native.getColor(25, 5), SK_ColorBLACK);
}

}  // namespace
