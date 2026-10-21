/** @file
 * The layer stack: one layer composites to the pen's own drawing, a
 * `REMOVE` layer erases where it has ink, a mask and a clip equal the
 * pen's own clip, a placement equals the pen's own matrix, and the
 * composite is re-made only when a layer moved.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Pen.h>
#include <sigildraw/Stack.h>

#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/Paper.h"

namespace {

using namespace sigil::draw;
using sigil::draw::testing::Paper;

/** Every pixel of an image, read back. */
SkBitmap pixelsOf(const sk_sp<SkImage>& image) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(image->width(), image->height()));
  EXPECT_TRUE(image->readPixels(bitmap.pixmap(), 0, 0));
  return bitmap;
}

/** How many pixels of two equal-sized pictures differ at all. */
int differingPixels(const SkBitmap& first, const SkBitmap& second) {
  int differing = 0;
  for (int y = 0; y < first.height(); ++y)
    for (int x = 0; x < first.width(); ++x)
      if (first.getColor(x, y) != second.getColor(x, y)) ++differing;
  return differing;
}

/** Something with edges at every angle and more than one colour, so a
 *  picture that moved or blended differently cannot match by accident. */
void drawSubject(Pen& pen) {
  pen.noStroke();
  pen.fill(240, 90, 40);
  pen.circle(40, 45, 50);
  pen.stroke(20, 60, 200);
  pen.strokeWeight(5);
  pen.fill(40, 200, 120, 160);
  pen.triangle(55, 15, 90, 80, 30, 85);
  pen.line(5, 95, 95, 5);
}

// ---- the composite ----------------------------------------------------------

TEST(Stack, OneLayerCompositesToThePensOwnDrawingPixelForPixel) {
  Paper stacked;
  Stack stack{100, 100};
  stacked.begin();
  drawSubject(stack.layer("subject").begin(stacked.pen));
  stack.layer("subject").end();
  stack.composite(stacked.pen);
  stacked.end();

  Paper direct;
  direct.begin();
  drawSubject(direct.pen);
  direct.end();

  EXPECT_EQ(differingPixels(stacked.pixels(), direct.pixels()), 0);
}

TEST(Stack, RemoveErasesExactlyWhereTheLayerHasInk) {
  Paper paper;
  Stack stack{100, 100};
  paper.begin();
  Pen& ground = stack.layer("ground").begin(paper.pen);
  ground.background(255, 0, 0);
  stack.layer("ground").end();
  Layer& cut = stack.layer("cut");
  Pen& eraser = cut.begin(paper.pen);
  eraser.noStroke();
  eraser.fill(255);
  eraser.circle(50, 50, 60);
  cut.end();
  cut.blendMode(REMOVE);
  stack.composite(paper.pen);
  paper.end();

  const SkBitmap ink = pixelsOf(cut.image());
  const SkBitmap result = paper.pixels();
  int erased = 0;
  for (int y = 0; y < 100; ++y)
    for (int x = 0; x < 100; ++x) {
      const int inkAlpha = (int)SkColorGetA(ink.getColor(x, y));
      const int left = (int)SkColorGetA(result.getColor(x, y));
      if (inkAlpha == 0) {
        ASSERT_EQ(result.getColor(x, y), SK_ColorRED) << x << "," << y;
      } else if (inkAlpha == 255) {
        ASSERT_EQ(left, 0) << x << "," << y;
        ++erased;
      } else {
        ASSERT_LE(std::abs(left - (255 - inkAlpha)), 1) << x << "," << y;
      }
    }
  EXPECT_GT(erased, 2000) << "the disc's interior is gone";
}

