/** @file
 * observable_flowfield_2 — random samples of a diagonal angle field.
 */

// TAGS: Drawing/Generative

#include <include/core/SkCanvas.h>
#include <include/core/SkSpan.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace chance = sigil::core::chance;
using namespace sigil::draw;

namespace {

/** One number in [0, 1) keyed on @p value. The stream is the library's,
 *  so nothing here carries a mixer of its own. */
float sample(uint32_t value) { return chance::Stream::mix64(value).unit(); }

struct ObservableFlowfield2 final : sketch::Sketch {
  void setup(sketch::SketchContext& context) override {
    context.canvas(720, 720);
    context.captureAt(0.05);

    context.composer.render(compose::graphics("observable_flowfield_2.loop",
                                              [this](Pen& pen) { draw(pen); })
                                .absolute()
                                .inset(0));
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.noFill();
      pen.strokeCap(SQUARE);
    }
    constexpr int kCount = 8000;
    constexpr float kStep = 10.0f;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(0);
    pen.stroke(255, 220);
    pen.strokeWeight(1.0f);

    std::vector<SkPoint> lines;
    lines.reserve(kCount * 2);
    for (int index = 0; index < kCount; ++index) {
      const float x = sample(index * 2u + 0xF7956509u) * pen.width;
      const float y = sample(index * 2u + 0xDC901BEBu) * pen.height;
      const float angle = (x + y) * 0.01f * TAU + clock * 0.28f;
      lines.push_back({x, y});
      lines.push_back(
          {x + std::cos(angle) * kStep, y + std::sin(angle) * kStep});
    }
    if (const SkPaint* stroke = pen.strokePaint())
      pen.canvas()->drawPoints(
          SkCanvas::kLines_PointMode,
          SkSpan<const SkPoint>(lines.data(), lines.size()), *stroke);
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableFlowfield2, "observable_flowfield_2",
                "Draw · Observable reproductions",
                "Random p5-style samples reveal the same diagonal angle field.")
