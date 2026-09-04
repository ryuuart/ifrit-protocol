/** @file
 * observable_flowfield_3 — random samples of a sine-composed angle field.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSpan.h>
#include <sigilsketch/draw/Draw.h>

#include <cmath>
#include <cstdint>
#include <vector>

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

struct ObservableFlowfield3 final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(720, 720);
    context.captureAt(0.05);
    context.pen.noFill();
    context.pen.strokeCap(SQUARE);
  }

  void draw(Pen& pen) override {
    constexpr int kCount = 14000;
    constexpr float kStep = 20.0f;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(0);
    pen.stroke(255, 190);
    pen.strokeWeight(0.55f);

    std::vector<SkPoint> lines;
    lines.reserve(kCount * 2);
    for (int index = 0; index < kCount; ++index) {
      const float x = sample(index * 2u + 0x6BCECDu) * pen.width;
      const float y = sample(index * 2u + 0x883A00u) * pen.height;
      const float angle = (std::sin(x * 0.01f + clock * 0.22f) +
                           std::sin(y * 0.01f - clock * 0.19f)) *
                          TAU;
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

SIGIL_SKETCH_AS(ObservableFlowfield3, "observable_flowfield_3",
                "Draw · Observable reproductions",
                "Random line samples expose a sine-composed p5 angle field.")