TEST(Stack, AMaskByAlphaEqualsAHandDrawnClip) {
  Paper stacked;
  Stack stack{100, 100};
  stacked.begin();
  drawSubject(stack.layer("subject").begin(stacked.pen));
  stack.layer("subject").end();
  Layer& stencil = stack.layer("stencil");
  Pen& shape = stencil.begin(stacked.pen);
  shape.noStroke();
  shape.fill(255);
  shape.rect(20, 10, 50, 60);
  stencil.end();
  stencil.hide();
  stack.layer("subject").mask(stencil);
  stack.composite(stacked.pen);
  stacked.end();

  // The same pixels put down under the pen's clip: a shape drawn under a
  // clip is rasterised differently at the clip's edge from one drawn whole,
  // so the reference cuts the layer's own pixels as the mask does.
  Paper direct;
  direct.begin();
  direct.pen.clip([&] { direct.pen.rect(20, 10, 50, 60); });
  direct.pen.image(stack.layer("subject").image(), 0, 0, 100, 100);
  direct.end();

  EXPECT_EQ(differingPixels(stacked.pixels(), direct.pixels()), 0);
}

TEST(Stack, AMaskByLuminanceShowsWhereTheMaskIsWhite) {
  Paper paper;
  Stack stack{100, 100};
  paper.begin();
  stack.layer("paint").begin(paper.pen).background(0, 0, 255);
  stack.layer("paint").end();
  Layer& stencil = stack.layer("stencil");
  Pen& shape = stencil.begin(paper.pen);
  shape.noStroke();
  shape.fill(255);
  shape.rect(0, 0, 50, 100);
  shape.fill(0);
  shape.rect(50, 0, 50, 100);
  stencil.end();
  stencil.hide();
  stack.layer("paint").mask(stencil, MaskChannel::Luminance);
  stack.composite(paper.pen);
  paper.end();
  EXPECT_EQ(paper.pixel(25, 50), SK_ColorBLUE) << "white shows the layer";
  EXPECT_EQ(SkColorGetA(paper.pixel(75, 50)), 0u) << "black hides it";
}

TEST(Stack, AClipShapeEqualsAHandDrawnClip) {
  Paper stacked;
  Stack stack{100, 100};
  stacked.begin();
  Layer& subject = stack.layer("subject");
  drawSubject(subject.begin(stacked.pen));
  subject.end();
  subject.clip([](Pen& pen) { pen.rect(15, 25, 60, 40); });
  stack.composite(stacked.pen);
  stacked.end();

  Paper direct;
  direct.begin();
  direct.pen.clip([&] { direct.pen.rect(15, 25, 60, 40); });
  direct.pen.image(subject.image(), 0, 0, 100, 100);
  direct.end();

  EXPECT_EQ(differingPixels(stacked.pixels(), direct.pixels()), 0);
}

TEST(Stack, ATransformEqualsThePensOwnMatrix) {
  const LayerTransform placement{.origin = {50, 50},
                                 .translation = {6, -4},
                                 .scale = {0.8f, 1.15f},
                                 .shear = {0.2f, -0.1f},
                                 .rotation = 0.4f};
  Paper stacked;
  Stack stack{100, 100};
  stacked.begin();
  Layer& subject = stack.layer("subject");
  drawSubject(subject.begin(stacked.pen));
  subject.end();
  subject.transform(placement);
  stack.composite(stacked.pen);
  stacked.end();

  Paper direct;
  direct.begin();
  direct.pen.translate(56, 46);
  direct.pen.rotate(0.4f);
  direct.pen.shearX(0.2f);
  direct.pen.shearY(-0.1f);
  direct.pen.scale(0.8f, 1.15f);
  direct.pen.translate(-50, -50);
  EXPECT_EQ(direct.pen.canvas()->getTotalMatrix(), placement.matrix())
      << "the placement is the matrix the pen's verbs build";
  direct.pen.image(subject.image(), 0, 0, 100, 100);
  direct.end();

  EXPECT_EQ(differingPixels(stacked.pixels(), direct.pixels()), 0);
}

TEST(Stack, OpacityAndVisibilityReachTheComposite) {
  Paper paper;
  Stack stack{100, 100};
  paper.begin();
  Layer& paint = stack.layer("paint");
  paint.begin(paper.pen).background(255, 255, 255);
  paint.end();
  paint.opacity(0.5f);
  stack.composite(paper.pen);
  EXPECT_NEAR((int)SkColorGetA(paper.pixel(50, 50)), 128, 1);
  paper.pen.clear();
  paint.hide();
  stack.composite(paper.pen);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(50, 50)), 0u) << "a hidden layer shows nothing";
  EXPECT_THROW(paint.opacity(std::numeric_limits<float>::quiet_NaN()),
               std::invalid_argument);
}

