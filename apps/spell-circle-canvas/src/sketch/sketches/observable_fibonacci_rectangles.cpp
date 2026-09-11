/** @file
 * observable_fibonacci_rectangles — signed Fibonacci tiles around the origin.
 */

#include <sigilsketch/draw/Draw.h>

#include <array>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

int signFor(int index) {
  constexpr std::array<int, 4> signs = {-1, 1, 1, -1};
  return signs[index % signs.size()];
}

struct ObservableFibonacciRectangles final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(800, 800);
    context.captureAt(0.05);
    context.pen.colorMode(HSB, 100);
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const int count =
        7 + static_cast<int>(5.0f * (0.5f + 0.5f * std::sin(clock * 0.72f)));
    std::vector<int> fibonacci = {0, 1, 1, 2};
    while (static_cast<int>(fibonacci.size()) < count)
      fibonacci.push_back(fibonacci[fibonacci.size() - 2] + fibonacci.back());

    std::vector<SkRect> tiles;
    SkRect bounds = SkRect::MakeEmpty();
    float x = 0.0f;
    float y = 0.0f;
    for (int index = 1; index < static_cast<int>(fibonacci.size()) - 1;
         ++index) {
      const SkRect tile =
          SkRect::MakeXYWH(x, y,
                           (float)(signFor(index + 1) * fibonacci[index - 1]),
                           (float)(signFor(index) * fibonacci[index]))
              .makeSorted();
      tiles.push_back(tile);
      bounds.join(tile);
      if (index % 2 == 1)
        x += signFor(index) * (fibonacci[index] + fibonacci[index - 1]);
      else
        y += signFor(index) * (fibonacci[index] + fibonacci[index + 1]);
    }
    const float scalar = std::min((pen.width - 96) / bounds.width(),
                                  (pen.height - 96) / bounds.height());
    pen.background(0);
    pen.push();
    pen.translate(pen.width * 0.5f, pen.height * 0.5f);
    pen.scale(scalar);
    pen.translate(-bounds.centerX(), -bounds.centerY());
    pen.strokeWeight(1.0f / scalar);
    for (size_t index = 0; index < tiles.size(); ++index) {
      const SkRect tile = tiles[index];
      pen.fill(static_cast<float>((10 * (index + 1)) % 100), 40, 100);
      pen.stroke(0, 34);
      pen.rect(tile.x(), tile.y(), tile.width(), tile.height());
    }
    pen.pop();
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableFibonacciRectangles,
                "observable_fibonacci_rectangles",
                "Draw · Observable reproductions",
                "Successive Fibonacci values place signed HSB rectangles.")
