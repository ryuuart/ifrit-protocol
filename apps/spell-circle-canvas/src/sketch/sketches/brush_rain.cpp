// brush_rain.cpp — mixed tools carried through a changing direction field.
//
// Each gesture keeps its own field direction and phase. The field bends the
// centreline while the selected tool supplies edge character, grain and taper.

// TAGS: Drawing/Brushes

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Brush.h>
#include <sigilsketch/canvas/Sketch.h>

#include <array>
#include <cmath>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
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

/** THE CYCLE: one tool per place in it, with the width it is asked for
 *  and the bristle count a tool that wants its own states. The
 *  watercolour stands in two of the seven, so it falls twice as often as
 *  the rest. */
struct Mark {
  brush::Tool (*make)(SkColor4f, float);
  float low, high;
  int bristles = 0;  // 0: the tool's own
};
const std::array<Mark, 7> kCycle{{
    {brush::spray, 18, 34},
    {brush::watercolor, 10, 23, 22},
    {brush::watercolor, 10, 23, 22},
    {brush::charcoal, 5, 11},
    {brush::marker, 4, 9},
    {brush::pencil, 1.1f, 2.7f},
    {brush::pencil, 1.1f, 2.7f},
}};

struct BrushRain {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(840, 840);
    ctx.captureAt(0.25);

    ctx.composer.render(compose::graphics("brush_rain.sheet", [this](Pen& pen) {
                          draw(pen);
                        }).inset(0));
  }

  void draw(Pen& pen) {
    pen.randomSeed(0xB125A1u);
    pen.noiseSeed(0x5EA8EDu);
    pen.background(249, 246, 231);

    for (int stroke = 0; stroke < 112; ++stroke) {
      const SkColor4f color = kPigments[(size_t)pen.random(kPigments.size())];
      const Mark& mark = kCycle[(size_t)(stroke % (int)kCycle.size())];
      brush::Tool tool = mark.make(color, pen.random(mark.low, mark.high));
      if (mark.bristles != 0) tool.bristles = mark.bristles;
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
