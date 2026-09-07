/** @file
 * observable_l_system_tree — a bracketed branching Lindenmayer tree.
 */

#include <sigilsketch/draw/Draw.h>

#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

std::string grow(std::string_view source) {
  std::string result;
  result.reserve(source.size() * 8);
  for (char symbol : source)
    result += symbol == 'F' ? "FF+[+F-F-F]-[-F+F+F]" : std::string(1, symbol);
  return result;
}

struct Turtle {
  SkPoint point;
  float angle;
};

struct ObservableLSystemTree final : sketch::DrawSketch {
  std::string sentence = "F";

  void setup(sketch::DrawContext& context) override {
    context.canvas(800, 800);
    context.captureAt(0.05);
    context.pen.strokeCap(ROUND);
    for (int generation = 0; generation < 4; ++generation)
      sentence = grow(sentence);
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const float turn = radians(25.0f + 3.0f * std::sin(clock * 0.55f));
    constexpr float kLength = 15.625f;
    Turtle turtle{{pen.width / 3.0f, pen.height}, -HALF_PI};
    std::vector<Turtle> stack;
    stack.reserve(32);

    pen.background(51);
    pen.noFill();
    pen.strokeWeight(1.0f);
    for (size_t index = 0; index < sentence.size(); ++index) {
      const char symbol = sentence[index];
      if (symbol == 'F') {
        const SkPoint next =
            turtle.point + SkPoint{std::cos(turtle.angle) * kLength,
                                   std::sin(turtle.angle) * kLength};
        const float shade =
            100.0f + 155.0f * static_cast<float>(index) / sentence.size();
        pen.stroke(shade);
        pen.line(turtle.point.x(), turtle.point.y(), next.x(), next.y());
        turtle.point = next;
      } else if (symbol == '+') {
        turtle.angle += turn;
      } else if (symbol == '-') {
        turtle.angle -= turn;
      } else if (symbol == '[') {
        stack.push_back(turtle);
      } else if (symbol == ']' && !stack.empty()) {
        turtle = stack.back();
        stack.pop_back();
      }
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableLSystemTree, "observable_l_system_tree",
                "Draw · Observable reproductions",
                "A bracketed p5 turtle expands one branching production.")
