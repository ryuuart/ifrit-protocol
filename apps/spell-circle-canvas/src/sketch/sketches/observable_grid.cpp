/** @file
 * observable_grid — repeated square cells under one animated rotation.
 */

#include <sigilsketch/draw/Draw.h>

#include <cmath>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

struct ObservableGrid final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(800, 800);
    context.captureAt(0.05);
    context.pen.rectMode(CENTER);
    context.pen.noFill();
  }

  void draw(Pen& pen) override {
    constexpr int kCells = 10;
    constexpr float kCell = 80.0f;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const float angle = 45.0f + 45.0f * std::sin(clock * 0.42f);
    pen.background(0);
    pen.stroke(255);
    pen.strokeWeight(1.0f);
    for (int column = 0; column < kCells; ++column) {
      for (int row = 0; row < kCells; ++row) {
        pen.push();
        pen.translate((column + 0.5f) * kCell, (row + 0.5f) * kCell);
        pen.rotate(angle);
        pen.rect(0, 0, kCell, kCell);
        pen.pop();
      }
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableGrid, "observable_grid",
                "Draw · Observable reproductions",
                "A ten-by-ten p5 square grid sharing one rotating control.")
