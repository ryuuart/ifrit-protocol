/** @file
 * The kept canvas: a node whose pen paints onto a surface that stands
 * between frames, and p5's loop words over it.
 */

#include <sigilcompose/draw/Draw.h>
#include <sigilmotion/clock/FrameClock.h>

#include <string>
#include <vector>

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

TEST(GraphicsNode, ThePointerReachesTheProgramInTheNodesOwnBox) {
  Host host;
  float x = -1, y = -1;
  bool pressed = false;
  host.composer.render(stack().child(graphics([&](Pen& pen) {
                                       x = pen.mouseX;
                                       y = pen.mouseY;
                                       pressed = pen.mouseIsPressed;
                                     })
                                         .absolute()
                                         .left(20)
                                         .top(10)
                                         .width(50)
                                         .height(50)));
  host.composer.setPointer({30, 15}, true);
  host.frame();
  EXPECT_FLOAT_EQ(x, 10.0f) << "the canvas point, less the node's corner";
  EXPECT_FLOAT_EQ(y, 5.0f);
  EXPECT_TRUE(pressed);
  host.composer.setPointer({100, 100}, false);
  host.frame(1.0 / 60.0);
  EXPECT_FLOAT_EQ(x, 80.0f);
  EXPECT_FLOAT_EQ(y, 90.0f);
  EXPECT_FALSE(pressed);
}

TEST(GraphicsNode, TheKeysReachTheProgram) {
  Host host;
  bool down = false, aHeld = false;
  std::string key;
  int code = 0;
  host.composer.render(stack().child(graphics([&](Pen& pen) {
                                       down = pen.keyIsPressed;
                                       key = pen.key;
                                       code = pen.keyCode;
                                       aHeld = pen.keyIsDown(65);
                                     })
                                         .width(50)
                                         .height(50)));
  host.composer.setKey("a", 65, true);
  host.frame();
  EXPECT_TRUE(down);
  EXPECT_EQ(key, "a");
  EXPECT_EQ(code, 65);
  EXPECT_TRUE(aHeld);
  host.composer.setKey("a", 65, false);
  host.frame(1.0 / 60.0);
  EXPECT_FALSE(down) << "released";
  EXPECT_FALSE(aHeld);
  EXPECT_EQ(key, "a") << "the last key pressed stays named, as p5 keeps it";
}

TEST(GraphicsNode, TheProgramCountsItsOwnRunsAndItsOwnStep) {
  sigil::motion::FrameClock clock;
  Host host;
  host.composer.setClock(&clock);
  std::vector<int> counts;
  std::vector<double> steps;
  host.composer.render(stack().child(graphics([&](Pen& pen) {
                                       if (pen.frameCount == 1)
                                         pen.frameRate(30);
                                       counts.push_back(pen.frameCount);
                                       steps.push_back(pen.deltaTime);
                                     })
                                         .width(50)
                                         .height(50)));
  host.frame();
  for (int i = 0; i < 3; ++i) {
    clock.advance(1.0 / 60.0);
    host.frame(1.0 / 60.0);
  }
  // Four frames at a sixtieth under a requested thirtieth: the program ran
  // on the first and the third, and its clock says so — two runs, the
  // second a thirtieth after the first.
  ASSERT_EQ(counts.size(), 2u);
  EXPECT_EQ(counts[0], 1);
  EXPECT_EQ(counts[1], 2) << "runs, not the frames the node was painted on";
  EXPECT_NEAR(steps[1], 1000.0 / 30.0, 1e-6);
}

TEST(GraphicsNode, TheCanvasIsFormedNoCoarserThanTheBakeDensity) {
  Host host;
  float density = 0;
  host.composer.setBakeDensity(2.0f);
  host.composer.render(
      stack().child(graphics([&](Pen& pen) { density = pen.contentScale(); })
                        .width(50)
                        .height(50)));
  host.frame();
  EXPECT_FLOAT_EQ(density, 2.0f)
      << "formed at the declared density on a canvas drawn at one";
}

}  // namespace
