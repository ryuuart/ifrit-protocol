/** @file
 * observable_random_walker — a four-direction walk constrained to the canvas.
 */

#include <sigilsketch/draw/Draw.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

uint32_t hash(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  return value ^ (value >> 16);
}

struct ObservableRandomWalker final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(800, 800);
    context.captureAt(0.05);
  }

  void draw(Pen& pen) override {
    constexpr float kStep = 20.0f;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const int visible =
        120 + static_cast<int>(std::fmod(clock * 72.0f, 320.0f));
    float x = pen.width * 0.5f;
    float y = pen.height * 0.5f;
    pen.background(0);
    pen.stroke(0);
    for (int step = 0; step < visible; ++step) {
      switch (hash(step + 0xE5878E94u) & 3u) {
        case 0:
          x += kStep;
          break;
        case 1:
          x -= kStep;
          break;
        case 2:
          y += kStep;
          break;
        default:
          y -= kStep;
          break;
      }
      x = std::clamp(x, 0.0f, pen.width - 1.0f);
      y = std::clamp(y, 0.0f, pen.height - 1.0f);
      const float distance =
          std::hypot(x - pen.width * 0.5f, y - pen.height * 0.5f);
      const float shade = std::clamp(
          255.0f * (1.0f - distance / (pen.width * 0.5f)), 0.0f, 255.0f);
      pen.fill(shade);
      pen.circle(x, y, kStep);
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableRandomWalker, "observable_random_walker",
                "Draw · Observable reproductions",
                "A constrained four-way walker leaves distance-shaded discs.")
