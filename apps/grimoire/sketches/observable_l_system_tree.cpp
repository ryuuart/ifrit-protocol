/** @file
 * observable_l_system_tree — a bracketed branching Lindenmayer tree.
 */

// TAGS: Drawing/Generative, Patterns/Ornament

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
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

struct Branch {
  SkPoint from;
  SkPoint to;
  float shade;
};

struct ObservableLSystemTree {
  std::string sentence = "F";

  void setup(sketch::SketchContext& context) {
    context.canvas(800, 800);
    context.captureAt(0.05);
    for (int generation = 0; generation < 4; ++generation)
      sentence = grow(sentence);

    context.composer.render(
        compose::graphics("observable_l_system_tree.loop", [this](Pen& pen) {
          draw(pen);
        }).inset(0));
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.strokeCap(ROUND);
    }
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const float turn = radians(25.0f + 3.0f * std::sin(clock * 0.55f));
    Turtle turtle{{0, 0}, -HALF_PI};
    std::vector<Turtle> stack;
    stack.reserve(32);
    std::vector<Branch> branches;
    branches.reserve(sentence.size() / 2);
    float left = 0, top = 0, right = 0, bottom = 0;

    pen.background(51);
    pen.noFill();
    for (size_t index = 0; index < sentence.size(); ++index) {
      const char symbol = sentence[index];
      if (symbol == 'F') {
        const SkPoint next = turtle.point + SkPoint{std::cos(turtle.angle),
                                                    std::sin(turtle.angle)};
        const float shade =
            100.0f + 155.0f * static_cast<float>(index) / sentence.size();
        branches.push_back({turtle.point, next, shade});
        left = std::min(left, next.x());
        top = std::min(top, next.y());
        right = std::max(right, next.x());
        bottom = std::max(bottom, next.y());
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
    const float scale = std::min((pen.width - 64.0f) / (right - left),
                                 (pen.height - 64.0f) / (bottom - top));
    pen.push();
    pen.translate((pen.width - (left + right) * scale) * 0.5f,
                  (pen.height - (top + bottom) * scale) * 0.5f);
    pen.scale(scale);
    pen.strokeWeight(1.0f / scale);
    for (const Branch& branch : branches) {
      pen.stroke(branch.shade);
      pen.line(branch.from.x(), branch.from.y(), branch.to.x(), branch.to.y());
    }
    pen.pop();
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableLSystemTree, "observable_l_system_tree",
                "Draw · Observable reproductions",
                "A bracketed p5 turtle expands one branching production.")
