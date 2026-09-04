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

  void draw(Pen& pen) override {
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const int count =
        7 + static_cast<int>(5.0f * (0.5f + 0.5f * std::sin(clock * 0.72f)));
    std::vector<int> fibonacci = {0, 1, 1, 2};
    while (static_cast<int>(fibonacci.size()) < count)
      fibonacci.push_back(fibonacci[fibonacci.size() - 2] + fibonacci.back());

    const float scalar = pen.width / (2.0f * fibonacci.back());
    float x = 0.0f;
    float y = 0.0f;
    pen.background(0);
    pen.push();
    pen.translate(pen.width * 0.5f, pen.height * 0.5f);
    for (int index = 1; index < static_cast<int>(fibonacci.size()) - 1;
         ++index) {
      pen.fill(static_cast<float>((10 * index) % 100), 40, 100);
      pen.stroke(0, 34);
      pen.rect(scalar * x, scalar * y,
               scalar * signFor(index + 1) * fibonacci[index - 1],
               scalar * signFor(index) * fibonacci[index]);
      if (index % 2 == 1)
        x += signFor(index) * (fibonacci[index] + fibonacci[index - 1]);
      else
        y += signFor(index) * (fibonacci[index] + fibonacci[index + 1]);
    }
    pen.pop();
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableFibonacciRectangles,
                "observable_fibonacci_rectangles",
                "Draw · Observable reproductions",
                "Successive Fibonacci values place signed HSB rectangles.")
