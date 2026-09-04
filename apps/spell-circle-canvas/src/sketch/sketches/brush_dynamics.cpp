// brush_dynamics.cpp — one dab executor driven by four stylus sensors.
//
// Each lane changes one observation while keeping the geometry and pigment
// stable, making pressure, tilt, barrel rotation and speed response readable
// without an attached tablet.

#include <sigildraw/kit/Brushwork.h>
#include <sigilsketch/draw/Draw.h>

#include <array>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

constexpr std::array<SkColor4f, 4> kInk{{
    {0.10f, 0.28f, 0.38f, 1},
    {0.77f, 0.20f, 0.14f, 1},
    {0.16f, 0.46f, 0.33f, 1},
    {0.55f, 0.31f, 0.12f, 1},
}};

std::vector<brush::Input> lane(float y, int count = 80) {
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

struct BrushDynamics final : sketch::DrawSketch {
  void setup(sketch::DrawContext& context) override {
    context.canvas(1000, 760);
    context.captureAt(0.25);
    context.pen.randomSeed(0xD1A6A1C5u);
    context.pen.noiseSeed(0xD1A6A1C5u);
  }

  void draw(Pen& pen) override {
    pen.background(248, 241, 224);

    std::vector<brush::Input> pressure = lane(170);
    for (size_t index = 0; index < pressure.size(); ++index) {
      const float t = (float)index / (float)(pressure.size() - 1);
      pressure[index].pressure = 0.12f + std::sin(t * PI) * 0.98f;
    }
    brush::Brush pressureTool = brush::marker(kInk[0], 28.0f);
    pressureTool.opacity = 0.58f;
    pressureTool.scatter = 0.4f;
    pressureTool.markerTip = false;
    pressureTool.pressure = {1, 1, 1};
    pressureTool.pressure.variation.reset();
    brush::deposit(pen, pressureTool, brush::dabs(pressure, 2.0f));
    label(pen, "PRESSURE", 170);

    std::vector<brush::Input> tilt = lane(320);
    for (size_t index = 0; index < tilt.size(); ++index) {
      const float t = (float)index / (float)(tilt.size() - 1);
      tilt[index].tilt = 0.15f + 0.85f * std::sin(t * PI);
      tilt[index].tiltDirection = -HALF_PI + t * PI;
    }
    brush::Brush tiltTool = brush::marker(kInk[1], 24.0f);
    tiltTool.opacity = 0.5f;
    tiltTool.scatter = 0.0f;
    tiltTool.aspect = 0.18f;
    tiltTool.rotation = brush::Rotation::Tilt;
    tiltTool.tiltAspect = 1.8f;
    tiltTool.tiltOffset = 0.65f;
    tiltTool.markerTip = false;
    tiltTool.pressure = {1, 1, 1};
    tiltTool.pressure.variation.reset();
    brush::deposit(pen, tiltTool, brush::dabs(tilt, 5.0f));
    label(pen, "TILT", 320);

    std::vector<brush::Input> barrel = lane(470);
    for (size_t index = 0; index < barrel.size(); ++index) {
      const float t = (float)index / (float)(barrel.size() - 1);
      barrel[index].barrelRotation = t * TWO_PI * 2.0f;
    }
    brush::Brush barrelTool = brush::marker(kInk[2], 25.0f);
    barrelTool.opacity = 0.52f;
    barrelTool.scatter = 0.0f;
    barrelTool.aspect = 0.12f;
    barrelTool.rotation = brush::Rotation::Fixed;
    barrelTool.markerTip = false;
    barrelTool.pressure = {1, 1, 1};
    barrelTool.pressure.variation.reset();
    brush::deposit(pen, barrelTool, brush::dabs(barrel, 6.0f));
    label(pen, "BARREL", 470);

    std::vector<brush::Input> speed = lane(620);
    double seconds = 0.0;
    for (size_t index = 0; index < speed.size(); ++index) {
      const float t = (float)index / (float)(speed.size() - 1);
      seconds += 0.003 + 0.045 * std::pow(std::sin(t * PI), 2.0f);
      speed[index].seconds = seconds;
    }
    brush::Brush speedTool = brush::spray(kInk[3], 25.0f);
    speedTool.opacity = 0.32f;
    speedTool.speedReference = 520.0f;
    speedTool.speedSize = 0.82f;
    speedTool.speedOpacity = 0.74f;
    speedTool.pressure = {1, 1, 1};
    speedTool.pressure.variation.reset();
    brush::deposit(pen, speedTool, brush::dabs(speed, 3.0f));
    label(pen, "SPEED", 620);

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
