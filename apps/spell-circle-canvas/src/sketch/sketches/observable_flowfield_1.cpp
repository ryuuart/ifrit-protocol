/** @file
 * observable_flowfield_1 — a regular grid sampling a diagonal angle field.
 */

#include <sigilsketch/draw/Draw.h>

#include <cmath>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

struct ObservableFlowfield1 final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(720, 720);
    context.captureAt(0.05);
    context.pen.noFill();
    context.pen.strokeCap(SQUARE);
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    constexpr float kStep = 12.0f;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(0);
    pen.stroke(255);
    pen.strokeWeight(1.0f);
    for (float x = 0; x < pen.width; x += kStep) {
      for (float y = 0; y < pen.height; y += kStep) {
        const float angle = (x + y) * 0.01f * TAU + clock * 0.34f;
        pen.line(x, y, x + std::cos(angle) * kStep,
                 y + std::sin(angle) * kStep);
      }
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableFlowfield1, "observable_flowfield_1",
                "Draw · Observable reproductions",
                "A regular p5 line grid sampling the angle (x + y) times tau.")
