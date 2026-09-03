// p5_hello.cpp — p5's bouncing ball, pasted in: a translucent background
// every frame is the trail, and the canvas keeps the last frame under it.
//
// The draw() everyone writes first. What changed to land here: the
// variables are members, `createCanvas` is `ctx.canvas`, and every verb
// has `pen.` in front of it.
//
// EDIT THESE FIRST
//   kFade   the background's alpha: lower is a longer trail
//   vx, vy  the speed, in pixels per frame

#include <sigilsketch/draw/Draw.h>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr float kFade = 30;

struct P5Hello final : sketch::DrawSketch {
  float x = 200, y = 100, vx = 3, vy = 2;

  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(400, 300);
    ctx.background(20);
    ctx.captureAt(3.0);
    ctx.pen.noStroke();
  }

  void draw(Pen& pen) override {
    pen.background(20, kFade);
    x += vx;
    y += vy;
    if (x < 20 || x > pen.width - 20) vx = -vx;
    if (y < 20 || y > pen.height - 20) vy = -vy;
    pen.fill(255, 120, 80);
    pen.circle(x, y, 40);
  }
};

}  // namespace

SIGIL_SKETCH(P5Hello, "Draw", "p5's bouncing ball with a trail, pasted in.")
