// brush_live_tutorial.cpp — six brush constructions in one timed sketch.
//
// One row per scene: the body, the ground it is laid on, whether the canvas
// keeps what the body drew, the seed its stream starts at, and the word the
// scene signs itself with. Rain and watercolor keep their canvas, so their
// marks accumulate; the other four are laid on fresh ground every frame as
// their field, geometry or pressure changes.

// EDIT THESE FIRST
//   kSceneSeconds  how long each construction remains on screen
//   kScenes        the six rows: the ground, the seed and the signature
//   kPalette       the pigments shared by all six scenes

// TAGS: Drawing/Brushes

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Brush.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilsketch/canvas/Sketch.h>

#include <array>
#include <cmath>
#include <span>
#include <string_view>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
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

constexpr std::array<std::string_view, 8> kWheelBrushes{{
    "marker",
    "marker",
    "tutorial-watercolor",
    "tutorial-watercolor",
    "charcoal",
    "HB",
    "2B",
    "rotring",
}};

/** One of a run, taken from the pen's own stream. */
template <class Run>
const auto& pick(Pen& pen, const Run& run) {
  return run[(size_t)pen.random((float)run.size())];
}

/** A polygon that breathes: every corner walks a small circle of its own
 *  phase, so the outline moves without any corner leading. */
std::vector<SkPoint> breathing(std::span<const SkPoint> base,
                               std::span<const float> phase, float local,
                               float rate, float reach) {
  std::vector<SkPoint> corners;
  corners.reserve(base.size());
  for (size_t index = 0; index < base.size(); ++index)
    corners.push_back(arrange::onEllipse(base[index], {reach, reach},
                                         phase[index] + local * rate));
  return corners;
}

struct BrushLiveTutorial {
  /** The scene table: the body, its ground, whether the canvas keeps what
   *  the body drew, the seed its stream starts at — 0 lets the stream run
   *  on from the frame before — and the signature over the drawing. */
  struct Scene {
    void (BrushLiveTutorial::*body)(Pen&, float local);
    SkColor4f ground;
    bool kept = false;
    uint64_t seed = 0;
    std::string_view word;
    SkColor4f ink;
  };

  static const std::array<Scene, 6> kScenes;

  brush::Engine brushes;
  int lastScene = -1;

  void setup(sketch::SketchContext& context) {
    context.canvas(840, 840);
    context.background({255 / 255.0f, 252 / 255.0f, 235 / 255.0f, 1});
    context.captureAt(12.5);

    brushes.scaleBrushes(3.5f);

    brush::Tool watercolor = brush::marker(SkColors::kBlack, 10.0f);
    watercolor.tip = brush::Tip::Custom;
    watercolor.scatter = 1.05f;
    watercolor.opacity = 0.085f;
    watercolor.spacing = 2.4f;
    watercolor.pressure = {0.8f, 1.3f, 0.8f};
    watercolor.rotation = brush::Rotation::Natural;
    watercolor.markerTip = false;
    watercolor.customTip = [](Pen& pen) {
      pen.noStroke();
      pen.rectMode(CENTER);
      pen.rect(-0.08f, -0.08f, 0.78f, 0.78f, 0.08f);
      pen.rect(0.34f, 0.34f, 0.31f, 0.31f, 0.05f);
    };
    brushes.add("tutorial-watercolor", watercolor);

    brush::Tool whiteCharcoal = brush::charcoal({0.95f, 0.97f, 0.94f, 1}, 1.5f);
    whiteCharcoal.scatter = 2.0f;
    whiteCharcoal.opacity = 0.52f;
    whiteCharcoal.blend = SCREEN;
    brushes.add("white-charcoal", whiteCharcoal);

    context.composer.render(compose::graphics("brush_live_tutorial.loop",
                                              [this](Pen& pen) { draw(pen); })
                                .inset(0));
  }

  void resetBrushes() {
    brushes.noField();
    brushes.noFill();
    brushes.noWash();
    brushes.noHatch();
    brushes.noMass();
    brushes.noStroke();
  }

