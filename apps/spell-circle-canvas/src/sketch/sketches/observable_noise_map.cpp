/** @file
 * observable_noise_map — a tiled view of coherent two-dimensional noise.
 */

// TAGS: Drawing/Generative, Patterns/Noise

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
using namespace sigil::draw;

namespace {

struct ObservableNoiseMap final : sketch::Sketch {
  void setup(sketch::SketchContext& context) override {
    context.canvas(720, 720);
    context.captureAt(0.05);

    context.composer.render(compose::graphics("observable_noise_map.loop",
                                              [this](Pen& pen) { draw(pen); })
                                .absolute()
                                .inset(0));
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.noiseSeed(0xA4CD8346u);
      pen.noStroke();
    }
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
