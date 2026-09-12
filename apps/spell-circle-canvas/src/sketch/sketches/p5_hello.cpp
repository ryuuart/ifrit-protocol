// p5_hello.cpp — p5's bouncing ball, pasted into a compose tree.
//
// The whole sketch is ONE NODE. `compose::graphics(program)` is a canvas
// that is KEPT between frames, which is what p5's own canvas is: the
// translucent background laid at the top of every frame is therefore the
// trail, and the ball's last position is still under it. A
// `compose::pen` node beside it would repaint from nothing and leave no
// trail at all.
//
// The draw() everyone writes first goes in unchanged. What changed to
// land here: the variables are members, `createCanvas` is `ctx.canvas`,
// and every verb has `pen.` in front of it.
//
// EDIT THESE FIRST
//   kFade   the background's alpha: lower is a longer trail
//   vx, vy  the speed, in pixels per frame

// TAGS: Drawing/Primitives, Motion/Physics, Runtime/Starter

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;

using sigil::draw::Pen;

namespace {

constexpr float kFade = 30;

}  // namespace

struct P5Hello final : sketch::Sketch {
  float x = 200, y = 100, vx = 3, vy = 2;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(400, 300);
    ctx.background({0.078f, 0.078f, 0.078f, 1});
    ctx.captureAt(3.0);
    // The node is declared once and the program runs every frame, so the
    // ball is stepped by the program and nothing is ever described again.
    ctx.composer.render(compose::graphics("p5_hello.ball",
                                          [this](Pen& pen) {
                                            pen.noStroke();
                                            pen.background(20, kFade);
                                            x += vx;
                                            y += vy;
                                            if (x < 20 || x > pen.width - 20)
                                              vx = -vx;
                                            if (y < 20 || y > pen.height - 20)
                                              vy = -vy;
                                            pen.fill(255, 120, 80);
                                            pen.circle(x, y, 40);
                                          })
                            .absolute()
                            .inset(0));
  }
};

SIGIL_SKETCH(P5Hello, "Draw",
             "p5's bouncing ball with a trail, as one compose::graphics node "
             "\xe2\x80\x94 the kept canvas is what makes the trail possible.")
