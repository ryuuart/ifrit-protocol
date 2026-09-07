/** @file
 * The door between the tree and the pen, both ways: a pen program as a
 * node's content, and an element retained inside a pen's loop.
 */

#include <sigilcompose/draw/Draw.h>

#include "support/Host.h"

namespace {

using sigil::draw::Frame;
using sigil::draw::Pen;

/** A pen over a raster surface of its own, begun with the host's fonts. */
struct Paper {
  Paper() : surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 100))) {
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  }
  void begin(int count, bool withFonts = true) {
    beginWith(count, withFonts ? &fonts() : nullptr);
  }
  void beginWith(int count, weave::FontContext& context) {
    beginWith(count, &context);
  }
  void beginWith(int count, weave::FontContext* context) {
    Frame frame;
    frame.width = 100;
    frame.height = 100;
    frame.seconds = count / 60.0;
    frame.deltaSeconds = 1.0 / 60.0;
    frame.frameCount = count;
    frame.fonts = context;
    pen.begin(*surface->getCanvas(), frame);
  }
  SkColor pixel(int x, int y) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surface->readPixels(bm.pixmap(), x, y);
    return bm.getColor(0, 0);
  }
  bool anyWhiteIn(SkIRect region) {
    for (int y = region.top(); y < region.bottom(); ++y)
      for (int x = region.left(); x < region.right(); ++x)
        if (pixel(x, y) == SK_ColorWHITE) return true;
    return false;
  }
  sk_sp<SkSurface> surface;
  Pen pen;
};

TEST(DrawNode, RunsThePenOverTheNodesBoxEveryFrame) {
  Host host;
  int runs = 0;
  float width = 0;
  host.composer.render(stack().child(pen([&](Pen& pen) {
                                       ++runs;
                                       width = pen.width;
                                       pen.noStroke();
                                       pen.fill(255, 0, 0);
                                       pen.rect(0, 0, 10, 10);
                                     })
                                         .width(50)
                                         .height(40)));
  host.frame();
  EXPECT_EQ(runs, 1);
  EXPECT_FLOAT_EQ(width, 50.0f);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(15, 15), SK_ColorBLACK);
  host.frame(1.0 / 60.0);
  EXPECT_EQ(runs, 2);  // Cache::None: the program runs each frame
}

TEST(DrawNode, ThePenHoldsItsStyleFromFrameToFrame) {
  Host host;
  host.composer.render(stack().child(pen([](Pen& pen) {
                                       if (pen.frameCount == 1) {
                                         pen.noStroke();
                                         pen.fill(0, 0, 255);
                                       }
                                       pen.rect(0, 0, 20, 20);
                                     })
                                         .width(50)
                                         .height(50)));
  host.frame();
  host.frame(1.0 / 60.0);
  EXPECT_EQ(host.pixel(10, 10), SK_ColorBLUE);
}

TEST(DrawNode, TheTransformStartsAtTheBox) {
  Host host;
  host.composer.render(stack().child(pen([](Pen& pen) {
                                       pen.noStroke();
                                       pen.fill(0, 255, 0);
                                       pen.rect(0, 0, 10, 10);
                                     })
                                         .width(30)
                                         .height(30)
                                         .left(60)
                                         .top(70)));
  host.frame();
  EXPECT_EQ(host.pixel(65, 75), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);
}

TEST(RetainedElement, PaintsAtTheBoxAndKeepsItsComposer) {
  Paper paper;
  for (int frame = 1; frame <= 2; ++frame) {
    paper.begin(frame);
    paper.pen.element(box().fill(red()), SkRect::MakeXYWH(10, 10, 30, 30));
    paper.pen.end();
  }
  EXPECT_EQ(paper.pixel(20, 20), SK_ColorRED);
  EXPECT_EQ(paper.pixel(39, 39), SK_ColorRED);
  EXPECT_EQ(paper.pixel(5, 5), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pixel(45, 45), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pen.retained().size(), 1u);
}

TEST(RetainedElement, ShapesTextWithThePensFonts) {
  Paper paper;
  paper.begin(1);
  paper.pen.element(text(u8"Hi", whiteStyle(24)),
                    SkRect::MakeXYWH(10, 10, 80, 40));
  paper.pen.end();
  EXPECT_TRUE(paper.anyWhiteIn(SkIRect::MakeXYWH(10, 10, 80, 40)));
  EXPECT_FALSE(paper.anyWhiteIn(SkIRect::MakeXYWH(0, 60, 100, 40)));
}

TEST(RetainedElement, FollowsThePensTransform) {
  Paper paper;
  paper.begin(1);
  paper.pen.translate(50, 50);
  paper.pen.element(box().fill(blue()), SkRect::MakeXYWH(0, 0, 20, 20));
  paper.pen.end();
  EXPECT_EQ(paper.pixel(60, 60), SK_ColorBLUE);
  EXPECT_EQ(paper.pixel(10, 10), SK_ColorTRANSPARENT);
}

TEST(RetainedElement, AnotherFontContextGetsAComposerBuiltOnIt) {
  // The guest is kept across frames and the composer inside it holds the
  // font context it was built from BY REFERENCE. A pen that draws with a
  // different one must not be answered with that composer: it would shape
  // against a context this pen does not hold, and one that has gone is a
  // dangling reference. The tree is the same, so what the case can see is
  // that the guest was remade and the drawing still lands.
  Paper paper;
  weave::FontContext second(sigil::weave::ports::systemFontManager());
  // ONE call site, so one slot and one kept guest — which is what makes
  // the second frame reach the composer the first frame built.
  for (int frame = 1; frame <= 2; ++frame) {
    paper.beginWith(frame, frame == 1 ? &fonts() : &second);
    paper.pen.element(text(u8"Hi", whiteStyle(24)),
                      SkRect::MakeXYWH(0, frame == 1 ? 0.0f : 50.0f, 90, 40));
    paper.pen.end();
  }
  EXPECT_EQ(paper.pen.retained().size(), 1u) << "one slot, one guest";
  EXPECT_TRUE(paper.anyWhiteIn(SkIRect::MakeXYWH(0, 0, 90, 40)));
  EXPECT_TRUE(paper.anyWhiteIn(SkIRect::MakeXYWH(0, 50, 90, 40)))
      << "the second context's text is shaped and drawn";
}

TEST(RetainedElement, PaintsNothingWithoutFonts) {
  Paper paper;
  paper.begin(1, false);
  paper.pen.element(box().fill(red()), SkRect::MakeXYWH(10, 10, 30, 30));
  paper.pen.end();
  EXPECT_EQ(paper.pixel(20, 20), SK_ColorTRANSPARENT);
  EXPECT_EQ(paper.pen.retained().size(), 0u);
}

}  // namespace
