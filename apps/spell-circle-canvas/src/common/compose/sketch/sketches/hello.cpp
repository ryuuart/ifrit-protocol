// hello.cpp — a starter sketch. Run it:
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/hello.cpp
//
// Then EDIT THIS FILE AND SAVE — the canvas reloads in a couple of
// seconds. Drop images into sketches/assets/ and load them with
// ctx.assets.image("name.png") (a magenta checker shows until the file
// exists; editing the file on disk hot-swaps it too).

#include <ifritsketch/Sketch.h>

#include <include/core/SkPathBuilder.h>

#include <cmath>

using namespace ifrit::compose;
using namespace ifrit::compose::util;
using namespace std::chrono_literals;

namespace {

textflow::TextStyle type(float size, SkColor color) {
  textflow::TextStyle style;
  style.shaping.fontSize = size;
  style.paint.foreground.setColor(color);
  return style;
}

} // namespace

struct HelloSketch : ifrit::compose::sketch::Sketch {
  choreograph::Output<float> wave{0.0f};

  void setup(sketch::SketchContext &ctx) override {
    // A steppable drives the bound Output every frame.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      wave = (float)std::sin(t * 1.6);
      return true;
    });

    auto card = [&](std::u8string label, SkColor4f color) {
      return box().width(150).height(90).corners({16})
          .fill(Fill::color(color))
          .background(shadow({0, 0, 0, 0.4f}, {3, 4}, 10))
          .alignItems(Align::Center).justify(Justify::Center)
          .child(text(std::move(label), type(20, SK_ColorWHITE)));
    };

    ctx.composer.render(
        stack()
            .fill(linearGradient({0, 0}, {0, ctx.size.height()},
                                 {{0.08f, 0.06f, 0.18f, 1},
                                  {0.03f, 0.10f, 0.16f, 1}}))
            // A row of cards — try changing colors, sizes, corners…
            .child(box().row().gap(24).absolute()
                       .inset(90, 120, 90, 330)
                       .child(card(u8"edit", {0.86f, 0.30f, 0.40f, 1}))
                       .child(card(u8"save", {0.30f, 0.56f, 0.95f, 1}))
                       .child(card(u8"reloads", {0.35f, 0.72f, 0.45f, 1})))
            // An image from the assets directory (magenta checker
            // until you drop a real file in).
            .child(image(ctx.assets.image("logo.png"))
                       .width(120).height(120).corners({20}).clip()
                       .absolute().inset(90, 280, 690, 240))
            // A custom leaf riding the bound Output.
            .child(custom([this](SkCanvas &canvas,
                                 const PaintContext &paint) {
                     SkPaint brush;
                     brush.setAntiAlias(true);
                     const float w = paint.size.width();
                     const float h = paint.size.height();
                     SkPathBuilder path;
                     path.moveTo(0, h / 2);
                     for (float x = 0; x <= w; x += 6)
                       path.lineTo(
                           x, h / 2 +
                                  std::sin(x * 0.03f +
                                           (float)paint.elapsedSeconds *
                                               2.0f) *
                                      h * 0.32f * wave.value());
                     brush.setStyle(SkPaint::kStroke_Style);
                     brush.setStrokeWidth(3);
                     brush.setColor(SK_ColorCYAN);
                     canvas.drawPath(path.detach(), brush);
                   })
                       .absolute().inset(240, 300, 90, 180)
                       .cache(Cache::None))
            .child(text(u8"ComposeSketch — edit hello.cpp and save",
                        type(17, 0xff9aa4bb))
                       .absolute().inset(90, 560, 90, 40)));
  }
};

IFRIT_SKETCH(HelloSketch)