// ---- the kept composite -----------------------------------------------------

TEST(Stack, AStillStackRecompositesNothing) {
  Paper paper;
  Stack stack{100, 100};
  Layer& subject = stack.layer("subject");
  for (int frame = 1; frame <= 4; ++frame) {
    paper.begin(frame);
    if (frame == 1) {
      drawSubject(subject.begin(paper.pen));
      subject.end();
    }
    stack.composite(paper.pen);
    paper.end();
  }
  EXPECT_EQ(stack.revision(), 1u) << "made once, put down four times";
  subject.opacity(1.0f);
  subject.blendMode(BLEND);
  subject.show();
  paper.begin(5);
  stack.composite(paper.pen);
  paper.end();
  EXPECT_EQ(stack.revision(), 1u) << "a setting given its own value moves nothing";
}

TEST(Stack, AChangedLayerRecompositesOnce) {
  Paper paper;
  Stack stack{100, 100};
  Layer& first = stack.layer("first");
  Layer& second = stack.layer("second");
  paper.begin();
  drawSubject(first.begin(paper.pen));
  first.end();
  stack.composite(paper.pen);
  ASSERT_EQ(stack.revision(), 1u);
  second.begin(paper.pen).circle(10, 10, 8);
  second.end();
  stack.composite(paper.pen);
  stack.composite(paper.pen);
  EXPECT_EQ(stack.revision(), 2u) << "one change, one re-make";
  first.transform({.translation = {3, 0}});
  stack.composite(paper.pen);
  stack.composite(paper.pen);
  paper.end();
  EXPECT_EQ(stack.revision(), 3u);
}

// ---- the list of layers -----------------------------------------------------

TEST(Stack, ANameAnswersItsLayerAndOrderIsBottomToTop) {
  Stack stack{40, 40};
  Layer& rings = stack.layer("rings");
  stack.layer("field");
  stack.layer("cut");
  EXPECT_EQ(&stack.layer("rings"), &rings) << "asking again edits, never piles up";
  EXPECT_EQ(stack.names(), (std::vector<std::string>{"rings", "field", "cut"}));
  EXPECT_TRUE(stack.reorder("cut", 0));
  EXPECT_TRUE(stack.reorder("rings", 99));
  EXPECT_EQ(stack.names(), (std::vector<std::string>{"cut", "field", "rings"}));
  EXPECT_FALSE(stack.reorder("missing", 0));
  EXPECT_EQ(stack.find("missing"), nullptr);
  EXPECT_EQ(stack.count(), 3u);
}

TEST(Stack, RemovingAMaskLayerUnmasksWhatItServed) {
  Stack stack{40, 40};
  Layer& rings = stack.layer("rings");
  Layer& field = stack.layer("field");
  rings.mask(field);
  ASSERT_EQ(rings.maskSource(), &field);
  EXPECT_TRUE(stack.remove("field"));
  EXPECT_EQ(rings.maskSource(), nullptr);
  EXPECT_FALSE(stack.remove("field"));
  EXPECT_EQ(stack.names(), (std::vector<std::string>{"rings"}));
}

TEST(Stack, AMaskIsAnotherLayerOfTheSameStack) {
  Stack stack{40, 40};
  Stack other{40, 40};
  Layer& rings = stack.layer("rings");
  EXPECT_THROW(rings.mask(rings), std::invalid_argument);
  EXPECT_THROW(rings.mask(other.layer("field")), std::invalid_argument);
}

TEST(Stack, LayersAreFormedNoCoarserThanTheStacksDensityFloor) {
  Paper paper;
  Stack stack{20, 20};
  stack.setDensityFloor(2.0f);
  Layer& subject = stack.layer("subject");
  paper.begin();
  subject.begin(paper.pen).background(255);
  subject.end();
  const sk_sp<SkImage> composite = stack.image(paper.pen);
  paper.end();
  EXPECT_EQ(subject.buffer().extent(), SkISize::Make(40, 40));
  ASSERT_TRUE(composite);
  EXPECT_EQ(composite->width(), 40);
}

}  // namespace
