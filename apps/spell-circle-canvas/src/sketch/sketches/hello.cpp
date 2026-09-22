// hello.cpp — a starter sketch. Run it:
//
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/hello.cpp
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
#include <sigilmotion/bind/Bound.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <string>
#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace draw = sigil::draw;
namespace motion = sigil::motion;

using namespace sigil::compose;

namespace {

sketch::kit::Theme sheetTheme() {
  auto look = sketch::kit::houseTheme();
  look.palette.ground = hexColor(0xf6f2e9);
  look.palette.ink = hexColor(0x253b40);
  look.palette.ash = hexColor(0x63777a);
  look.palette.rule = hexColor(0xd5dcd5);
  look.type.title = {.size = 42};
  look.type.subtitle = {.size = 16};
  look.type.footer = {.size = 12};
  look.type.captionNote = {.size = 14};
  look.spacing.marginX = 48;
  look.spacing.marginTop = 42;
  look.spacing.marginBottom = 32;
  look.spacing.contentGap = 32;
  return look;
}

Element card(Utf8 step, Utf8 title, Utf8 note, material::Color color) {
  return box()
      .column()
      .gap(10)
      .flexGrow()
      .height(150)
      .padding(24)
      .borderRadius({18})
      .fill(Fill::color(color))
      .children({document::eyebrow(std::move(step)),
                 text(std::move(title)).font({.size = 28}),
                 text(std::move(note))});
}

}  // namespace

struct HelloSketch {
  choreograph::Output<float> wave{0.0f};
  int score = 0;
  double nextScoreAt = 1.0;

  Element describe() {
    const sketch::kit::Provide look(sheetTheme());
    return sketch::kit::page(
        {.title = "Hello, Sketchbook.",
         .subtitle = "Start with a shape. Give it a rhythm. Make it your own.",
         .footer =
             "Open hello.cpp  ·  change a colour or a word  ·  save to reload"},
        box().column().gap(28).children(
            {box().row().gap(18).children(
                 {card("01 / MAKE", "Edit", "A colour, a curve, a word.",
                       hexColor(0xf4baa5)),
                  card("02 / TRY", "Save", "Your canvas follows along.",
                       hexColor(0xbfdadf))
                      .translateY(motion::bind(&wave).scale(-3)),
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
                           .ink(hexColor(0x63777a))}),
                  box().column().width(240).gap(12).children(
                      {text("A value that changes.").font({.size = 20}),
                       kit::centred()
                           .column()
                           .gap(8)
                           .height(174)
                           .borderRadius({18})
                           .fill(Fill::color(hexColor(0xe5e9df)))
                           .children({text(std::to_string(score))
                                          .font({.size = 64})
                                          .key("score"),
                                      document::eyebrow("and counting")}),
                       text("Update only when data changes.")
                           .ink(hexColor(0x63777a))})})}));
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {1000, 700}, .captureAt = 1.0});
    ctx.composer.render(describe());

    // One output animates a retained card and feeds the immediate wave.
    ctx.ticker.add([this, &ticker = ctx.ticker] {
      wave = static_cast<float>(std::sin(ticker.elapsed() * 1.6));
    });
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    if (elapsed < nextScoreAt) return;
    nextScoreAt = elapsed + 1.0;
    score += 25;
    // The keyed counter changes; the other retained elements keep their state.
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(HelloSketch, "Start & fixtures", "The starter sketch. Copy it.")
