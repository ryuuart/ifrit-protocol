/** @file
 * The kept canvas: a node whose pen paints onto a surface that stands
 * between frames, and p5's loop words over it.
 */

#include <sigilcompose/draw/Draw.h>

#include "support/Host.h"

namespace {

using sigil::draw::Pen;

TEST(GraphicsNode, KeepsWhatAnEarlierFrameDrew) {
  Host host;
  host.composer.render(
      stack().child(graphics([](Pen& pen) {
                      pen.noStroke();
                      pen.fill(255, 0, 0);
                      pen.rect((float)(pen.frameCount - 1) * 10.0f, 0, 10, 10);
                    })
                        .width(50)
                        .height(50)));
  host.frame();
  host.frame(1.0 / 60.0);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED) << "the first frame's block";
  EXPECT_EQ(host.pixel(15, 5), SK_ColorRED) << "and the second's beside it";
}

TEST(DrawNode, APenNodeRepaintsFromNothing) {
  Host host;
  host.composer.render(
      stack().child(pen([](Pen& pen) {
                      pen.noStroke();
                      pen.fill(255, 0, 0);
                      pen.rect((float)(pen.frameCount - 1) * 10.0f, 0, 10, 10);
                    })
                        .width(50)
                        .height(50)));
  host.frame();
  host.frame(1.0 / 60.0);
  // The same program on a pen node: there is no surface under it, so only
  // the frame that just ran is on the canvas.
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK) << "the first frame's block went";
  EXPECT_EQ(host.pixel(15, 5), SK_ColorRED);
}

TEST(GraphicsNode, NoLoopStopsTheProgramAndTheSurfaceIsStillPutDown) {
  Host host;
  int runs = 0;
  host.composer.render(stack().child(graphics([&runs](Pen& pen) {
                                       ++runs;
                                       pen.noStroke();
                                       pen.fill(255, 0, 0);
                                       pen.rect(0, 0, 10, 10);
                                       pen.noLoop();
                                     })
                                         .width(50)
                                         .height(50)));
  host.frame();
  host.frame(1.0 / 60.0);
  host.frame(1.0 / 60.0);
  EXPECT_EQ(runs, 1);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED) << "put down on every frame";
}

TEST(GraphicsNode, RedrawRunsTheProgramOnce) {
  Host host;
  int runs = 0;
  host.composer.render(stack().child(graphics([&runs](Pen& pen) {
                                       ++runs;
                                       pen.noLoop();
                                       if (runs == 1) pen.redraw();
                                     })
                                         .width(50)
                                         .height(50)));
  host.frame();
  host.frame(1.0 / 60.0);
  host.frame(1.0 / 60.0);
  EXPECT_EQ(runs, 2) << "the one redraw, and nothing after it";
}

}  // namespace
