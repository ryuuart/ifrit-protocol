// brush_dynamics.cpp — one dab executor driven by four stylus sensors.
//
// Each lane changes one observation while keeping the geometry and pigment
// stable, making pressure, tilt, barrel rotation and speed response readable
// without an attached tablet.

// TAGS: Drawing/Brushes

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Brush.h>
#include <sigilsketch/canvas/Sketch.h>

#include <array>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

constexpr std::array<SkColor4f, 4> kInk{{
    {0.10f, 0.28f, 0.38f, 1},
    {0.77f, 0.20f, 0.14f, 1},
    {0.16f, 0.46f, 0.33f, 1},
    {0.55f, 0.31f, 0.12f, 1},
}};

/** THE STROKE EVERY LANE IS SAMPLED ALONG: one gentle wave across the
 *  sheet at @p y, so what differs between lanes is the observation and
 *  never the geometry. */
std::vector<brush::Input> along(float y, int count = 80) {
  std::vector<brush::Input> input;
  input.reserve((size_t)count);
  for (int index = 0; index < count; ++index) {
    const float t = (float)index / (float)(count - 1);
    input.push_back({.position = {180.0f + t * 740.0f,
                                  y + std::sin(t * TWO_PI * 1.5f) * 12.0f},
                     .seconds = (double)t});
  }
  return input;
}

void label(Pen& pen, const char* text, float y) {
  pen.noStroke();
  pen.fill(42, 39, 34, 220);
  pen.textAlign(RIGHT, CENTER);
  pen.textSize(21);
  pen.text(text, 145, y);
}

/** ONE LANE: the stroke at @p y with @p sense written into every sample
 *  along it, laid down by @p tool at @p spacing and named at the left.
 *  The tool's own pressure is held flat whatever the lane is studying, so
 *  a reader compares one observation and not two. */
template <class Sense>
void lane(Pen& pen, float y, const char* name, brush::Tool tool, float spacing,
          Sense sense) {
  std::vector<brush::Input> input = along(y);
  for (size_t index = 0; index < input.size(); ++index)
    sense(input[index], (float)index / (float)(input.size() - 1));
  tool.pressure = {1, 1, 1};
  tool.pressure.variation.reset();
  brush::deposit(pen, tool, brush::dabs(input, spacing));
  label(pen, name, y);
}

struct BrushDynamics final : sketch::Sketch {
  void setup(sketch::SketchContext& context) override {
    context.canvas(1000, 760);
    context.captureAt(0.25);

    context.composer.render(compose::graphics("brush_dynamics.sheet",
                                              [this](Pen& pen) { draw(pen); })
                                .absolute()
                                .inset(0));
  }

  void draw(Pen& pen) {
    pen.randomSeed(0xD1A6A1C5u);
    pen.noiseSeed(0xD1A6A1C5u);
    pen.background(248, 241, 224);

    brush::Tool pressureTool = brush::marker(kInk[0], 28.0f);
    pressureTool.opacity = 0.58f;
    pressureTool.scatter = 0.4f;
    pressureTool.markerTip = false;
    lane(pen, 170, "PRESSURE", pressureTool, 2.0f,
         [](brush::Input& at, float t) {
           at.pressure = 0.12f + std::sin(t * PI) * 0.98f;
         });

    brush::Tool tiltTool = brush::marker(kInk[1], 24.0f);
    tiltTool.opacity = 0.5f;
    tiltTool.scatter = 0.0f;
    tiltTool.aspect = 0.18f;
    tiltTool.rotation = brush::Rotation::Tilt;
    tiltTool.tiltAspect = 1.8f;
    tiltTool.tiltOffset = 0.65f;
    tiltTool.markerTip = false;
    lane(pen, 320, "TILT", tiltTool, 5.0f, [](brush::Input& at, float t) {
      at.tilt = 0.15f + 0.85f * std::sin(t * PI);
      at.tiltDirection = -HALF_PI + t * PI;
    });

    brush::Tool barrelTool = brush::marker(kInk[2], 25.0f);
    barrelTool.opacity = 0.52f;
    barrelTool.scatter = 0.0f;
    barrelTool.aspect = 0.12f;
    barrelTool.rotation = brush::Rotation::Fixed;
    barrelTool.markerTip = false;
    lane(pen, 470, "BARREL", barrelTool, 6.0f, [](brush::Input& at, float t) {
      at.barrelRotation = t * TWO_PI * 2.0f;
    });

    // The speed lane is the one whose sensor is CUMULATIVE: a dab's speed
    // is the distance over the time since the one before it.
    brush::Tool speedTool = brush::spray(kInk[3], 25.0f);
    speedTool.opacity = 0.32f;
    speedTool.speedReference = 520.0f;
    speedTool.speedSize = 0.82f;
    speedTool.speedOpacity = 0.74f;
    double seconds = 0.0;
    lane(pen, 620, "SPEED", speedTool, 3.0f,
         [&seconds](brush::Input& at, float t) {
           seconds += 0.003 + 0.045 * std::pow(std::sin(t * PI), 2.0f);
           at.seconds = seconds;
         });

    pen.noStroke();
    pen.fill(37, 34, 30, 225);
    pen.textAlign(CENTER, CENTER);
    pen.textSize(30);
    pen.text("STYLUS DYNAMICS", 500, 65);
    pen.noLoop();
  }
};

}  // namespace

SIGIL_SKETCH(BrushDynamics, "Draw · Procedural",
             "Pressure, tilt, barrel rotation and speed through one sampler.")
