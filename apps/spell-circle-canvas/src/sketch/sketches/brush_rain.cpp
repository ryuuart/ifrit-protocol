// brush_rain.cpp — mixed tools carried through a changing direction field.
//
// Each gesture keeps its own field direction and phase. The field bends the
// centreline while the selected tool supplies edge character, grain and taper.

#include <sigildraw/brush/Brush.h>
#include <sigilsketch/draw/Draw.h>

#include <array>
#include <cmath>

namespace sketch = sigil::sketch;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

struct Seabed {
  float direction = 0.0f;
  float phase = 0.0f;

  float operator()(SkPoint point, float) const {
    return direction + std::sin(point.fX * 0.009f + phase) * 0.24f +
           std::cos(point.fY * 0.012f - phase * 0.7f) * 0.18f;
  }
};

constexpr std::array<SkColor4f, 6> kPigments{{
    {0.08f, 0.31f, 0.27f, 1},
    {0.20f, 0.66f, 0.54f, 1},
    {0.29f, 0.52f, 0.68f, 1},
    {0.91f, 0.27f, 0.18f, 1},
    {0.98f, 0.69f, 0.03f, 1},
    {0.16f, 0.19f, 0.28f, 1},
}};

struct BrushRain final : sketch::DrawSketch {
  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(840, 840);
    ctx.captureAt(0.25);
    ctx.pen.randomSeed(0xB125A1u);
    ctx.pen.noiseSeed(0x5EA8EDu);
  }

  void draw(sketch::DrawContext& ctx) override {
    Pen& pen = ctx.pen;
    pen.background(249, 246, 231);

    for (int stroke = 0; stroke < 112; ++stroke) {
      const SkColor4f color = kPigments[(size_t)pen.random(kPigments.size())];
      brush::Tool tool;
      switch (stroke % 7) {
        case 0:
          tool = brush::spray(color, pen.random(18, 34));
          break;
        case 1:
        case 2:
          tool = brush::watercolor(color, pen.random(10, 23));
          tool.bristles = 22;
          break;
        case 3:
          tool = brush::charcoal(color, pen.random(5, 11));
          break;
        case 4:
          tool = brush::marker(color, pen.random(4, 9));
          break;
        default:
          tool = brush::pencil(color, pen.random(1.1f, 2.7f));
          break;
      }
      tool.opacity *= pen.random(0.7f, 1.1f);
      tool.pressure = {pen.random(0.08f, 0.45f), pen.random(0.78f, 1.25f),
                       pen.random(0.05f, 0.38f)};
      const float direction = pen.random(-0.35f, 0.35f);
      const Seabed field{direction, pen.random(0.0f, TWO_PI)};
      const SkPoint start{pen.random(-80, 740), pen.random(25, 815)};
      brush::flowLine(pen, tool, start, pen.random(110, 310), 0, field);
    }

    pen.noStroke();
    pen.fill(24, 30, 30, 226);
    pen.textAlign(CENTER, CENTER);
    pen.textSize(55);
    pen.text("SIGILDRAW", 420, 420);
    pen.noLoop();
  }
};

}  // namespace

SIGIL_SKETCH(BrushRain, "Draw · Procedural",
             "Mixed natural-media marks carried through a seabed field.")
