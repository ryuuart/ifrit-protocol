// hello.cpp — a starter sketch. Run it:
//
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/hello.cpp
//
// Then EDIT THIS FILE AND SAVE — the canvas reloads in a couple of
// seconds. Drop an image beside this file and load it with
// ctx.assets.image(ctx.local("name.png")) (a magenta checker shows until
// the file exists; editing the file on disk hot-swaps it too).

// TAGS: Runtime/Starter

#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <cmath>

namespace sketch = sigil::sketch;
namespace draw = sigil::draw;

using namespace sigil::compose;
using namespace std::chrono_literals;

// The three ways things move here (retained-mode, not p5's redraw
// loop):
//  1. setup() DECLARES the scene once — including its motion: bound
//     Outputs, transitions, ticker steppables. The runtime animates
//     them every frame without re-describing anything.
//  2. a pen() leaf is the immediate-mode floor — its program runs every
//     frame, in p5's own verbs (see the wave below).
//  3. update(elapsed, ctx) is for DATA changes: mutate state, call
//     composer.render(describe()) again, and the reconciler diffs it
//     (see the score counter below).
struct HelloSketch {
  choreograph::Output<float> wave{0.0f};
  int score = 0;
  double nextScoreAt = 0.0;

  Element describe(sketch::SketchContext& ctx) {
    auto card = [](Utf8 label, SkColor4f color) {
      return kit::centred()
          .width(150)
          .height(90)
          .corners({16})
          .fill(Fill::color(color))
          .background(shadow({0, 0, 0, 0.4f}, {3, 4}, 10))

          .children({text(std::move(label)).font({.size = 20})});
    };

    // The type is white unless a line says otherwise: the ink and the
    // font flow down the tree, and a leaf names only what differs.
    return stack()
        .ink(hexColor(0xffffff))
        .fill(linearGradient(
            {0, 0}, {0, ctx.size.height()},
            {{0.08f, 0.06f, 0.18f, 1}, {0.03f, 0.10f, 0.16f, 1}}))
        // A row of cards — try changing colors, sizes, corners…
        .children(
            {box()
                 .row()
                 .gap(24)
                 .inset(90, 120, 90, 330)
                 .children({card("edit", {0.86f, 0.30f, 0.40f, 1}),
                            card("save", {0.30f, 0.56f, 0.95f, 1}),
                            card("reloads", {0.35f, 0.72f, 0.45f, 1})}),
             // An image from the assets directory (magenta checker
             // until you drop a real file in).
             image(ctx.assets.image("logo.png"))
                 .width(120)
                 .height(120)
                 .corners({20})
                 .clip()
                 .inset(90, 280, 690, 240),
             // A PEN LEAF riding the bound Output: p5's verbs inside a
             // node of the tree, run every frame.
             // KEYLESS: the wave reads the pen's clock and a bound Output.
             pen([this](draw::Pen& pen) {
               pen.noFill();
               pen.stroke(0, 255, 255);
               pen.strokeWeight(3);
               pen.beginShape();
               // the loop walks a distance; the accumulated float is the
               // position
               // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
               for (float x = 0; x <= pen.width; x += 6)
                 pen.vertex(
                     x, pen.height / 2 +
                            std::sin(x * 0.03f + (float)pen.millis() * 0.002f) *
                                pen.height * 0.32f * wave.value());
               pen.endShape();
             }).inset(240, 300, 90, 180),
             // Re-rendered by update() whenever the score changes —
             // the keyed text keeps its identity across renders.
             text("score " + std::to_string(score))
                 .font({.size = 24})
                 .ink(hexColor(0xffd9a0))
                 .key("score")
                 .inset(650, 120, 90, 480),
             text(u8"Sketchbook — edit hello.cpp and save")
                 .font({.size = 17})
                 .ink(hexColor(0x9aa4bb))
                 .inset(90, 560, 90, 40)});
  }

  void setup(sketch::SketchContext& ctx) {
    // p5's createCanvas/background: declare the canvas you want —
    // the window letterboxes to it, headless captures honor it.
    sketch::kit::stage(ctx, {.size = {1000, 700},
                             .background = SkColor4f{0.05f, 0.04f, 0.10f, 1}});

    // Declared motion: a steppable drives the bound Output every
    // frame from here on — no per-frame describes needed.
    ctx.ticker.add([this, &ticker = ctx.ticker] {
      const double t = ticker.elapsed();
      wave = (float)std::sin(t * 1.6);
    });
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    // Data path: when state changes, describe again and let the
    // reconciler diff. Everything unchanged stays cached.
    if (elapsed < nextScoreAt) return;
    nextScoreAt = elapsed + 1.0;
    score += 25;
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(HelloSketch, "Start & fixtures", "The starter sketch. Copy it.")
