// brush_live_tutorial.cpp — six brush constructions in one timed sketch.
//
// The canvas persists between frames, so rain and watercolor can accumulate;
// the other scenes clear and redraw as their field, geometry or pressure
// changes. Every scene uses the same instance-owned engine and seeded streams.

// EDIT THESE FIRST
//   kSceneSeconds  how long each construction remains on screen
//   kPalette       the pigments shared by all six scenes

#include <sigildraw/kit/Brushwork.h>
#include <sigilsketch/draw/Draw.h>

#include <array>
#include <cmath>
#include <string_view>
#include <vector>

namespace sketch = sigil::sketch;
namespace brush = sigil::draw::brush;
using namespace sigil::draw;

namespace {

constexpr float kSceneSeconds = 5.0f;
constexpr float kScale = 1.4f;
constexpr std::array<SkColor4f, 5> kPalette{{
    {0.00f, 0.13f, 0.52f, 1},
    {0.00f, 0.24f, 0.20f, 1},
    {0.99f, 0.83f, 0.00f, 1},
    {1.00f, 0.15f, 0.01f, 1},
    {0.42f, 0.58f, 0.02f, 1},
}};

constexpr std::array<SkColor4f, 6> kRainColors{{
    {0.17f, 0.41f, 0.35f, 1},
    {0.29f, 0.84f, 0.69f, 1},
    {0.50f, 0.67f, 0.78f, 1},
    {0.31f, 0.58f, 0.80f, 1},
    {0.96f, 0.41f, 0.31f, 1},
    {1.00f, 0.83f, 0.00f, 1},
}};

constexpr std::array<std::string_view, 9> kBrushes{{
    "marker",
    "tutorial-watercolor",
    "spray",
    "charcoal",
    "HB",
    "2B",
    "cpencil",
    "2H",
    "rotring",
}};

void label(Pen& pen, std::string_view text, SkColor4f color, float x = 300,
           float y = 300) {
  pen.noStroke();
  pen.fill(color);
  pen.textAlign(CENTER, CENTER);
  pen.textSize(40);
  pen.text(text, x, y);
}

struct BrushLiveTutorial final : sketch::DrawSketch {
  brush::Engine brushes;
  int lastScene = -1;

  void setup(sketch::DrawContext& context) override {
    context.canvas(840, 840);
    context.background(255, 252, 235);
    context.captureAt(12.5);
    context.pen.frameRate(30);
    context.pen.randomSeed(0x213123u);
    context.pen.noiseSeed(0x213123u);

    brushes.angleMode(DEGREES);
    brushes.scaleBrushes(3.5f);

    brush::Brush watercolor = brush::marker(SkColors::kBlack, 10.0f);
    watercolor.tip = brush::Tip::Custom;
    watercolor.scatter = 1.05f;
    watercolor.opacity = 0.085f;
    watercolor.spacing = 2.4f;
    watercolor.pressure = {0.8f, 1.3f, 0.8f};
    watercolor.rotation = brush::Rotation::Natural;
    watercolor.markerTip = false;
    watercolor.customTip = [](Pen& pen, const brush::Dab&) {
      pen.noStroke();
      pen.rectMode(CENTER);
      pen.rect(-0.08f, -0.08f, 0.78f, 0.78f, 0.08f);
      pen.rect(0.34f, 0.34f, 0.31f, 0.31f, 0.05f);
    };
    brushes.add("tutorial-watercolor", watercolor);

    brush::Brush whiteCharcoal =
        brush::charcoal({0.95f, 0.97f, 0.94f, 1}, 1.5f);
    whiteCharcoal.scatter = 2.0f;
    whiteCharcoal.opacity = 0.52f;
    whiteCharcoal.blend = SCREEN;
    brushes.add("white-charcoal", whiteCharcoal);
  }

  void resetBrushes() {
    brushes.noField();
    brushes.noFill();
    brushes.noWash();
    brushes.noHatch();
    brushes.noMass();
    brushes.noStroke();
  }

  void rain(Pen& pen, bool entered) {
    if (entered) {
      pen.background(255, 252, 235);
      pen.randomSeed(0xB125A1u);
    }

    brushes.field("seabed");
    const std::string_view name =
        kBrushes[(size_t)pen.random((float)kBrushes.size())];
    const SkColor4f color =
        kRainColors[(size_t)pen.random((float)kRainColors.size())];
    brushes.set(name, color, pen.random(0.7f, 1.6f));
    brushes.flowLine(pen, {pen.random(600), pen.random(600)},
                     pen.random(140, 240), pen.random(360));
    label(pen, "*SIGILDRAW", {0.04f, 0.05f, 0.05f, 1});
  }

