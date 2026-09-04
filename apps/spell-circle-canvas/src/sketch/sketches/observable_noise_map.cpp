/** @file
 * observable_noise_map — a tiled view of coherent two-dimensional noise.
 */

#include <sigilsketch/draw/Draw.h>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

struct ObservableNoiseMap final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(720, 720);
    context.captureAt(0.05);
    context.pen.noiseSeed(0xA4CD8346u);
    context.pen.noStroke();
  }

  void draw(Pen& pen) override {
    constexpr int kStep = 12;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(0);
    for (int x = 0; x < static_cast<int>(pen.width); x += kStep) {
      for (int y = 0; y < static_cast<int>(pen.height); y += kStep) {
        const float value =
            255.0f * pen.noise(x * 0.01f, y * 0.01f, clock * 0.075f);
        pen.fill(value);
        pen.rect(static_cast<float>(x), static_cast<float>(y), kStep, kStep);
      }
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableNoiseMap, "observable_noise_map",
                "Draw · Observable reproductions",
                "Coherent p5-style noise sampled into a grayscale tile map.")
