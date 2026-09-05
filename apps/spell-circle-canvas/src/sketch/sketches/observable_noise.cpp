/** @file
 * observable_noise — coherent noise beside an uncorrelated random polyline.
 */

#include <sigilsketch/draw/Draw.h>

#include <cmath>
#include <cstdint>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

float sample(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  value ^= value >> 16;
  return static_cast<float>(value & 0x00ffffffu) / 16777216.0f;
}

struct ObservableNoise final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(900, 720);
    context.captureAt(0.05);
    context.pen.noiseSeed(0x8B554407u);
    context.pen.noFill();
    context.pen.strokeJoin(ROUND);
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(0);
    pen.stroke(255);
    pen.strokeWeight(5.0f);
    pen.beginShape();
    for (int x = 0; x < static_cast<int>(pen.width * 0.5f); ++x) {
      const float nx = x / pen.width * 10.0f + clock * 0.15f;
      pen.vertex(static_cast<float>(x), pen.height * pen.noise(nx));
    }
    pen.endShape();

    const uint32_t epoch = static_cast<uint32_t>(std::floor(clock * 3.0f));
    pen.stroke(0, 0, 255);
    pen.beginShape();
    for (int x = 0; x <= static_cast<int>(pen.width * 0.5f); x += 50) {
      const float y = pen.height * sample(epoch * 131u + x + 0x448u);
      pen.vertex(pen.width * 0.5f + x, y);
    }
    pen.endShape();
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableNoise, "observable_noise",
                "Draw · Observable reproductions",
                "A coherent Perlin curve is compared with random samples.")
