/** @file
 * p5_liquid_layers — animated liquid brush ribbons over precise graphics.
 *
 * A baked grid material and exact vector rings make the underpainting. Broad
 * pressure-shaped nibs, wet fibres and small pigment blooms then cross it in
 * layers. The random stream restarts each frame, so only the authored control
 * points move and the brush texture does not flicker in a captured sequence.
 */

#include <sigildraw/brush/Brush.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/draw/Draw.h>

#include <array>
#include <cmath>

namespace sketch = sigil::sketch;
namespace brush = sigil::draw::brush;
namespace mskia = sigil::material::skia;
namespace pattern = sigil::material::pattern;
using namespace sigil::draw;

namespace {

constexpr int kRibbons = 6;
constexpr std::array<SkColor4f, 4> kPigment{{
    {0.08f, 0.82f, 0.88f, 1.0f},
    {0.56f, 0.18f, 0.94f, 1.0f},
    {1.00f, 0.36f, 0.16f, 1.0f},
    {0.96f, 0.72f, 0.14f, 1.0f},
}};

mskia::Paint graphPaper() {
  pattern::Tile fine =
      pattern::gridLines(34.0f, 1.15f, {0.18f, 0.46f, 0.58f, 0.34f});
  pattern::Tile coarse =
      pattern::gridLines(136.0f, 2.2f, {0.70f, 0.82f, 0.88f, 0.22f});
  return mskia::Paint::blend(
      {{mskia::Paint::solid({0.018f, 0.035f, 0.070f, 1.0f}),
        SkBlendMode::kSrcOver},
       {mskia::Paint::shader(fine.texture().shader()), SkBlendMode::kSrcOver},
       {mskia::Paint::shader(coarse.texture().shader()),
        SkBlendMode::kSrcOver}});
}

brush::Tool liquidNib(SkColor4f colour, float width) {
  brush::Tool tool;
  tool.tip = brush::Tip::Nib;
  tool.color = colour;
  tool.width = width;
  tool.spacing = 6.0f;
  tool.opacity = 0.19f;
  tool.pressure = {0.08f, 1.18f, 0.10f};
  tool.pressure.variation.reset();
  tool.blend = SCREEN;
  tool.sharpness = 0.16f;
  tool.noise = 0.12f;
  return tool;
}

struct P5LiquidLayers final : sketch::DrawSketch {
  const mskia::Paint ground = graphPaper();

  void setup(sketch::DrawContext& context) override {
    context.canvas(720, 560);
    context.background(4, 9, 18);
    context.captureAt(0.05);  // the painting is a direct function of the clock
    context.pen.strokeCap(ROUND);
    context.pen.strokeJoin(ROUND);
    context.pen.noFill();
  }

  std::array<brush::Sample, 6> controls(int ribbon, float clock, float width,
                                        float height) const {
    std::array<brush::Sample, 6> path;
    for (int point = 0; point < 6; ++point) {
      const float along = static_cast<float>(point) / 5.0f;
      const float x = -width * 0.10f + along * width * 1.20f;
      const float base = height * (ribbon + 1.0f) / (kRibbons + 1.0f);
      const float wave = height * 0.105f *
                         std::sin(along * TAU * 1.35f + ribbon * 0.8f +
                                  clock * (0.34f + ribbon * 0.018f));
      const float cross =
          height * 0.032f * std::sin(clock * 0.53f + point + ribbon);
      const float pressure = 0.22f + 0.85f * std::sin(along * PI);
      path[point] = {{x, base + wave + cross}, pressure};
    }
    return path;
  }

  void draw(Pen& pen) override {
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.randomSeed(0x11A71Du);
    pen.background(ground);

    pen.noFill();
    pen.stroke(218, 235, 242, 92);
    pen.strokeWeight(2.0f);
    for (int ring = 0; ring < 7; ++ring) {
      const float diameter = 76.0f + ring * 61.0f;
      pen.circle(pen.width * 0.5f, pen.height * 0.5f, diameter);
    }
    pen.beginShape();
    for (int point = 0; point < 12; ++point) {
      const float angle = point * TAU / 12.0f + clock * 0.08f;
      const float radius = point % 2 == 0 ? 234.0f : 82.0f;
      pen.vertex(pen.width * 0.5f + std::cos(angle) * radius,
                 pen.height * 0.5f + std::sin(angle) * radius);
    }
    pen.endShape(CLOSE);

    for (int ribbon = 0; ribbon < kRibbons; ++ribbon) {
      const SkColor4f colour = kPigment[ribbon % kPigment.size()];
      const std::array<brush::Sample, 6> path =
          controls(ribbon, clock, pen.width, pen.height);

      brush::Tool body = liquidNib(colour, 56.0f + 8.0f * (ribbon % 3));
      brush::spline(pen, body, path, 0.74f);

      brush::Tool wet = brush::watercolor(colour, body.width * 0.74f);
      wet.spacing = 8.0f;
      wet.bristles = 10;
      wet.opacity = 0.12f;
      wet.scatter = 0.52f;
      wet.pressure = body.pressure;
      wet.blend = SCREEN;
      brush::spline(pen, wet, path, 0.74f);
    }

    brush::Tool bloom = brush::spray({1.0f, 0.68f, 0.24f, 1.0f}, 52.0f);
    bloom.opacity = 0.10f;
    bloom.blend = ADD;
    for (int mark = 0; mark < 14; ++mark) {
      const float angle = mark * TAU / 14.0f + clock * 0.12f;
      const SkPoint at{pen.width * 0.5f + std::cos(angle) * 168.0f,
                       pen.height * 0.5f + std::sin(angle) * 130.0f};
      brush::line(
          pen, bloom, at,
          {at.x() + std::cos(angle) * 8.0f, at.y() + std::sin(angle) * 8.0f});
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(P5LiquidLayers, "p5_liquid_layers", "Draw · Generative",
                "Layered liquid nibs, wet fibres and pigment blooms moving "
                "over a precise generated graphic.")