  void fields(Pen& pen, float localSeconds) {
    pen.background(8, 15, 21);
    resetBrushes();

    const std::vector<std::string> names = brushes.listFields();
    if (!names.empty()) {
      const size_t index =
          (size_t)std::floor(localSeconds / 0.72f) % names.size();
      brushes.field(names[index]);
    }

    pen.randomSeed(0x33213u);
    brushes.set("white-charcoal", {0.94f, 0.96f, 0.92f, 1}, 1.0f);
    brushes.circle(pen, 300, 300, 180, 0.3f);

    brushes.pick("HB");
    for (int line = 0; line < 30; ++line)
      brushes.flowLine(pen, {pen.random(600), pen.random(600)}, 75, 0);

    label(pen, "*field()", {0.82f, 0.84f, 0.84f, 1});
  }

  void wheel(Pen& pen, float seconds) {
    pen.background(226, 231, 220);
    resetBrushes();
    brushes.field("seabed");

    constexpr std::array<std::string_view, 8> wheelBrushes{{
        "marker",
        "marker",
        "tutorial-watercolor",
        "tutorial-watercolor",
        "charcoal",
        "HB",
        "2B",
        "rotring",
    }};
    for (int ray = 0; ray < 20; ++ray) {
      const float angle = (float)ray * 18.0f + seconds * 30.0f;
      pen.randomSeed(0x33213u * (uint64_t)(ray + 1));
      const std::string_view name =
          wheelBrushes[(size_t)pen.random((float)wheelBrushes.size())];
      const SkColor4f color =
          kPalette[(size_t)pen.random((float)kPalette.size())];
      brushes.set(name, color, 1.0f);
      const float radiansValue = radians(-angle);
      brushes.flowLine(pen,
                       {300.0f + 100.0f * std::cos(radiansValue),
                        300.0f + 100.0f * std::sin(radiansValue)},
                       320, angle);
    }

    brushes.noField();
    pen.randomSeed(0x5EEDu + (uint64_t)std::floor(seconds * 2.0f));
    brushes.set(wheelBrushes[(size_t)pen.random((float)wheelBrushes.size())],
                kPalette[(size_t)pen.random((float)kPalette.size())], 1.0f);
    brushes.circle(pen, 300, 300, 100, 0.2f);
    label(pen, "*stroke()", {0.16f, 0.16f, 0.15f, 1});
  }

  void hatches(Pen& pen, float localSeconds) {
    pen.background(255, 230, 212);
    resetBrushes();

    constexpr std::array<SkPoint, 6> roseBase{{
        {80, 150},
        {180, 150},
        {420, 150},
        {480, 450},
        {280, 450},
        {130, 450},
    }};
    constexpr std::array<float, 6> rosePhase{
        {0.4f, 1.7f, 3.2f, 4.8f, 2.4f, 5.6f}};
    std::vector<SkPoint> rose;
    rose.reserve(roseBase.size());
    for (size_t index = 0; index < roseBase.size(); ++index) {
      const float motion = std::sin(rosePhase[index] + localSeconds * 3.2f);
      const float cross = std::sin(rosePhase[(index + 2) % rosePhase.size()] +
                                   localSeconds * 2.7f);
      rose.push_back({roseBase[index].fX + motion * 20.0f,
                      roseBase[index].fY + cross * 20.0f});
    }

    brushes.hatchStyle("HB", {0.78f, 0.38f, 0.51f, 1}, 1.3f);
    brushes.hatch({.spacing = 15.0f, .angle = 45.0f});
    brushes.polygon(pen, rose);

    constexpr std::array<SkPoint, 3> goldBase{{
        {250, 250},
        {500, 300},
        {300, 520},
    }};
    constexpr std::array<float, 3> goldPhase{{2.1f, 4.3f, 0.8f}};
    std::vector<SkPoint> gold;
    gold.reserve(goldBase.size());
    for (size_t index = 0; index < goldBase.size(); ++index) {
      const float swing = std::sin(goldPhase[index] + localSeconds * 2.5f);
      const float lift = std::cos(goldPhase[index] + localSeconds * 2.1f);
      gold.push_back({goldBase[index].fX + swing * (20.0f + index * 10.0f),
                      goldBase[index].fY + lift * (20.0f + index * 8.0f)});
    }

    brushes.hatchStyle("marker", {0.88f, 0.71f, 0.07f, 1}, 0.18f);
    brushes.hatch({.spacing = 10.0f, .angle = 130.0f, .jitter = 0.10f});
    brushes.polygon(pen, gold);
    brushes.noHatch();
    label(pen, "*hatch()", {0.20f, 0.18f, 0.17f, 1});
  }

