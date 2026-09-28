/** @file
 * p5_fractal_garden — a recursive radial tree whose geometry and material
 * share the sketch clock.
 *
 * The branches are generated with p5-style coordinates by one recursive
 * function, then batched by depth through the canvas the Pen lends. Their ink
 * is not an RGB stroke: it is one live material with a procedural grain child,
 * resolved against the canvas and time on every frame. The centre uses a
 * second material fitted to its circle, so the glow needs no raster asset.
 */

// TAGS: Drawing/Generative

#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/paint/Bases.h>
#include <include/core/SkPathBuilder.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace field = sigil::material::field;
namespace material = sigil::material;
using namespace sigil::draw;

namespace {

constexpr int kTrunks = 6;
constexpr int kDepth = 7;
constexpr float kFirstLength = 108.0f;

material::Material branchInk(material::Material program) {
  return std::move(program)
      .slot("uGrain", field::grain(0.08f, 3, 17.0f))
      .quantizeTime(30.0f);
}

material::Material ground() {
  return material::from(sigil::material::Color{0.025f, 0.032f, 0.065f, 1.0f}).layer(field::grain(0.012f, 4, 31.0f, 0.6f, 2.2f), {.blend = material::BlendMode::SoftLight, .opacity = 0.22f});
}

material::Material budLight() {
  return sigil::material::radialGradient(
      {0.36f, 0.30f}, 0.92f,
      {{0.00f, {1.00f, 0.98f, 0.82f, 1.0f}},
       {0.30f, {1.00f, 0.62f, 0.30f, 1.0f}},
       {0.72f, {0.42f, 0.30f, 0.90f, 0.92f}},
       {1.00f, {0.03f, 0.05f, 0.16f, 0.12f}}},
      {.extent = material::RadialExtent::ClosestSide});
}

struct P5FractalGarden {
  struct Segment {
    SkPoint from;
    SkPoint to;
  };

  material::Material branches = material::Color{0, 0, 0, 0};
  const material::Material background = ground();
  const material::Material buds = budLight();

  void setup(sketch::SketchContext& context) {
    branches = branchInk(material::shader(context.assets.hub(), context.local("branch.sksl"),
                       {.textures = {{"uGrain", {}}}}));
    context.canvas(900, 900);
    context.background({6 / 255.0f, 8 / 255.0f, 16 / 255.0f, 1});
    context.captureAt(0.05);  // the tree is a direct function of the clock

    context.composer.render(
        compose::graphics("p5_fractal_garden.loop", [this](Pen& pen) {
          draw(pen);
        }).inset(0));
  }

  void branch(std::array<std::vector<Segment>, kDepth + 1>& levels,
              std::vector<SkPoint>& tips, float x, float y, float length,
              float angle, int depth, float clock, int lineage) {
    const float x2 = x + std::cos(angle) * length;
    const float y2 = y + std::sin(angle) * length;
    levels[depth].push_back({{x, y}, {x2, y2}});

    if (depth == 0) {
      tips.push_back({x2, y2});
      return;
    }

    const float phase = clock * 0.55f + lineage * 0.71f;
    const float spread = 0.40f + 0.055f * std::sin(phase);
    const float sway = 0.035f * std::sin(clock * 0.8f + depth * 0.9f + lineage);
    branch(levels, tips, x2, y2, length * 0.725f, angle - spread + sway,
           depth - 1, clock, lineage * 2 + 1);
    branch(levels, tips, x2, y2, length * 0.725f, angle + spread + sway,
           depth - 1, clock, lineage * 2 + 2);
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.angleMode(RADIANS);
      pen.strokeCap(ROUND);
      pen.noFill();
    }
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(background);

    std::array<std::vector<Segment>, kDepth + 1> levels;
    std::vector<SkPoint> tips;
    tips.reserve(kTrunks * (1 << kDepth));
    const float turn = clock * 0.075f;
    for (int trunk = 0; trunk < kTrunks; ++trunk) {
      const float angle = -HALF_PI + trunk * TAU / kTrunks + turn;
      branch(levels, tips, pen.width * 0.5f, pen.height * 0.5f, kFirstLength,
             angle, kDepth, clock, trunk + 1);
    }

    pen.stroke(material::skia::paint(branches), CANVAS);
    for (int depth = kDepth; depth >= 0; --depth) {
      pen.strokeWeight(1.35f + 0.52f * depth);
      SkPathBuilder path;
      for (const Segment& segment : levels[depth]) {
        path.moveTo(segment.from);
        path.lineTo(segment.to);
      }
      if (const SkPaint* stroke = pen.strokePaint())
        pen.canvas()->drawPath(path.detach(), *stroke);
    }

    pen.blendMode(ADD);
    pen.strokeWeight(7.0f + 2.0f * (0.5f + 0.5f * std::sin(clock * 2.1f)));
    if (const SkPaint* stroke = pen.strokePaint())
      pen.canvas()->drawPoints(SkCanvas::kPoints_PointMode,
                               SkSpan<const SkPoint>(tips.data(), tips.size()),
                               *stroke);

    pen.push();
    pen.blendMode(BLEND);
    pen.noStroke();
    pen.fill(material::skia::paint(buds), SHAPE);
    pen.circle(pen.width * 0.5f, pen.height * 0.5f,
               42.0f + 5.0f * std::sin(clock * 1.3f));
    pen.pop();
    pen.blendMode(BLEND);
  }
};

}  // namespace

SIGIL_SKETCH_AS(P5FractalGarden, "p5_fractal_garden", "Draw · Generative",
                "A breathing radial fractal in live grained ink and "
                "shape-fitted material light.")
