/** @file
 * observable_random_walker — a four-direction walk constrained to the canvas.
 */

// TAGS: Drawing/Generative

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace chance = sigil::core::chance;
using namespace sigil::draw;

namespace {

/** One word keyed on @p value, from the library's mixer. */
uint32_t hash(uint32_t value) { return chance::Stream::mix64(value).bits(); }

struct ObservableRandomWalker final : sketch::Sketch {
  void setup(sketch::SketchContext& context) override {
    context.canvas(800, 800);
    context.captureAt(0.05);

    context.composer.render(compose::graphics("observable_random_walker.loop",
                                              [this](Pen& pen) { draw(pen); })
                                .absolute()
                                .inset(0));
  }

  void draw(Pen& pen) {
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
