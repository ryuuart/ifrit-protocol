/** @file
 * observable_flowfield_1 — a regular grid sampling a diagonal angle field.
 */

// TAGS: Drawing/Generative

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
using namespace sigil::draw;

namespace {

struct ObservableFlowfield1 final : sketch::Sketch {
  void setup(sketch::SketchContext& context) override {
    context.canvas(720, 720);
    context.captureAt(0.05);

    context.composer.render(compose::graphics("observable_flowfield_1.loop",
                                              [this](Pen& pen) { draw(pen); })
                                .absolute()
                                .inset(0));
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.noFill();
      pen.strokeCap(SQUARE);
    }
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