  /** Rain down the seabed field: one brush and one colour per drop, dealt
   *  out of the stream, onto a canvas that keeps every drop. */
  void rain(Pen& pen, float) {
    brushes.field("seabed");
    const std::string_view name = pick(pen, kBrushes);
    const SkColor4f color = pick(pen, kRainColors);
    brushes.set(name, color, pen.random(0.7f, 1.6f));
    brushes.flowLine(pen, {pen.random(600), pen.random(600)},
                     pen.random(140, 240), pen.random(360));
  }

  /** Every field in the catalogue in turn, a charcoal circle and thirty
   *  pencil lines read through whichever one is standing. */
  void fields(Pen& pen, float local) {
    const std::vector<std::string> names = brushes.listFields();
    if (!names.empty())
      brushes.field(names[(size_t)std::floor(local / 0.72f) % names.size()]);

    brushes.set("white-charcoal", {0.94f, 0.96f, 0.92f, 1}, 1.0f);
    brushes.circle(pen, 300, 300, 180, 0.3f);

    brushes.pick("HB");
    for (int line = 0; line < 30; ++line)
      brushes.flowLine(pen, {pen.random(600), pen.random(600)}, 75, 0);
  }

  /** Twenty rays off one circle, each in its own brush and pigment, with
   *  a disc struck at the hub in a brush the second picks. */
  void wheel(Pen& pen, float local) {
    brushes.field("seabed");
    for (int ray = 0; ray < 20; ++ray) {
      const float angle = arrange::along(local * 30.0f, 360.0f, (size_t)ray, 20,
                                         arrange::Turn::Closed);
      pen.randomSeed(0x33213u * (uint64_t)(ray + 1));
      const std::string_view name = pick(pen, kWheelBrushes);
      brushes.set(name, pick(pen, kPalette), 1.0f);
      brushes.flowLine(pen,
                       arrange::onEllipse({300.0f, 300.0f}, {100.0f, 100.0f},
                                          radians(-angle)),
                       320, angle);
    }

    brushes.noField();
    pen.randomSeed(0x5EEDu + (uint64_t)std::floor(local * 2.0f));
    const std::string_view hub = pick(pen, kWheelBrushes);
    brushes.set(hub, pick(pen, kPalette), 1.0f);
    brushes.circle(pen, 300, 300, 100, 0.2f);
  }

  /** Two breathing polygons, each filled with its own hatch: a close rose
   *  hatch across the six-corner one, a wide gold hatch across the three. */
  void hatches(Pen& pen, float local) {
    static constexpr std::array<SkPoint, 6> roseBase{{{80, 150},
                                                      {180, 150},
                                                      {420, 150},
                                                      {480, 450},
                                                      {280, 450},
                                                      {130, 450}}};
    static constexpr std::array<float, 6> rosePhase{
        {0.4f, 1.7f, 3.2f, 4.8f, 2.4f, 5.6f}};
    brushes.hatchStyle("HB", {0.78f, 0.38f, 0.51f, 1}, 1.3f);
    brushes.hatch(pen, 15.0f, 45.0f);
    brushes.polygon(pen, breathing(roseBase, rosePhase, local, 3.0f, 20.0f));

    static constexpr std::array<SkPoint, 3> goldBase{
        {{250, 250}, {500, 300}, {300, 520}}};
    static constexpr std::array<float, 3> goldPhase{{2.1f, 4.3f, 0.8f}};
    brushes.hatchStyle("marker", {0.88f, 0.71f, 0.07f, 1}, 0.18f);
    brushes.hatch(pen, 10.0f, 130.0f, 0.10f);
    brushes.polygon(pen, breathing(goldBase, goldPhase, local, 2.3f, 30.0f));
    brushes.noHatch();
  }

  /** A pigment patch every fifth frame, bled and textured, onto a canvas
   *  that keeps them: the wash builds up where the patches overlap. */
  void watercolor(Pen& pen, float) {
    if (pen.frameCount % 5 != 0) return;
    brushes.set("marker", {0.88f, 0.71f, 0.07f, 1}, 0.08f);
    const SkColor4f pigment = pick(pen, kPalette);
    brushes.fill(pigment, pen.random(60, 110) / 255.0f);
    brushes.fillBleed(pen.random(0.10f, 0.55f));
    brushes.fillTexture(0.4f, 0.4f, true);
    brushes.rect(pen, pen.random(600), pen.random(600), pen.random(50, 140),
                 pen.random(50, 140), CENTER);
    brushes.noFill();
  }

