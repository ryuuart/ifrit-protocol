// hello.cpp — a starter sketch. Run it:
//
//   ./build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
//       ../grimoire/sketches/hello.cpp
//
// Edit a card's words or colour and save to reload. The cards are retained
// elements, the wave is an immediate pen program, and the counter is data
// re-described only when it changes. All three share one composition.
// TAGS: Runtime/Starter

#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Oscillator.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace draw = sigil::draw;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;

Element card(Utf8 step, Utf8 title, Utf8 note, material::Color color) {
  return box()
      .column()
      .gap(10)
      .flexGrow()
      .height(150)
      .padding(24)
      .borderRadius({18})
      .fill(Fill::color(color))
      .ink(hexColor(0x253b40))
      .children({document::eyebrow(std::move(step)),
                 text(std::move(title)).font({.size = 28}),
                 text(std::move(note))});
}

}  // namespace

struct HelloSketch {
  motion::Animatable<float> wave;
  int score = 0;
  double nextScoreAt = 1.0;

  Element describe() {
    return sketch::kit::page(
        {.title = "Hello, Grimoire.",
         .subtitle = "Start with a shape. Give it a rhythm. Make it your own.",
         .footer =
             "Open hello.cpp  ·  change a colour or a word  ·  save to reload"},
        box().column().gap(28).children(
            {box().row().gap(18).children(
                 {card("01 / MAKE", "Edit", "A colour, a curve, a word.",
                       hexColor(0xf4baa5)),
                  card("02 / TRY", "Save", "Your canvas follows along.",
                       hexColor(0xbfdadf))
                      .translateY(motion::bind(wave, {.to = {0.0f, -3.0f}})),
                  card("03 / PLAY", "Repeat", "Keep the part you love.",
                       hexColor(0xdbe6b4))}),
             box().row().gap(24).children(
                 {box().column().flexGrow().gap(12).children(
                      {text("A line with a little life.").font({.size = 20}),
                       // This keyless program reads the clock and runs each
                       // frame.
                       pen([this](draw::Pen& pen) {
                         pen.noFill();
                         pen.stroke(88, 112, 117);
                         pen.strokeWeight(1);
                         pen.line(24, pen.height / 2, pen.width - 24,
                                  pen.height / 2);
                         pen.stroke(190, 225, 213);
                         pen.strokeWeight(3);
                         pen.beginShape();
                         for (int x = 24; x <= static_cast<int>(pen.width) - 24;
                              x += 4)
                           pen.vertex(
                               static_cast<float>(x),
                               pen.height / 2 +
                                   std::sin(x * 0.03f + pen.millis() * 0.002f) *
                                       pen.height * 0.28f * wave.value());
                         pen.endShape();
                       })
                           .height(174)
                           .borderRadius({18})
                           .fill(Fill::color(hexColor(0x253b40)))
                           .overflow(Overflow::Clip),
                       text("Draw every frame with the pen.")
                           .ink(var("palette-ash"))}),
                  box().column().width(240).gap(12).children(
                      {text("A value that changes.").font({.size = 20}),
                       kit::centred()
                           .column()
                           .gap(8)
                           .height(174)
                           .borderRadius({18})
                           .fill(var("palette-cell-ground"))
                           .children({text(std::to_string(score))
                                          .font({.size = 64})
                                          .key("score"),
                                      document::eyebrow("and counting")}),
                       text("Update only when data changes.")
                           .ink(var("palette-ash"))})})}));
  }

  void setup(sketch::Scene2DContext& ctx) {
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .sheet = rule(":root")
                      .var("spacing-margin-horizontal", 48)
                      .var("spacing-margin-top", 42)
                      .var("spacing-margin-bottom", 32)
                      .var("spacing-content-gap", 32)
                      .var("spacing-rule-gap", 16),
         .size = {1000, 700},
         .captureAt = 1.0});
    // The clock, followed: a sine of 1.6 radians a second. One value
    // animates a retained card and feeds the immediate wave.
    wave = motion::bind(ctx.engine.time(),
                        {.ease = motion::Oscillator{.hertz = 1.6f / kTau}});
    ctx.composer.render(describe());
  }

  void update(double elapsed, sketch::Scene2DContext& ctx) {
    if (elapsed < nextScoreAt) return;
    nextScoreAt = elapsed + 1.0;
    score += 25;
    // The keyed counter changes; the other retained elements keep their state.
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(HelloSketch, "Start & fixtures", "The starter sketch. Copy it.")
