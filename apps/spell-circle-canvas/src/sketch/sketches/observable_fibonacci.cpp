/** @file
 * observable_fibonacci — squares distributed by the golden-angle sequence.
 */

#include <sigilsketch/draw/Draw.h>

#include <cmath>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

struct ObservableFibonacci final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(800, 800);
    context.captureAt(0.05);
    context.pen.noStroke();
    context.pen.rectMode(CENTER);
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
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
