/** @file
 * observable_fibonacci — squares distributed by the golden-angle sequence.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
using namespace sigil::draw;

namespace {

struct ObservableFibonacci final : sketch::Sketch {
  void setup(sketch::SketchContext& context) override {
    context.canvas(800, 800);
    context.captureAt(0.05);

    context.composer.render(compose::graphics("observable_fibonacci.loop",
                                              [this](Pen& pen) { draw(pen); })
                                .absolute()
                                .inset(0));
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.noStroke();
      pen.rectMode(CENTER);
    }
    constexpr int kCount = 1000;
    constexpr float kPhi = 1.61803398875f;
    constexpr float kRadius = 0.70710678118f;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(0);
    pen.fill(255);
    for (int index = 1; index < kCount; ++index) {
      const float fraction = static_cast<float>(index) / kCount;
      const float angle = (index * kPhi + clock * 0.018f) * TAU;
      const float distance = fraction * kRadius * pen.width;
      const float x = pen.width * 0.5f + std::cos(angle) * distance;
      const float y = pen.height * 0.5f + std::sin(angle) * distance;
      const float size = fraction * 0.05f * pen.width;
      pen.rect(x, y, size, size);
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableFibonacci, "observable_fibonacci",
                "Draw · Observable reproductions",
                "One thousand growing squares follow the golden-angle spiral.")
