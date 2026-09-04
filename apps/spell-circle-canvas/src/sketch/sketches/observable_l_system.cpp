/** @file
 * observable_l_system — a turtle interpreting a two-rule Lindenmayer word.
 */

#include <sigilsketch/draw/Draw.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

std::string rewrite(std::string_view source) {
  std::string result;
  result.reserve(source.size() * 8);
  for (char symbol : source) {
    if (symbol == 'A')
      result += "-BF+AFA+FB-";
    else if (symbol == 'B')
      result += "+AF-BFB-FA+";
    else
      result += symbol;
  }
  return result;
}

float radiusFor(size_t index) {
  uint32_t value = static_cast<uint32_t>(index) * 0x9e3779b9u + 0x1D6100A1u;
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  return 6.0f + static_cast<float>(value & 1023u) / 1023.0f * 14.0f;
}

struct ObservableLSystem final : sketch::DrawSketch {
  std::string sentence = "A";

  void setup(sketch::DrawContext& context) override {
    context.canvas(800, 800);
    context.captureAt(0.05);
    context.pen.stroke(255);
    for (int generation = 0; generation < 5; ++generation)
      sentence = rewrite(sentence);
  }

  void draw(Pen& pen) override {
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const float cycle = 0.5f - 0.5f * std::cos(clock * 0.42f);
    const size_t visible = std::max<size_t>(
        1, static_cast<size_t>(sentence.size() * (0.18f + cycle * 0.82f)));
    float x = 0.0f;
    float y = pen.height - 1.0f;
    float angle = 0.0f;
    constexpr float kStep = 53.3f;

    pen.background(0);
    pen.stroke(255);
    pen.strokeWeight(1.0f);
    for (size_t index = 0; index < std::min(visible, sentence.size());
         ++index) {
      const char symbol = sentence[index];
      if (symbol == 'F') {
        const float x2 = x + kStep * std::cos(angle);
        const float y2 = y + kStep * std::sin(angle);
        pen.line(x, y, x2, y2);
        x = x2;
        y = y2;
      } else if (symbol == '+') {
        angle += HALF_PI;
      } else if (symbol == '-') {
        angle -= HALF_PI;
      } else {
        pen.noStroke();
        pen.fill(255);
        pen.circle(x, y, radiusFor(index));
        pen.stroke(255);
      }
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableLSystem, "observable_l_system",
                "Draw · Observable reproductions",
                "A two-rule Lindenmayer word is revealed by a p5 turtle.")
