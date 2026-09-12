/** @file
 * observable_noise — coherent noise beside an uncorrelated random polyline.
 */

// TAGS: Drawing/Generative, Patterns/Noise

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <cstdint>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace chance = sigil::core::chance;
using namespace sigil::draw;

namespace {

/** One number in [0, 1) keyed on @p value. The stream is the library's,
 *  so nothing here carries a mixer of its own. */
float sample(uint32_t value) { return chance::Stream::mix64(value).unit(); }

struct ObservableNoise final : sketch::Sketch {
  void setup(sketch::SketchContext& context) override {
    context.canvas(900, 720);
    context.captureAt(0.05);

    context.composer.render(compose::graphics("observable_noise.loop",
                                              [this](Pen& pen) { draw(pen); })
                                .absolute()
                                .inset(0));
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.noiseSeed(0x8B554407u);
      pen.noFill();
      pen.strokeJoin(ROUND);
    }
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