  /** One charcoal spline through four samples whose third is driven by the
   *  clock, and four pencil ribbons offset off it. */
  void splines(Pen& pen, float local) {
    brushes.set("2B", {0.05f, 0.18f, 0.35f, 1}, 2.0f);
    brushes.circle(pen, 155, 140, 50);

    const float phase = local * 1.6f;
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
      pen.randomSeed(0x62A11u);
      brushes.spline(
          pen,
          std::array<brush::Sample, 4>{{
              {{30.0f + 55.0f * ribbon, 30}, 1.0f},
              {{250.0f - 3.0f * ribbon, 100.0f + 5.0f * ribbon}, 1.0f},
              {{x, y}, pressure},
              {{570.0f - 100.0f * ribbon, 570}, 1.0f},
          }},
          1.0f);
    }
  }

  void draw(Pen& pen) {
    // A press freezes the tutorial on the scene it is showing.
    if (pen.mouseIsPressed) pen.noLoop();
    if (pen.frameCount == 1) {
      pen.frameRate(30);
      pen.randomSeed(0x213123u);
      pen.noiseSeed(0x213123u);
      pen.angleMode(DEGREES);
    }
    const float seconds = (float)pen.millis() / 1000.0f;
    const int index = (int)std::floor(seconds / kSceneSeconds) % 6;
    const Scene& scene = kScenes[(size_t)index];
    const bool entered = index != lastScene;

    // A scene the canvas KEEPS is laid on its ground once, on the way in;
    // every other scene is laid on it again for each frame it draws.
    if (entered || !scene.kept) {
      resetBrushes();
      pen.background(scene.ground);
      if (scene.seed) pen.randomSeed(scene.seed);
    }

    pen.push();
    pen.scale(kScale);
    (this->*scene.body)(pen, std::fmod(seconds, kSceneSeconds));
    pen.noStroke();
    pen.fill(scene.ink);
    pen.textAlign(CENTER, CENTER);
    pen.textSize(40);
    pen.text(scene.word, 300, 300);
    pen.pop();
    lastScene = index;
  }
};

// The six scenes, in the order they come round.
const std::array<BrushLiveTutorial::Scene, 6> BrushLiveTutorial::kScenes{{
    {.body = &BrushLiveTutorial::rain,
     .ground = {1.0f, 252 / 255.0f, 235 / 255.0f, 1},
     .kept = true,
     .seed = 0xB125A1u,
     .word = "*SIGILDRAW",
     .ink = {0.04f, 0.05f, 0.05f, 1}},
    {.body = &BrushLiveTutorial::fields,
     .ground = {8 / 255.0f, 15 / 255.0f, 21 / 255.0f, 1},
     .seed = 0x33213u,
     .word = "*field()",
     .ink = {0.82f, 0.84f, 0.84f, 1}},
    {.body = &BrushLiveTutorial::wheel,
     .ground = {226 / 255.0f, 231 / 255.0f, 220 / 255.0f, 1},
     .word = "*stroke()",
     .ink = {0.16f, 0.16f, 0.15f, 1}},
    {.body = &BrushLiveTutorial::hatches,
     .ground = {1.0f, 230 / 255.0f, 212 / 255.0f, 1},
     .word = "*hatch()",
     .ink = {0.20f, 0.18f, 0.17f, 1}},
    {.body = &BrushLiveTutorial::watercolor,
     .ground = {1.0f, 252 / 255.0f, 235 / 255.0f, 1},
     .kept = true,
     .seed = 0xF111u,
     .word = "*fill()",
     .ink = {0.05f, 0.05f, 0.04f, 1}},
    {.body = &BrushLiveTutorial::splines,
     .ground = {68 / 255.0f, 94 / 255.0f, 135 / 255.0f, 1},
     .seed = 0x5A11CEu,
     .word = "*spline()",
     .ink = {0.84f, 0.86f, 0.85f, 1}},
}};

}  // namespace

SIGIL_SKETCH(BrushLiveTutorial, "Draw · Procedural",
             "Six timed scenes for fields, tools, hatches, watercolor and "
             "pressure splines.")