  void watercolor(Pen& pen, bool entered) {
    if (entered) {
      pen.background(255, 252, 235);
      pen.randomSeed(0xF111u);
    }
    if (pen.frameCount % 5 == 0) {
      constexpr std::array<SkColor4f, 6> pigments{{
          {0.48f, 0.28f, 0.00f, 1},
          {0.00f, 0.13f, 0.52f, 1},
          {0.00f, 0.24f, 0.20f, 1},
          {0.99f, 0.83f, 0.00f, 1},
          {1.00f, 0.15f, 0.01f, 1},
          {0.42f, 0.58f, 0.02f, 1},
      }};
      resetBrushes();
      brushes.set("marker", {0.88f, 0.71f, 0.07f, 1}, 0.08f);
      brushes.fill(pigments[(size_t)pen.random((float)pigments.size())],
                   pen.random(60, 110) / 255.0f);
      brushes.fillBleed(pen.random(0.10f, 0.55f));
      brushes.fillTexture(0.4f, 0.4f, true);
      brushes.rect(pen, pen.random(600), pen.random(600), pen.random(50, 140),
                   pen.random(50, 140), brush::RectMode::Center);
      brushes.noFill();
    }
    label(pen, "*fill()", {0.05f, 0.05f, 0.04f, 1});
  }

  void splines(Pen& pen, float localSeconds) {
    pen.background(68, 94, 135);
    resetBrushes();

    pen.randomSeed(0x5A11CEu);
    brushes.set("2B", {0.05f, 0.18f, 0.35f, 1}, 2.0f);
    brushes.circle(pen, 155, 140, 50);

    const float phase = localSeconds * 1.6f;
    const float x = 280.0f - 150.0f * std::cos(std::sin(phase) * TWO_PI);
    const float y = 300.0f + 50.0f * std::sin(phase * 0.83f);
    const float pressure = 1.12f + std::sin(phase * 0.71f) * 0.34f;
    const std::array<brush::Sample, 4> points{{
        {{30, 30}, 1.0f},
        {{250, 100}, 1.18f},
        {{x, y}, pressure},
        {{570, 570}, 1.0f},
    }};

    brushes.set("white-charcoal", {0.96f, 0.97f, 0.94f, 1}, 1.0f);
    brushes.spline(pen, points, 1.0f);
    brushes.set("2H", {0.93f, 0.95f, 0.92f, 1}, 1.0f);
    for (int ribbon = 1; ribbon <= 4; ++ribbon) {
      const std::array<brush::Sample, 4> copy{{
          {{30.0f + 55.0f * ribbon, 30}, 1.0f},
          {{250.0f - 3.0f * ribbon, 100.0f + 5.0f * ribbon}, 1.0f},
          {{x, y}, pressure},
          {{570.0f - 100.0f * ribbon, 570}, 1.0f},
      }};
      pen.randomSeed(0x62A11u);
      brushes.spline(pen, copy, 1.0f);
    }
    label(pen, "*spline()", {0.84f, 0.86f, 0.85f, 1}, x + 65, y);
  }

  void draw(Pen& pen) override {
    const float seconds = (float)pen.millis() / 1000.0f;
    const int scene = (int)std::floor(seconds / kSceneSeconds) % 6;
    const float localSeconds = std::fmod(seconds, kSceneSeconds);
    const bool entered = scene != lastScene;
    if (entered) resetBrushes();

    pen.push();
    pen.scale(kScale);
    switch (scene) {
      case 0:
        rain(pen, entered);
        break;
      case 1:
        fields(pen, localSeconds);
        break;
      case 2:
        wheel(pen, localSeconds);
        break;
      case 3:
        hatches(pen, localSeconds);
        break;
      case 4:
        watercolor(pen, entered);
        break;
      case 5:
        splines(pen, localSeconds);
        break;
    }
    pen.pop();
    lastScene = scene;
  }

  void mousePressed(Pen& pen) override { pen.noLoop(); }
};

}  // namespace

SIGIL_SKETCH(BrushLiveTutorial, "Draw · Procedural",
             "Six timed scenes for fields, tools, hatches, watercolor and "
             "pressure splines.")
