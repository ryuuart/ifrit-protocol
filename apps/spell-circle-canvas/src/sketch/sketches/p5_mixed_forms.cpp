// p5_mixed_forms.cpp — a p5 loop reaching for the other forms, standing
// in a compose tree that tells it what colour and what type it is in.
//
// The door goes both ways, and the cascade crosses it in both directions.
// The box around the node sets the font and the ink; the pen inside it
// finds them already in force, so `pen.text` is shaped in the inherited
// face at the inherited size with NO textFont call, and the guest the pen
// paints through `pen.element(card, box)` begins in the same pair — its
// lines name no style at all and are set in what the tree says.
//
// What the pen still decides for itself is what it sets: `pen.fill(Paint)`
// puts a material under a p5 verb, inside a push/pop so the type after it
// is back in the ink. The node is `compose::graphics`, so the canvas is
// KEPT and the translucent ground each frame is the trail.
//
// EDIT THESE FIRST
//   kCount   how many balls drift on the noise field
//   kDrift   how fast the field moves them
//   kDisplay the size the tree sets, which the pen's line is shaped at

#include <include/core/SkRect.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>

#include <string>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;

using sigil::material::skia::Paint;
using sigil::draw::Pen;

namespace {

constexpr int kCount = 24;
constexpr float kDrift = 0.35f;
constexpr float kDisplay = 26;  // the size the tree sets

}  // namespace

struct P5MixedForms final : sketch::Sketch {
  // One shaded ball: a radial ramp in the pen's space, so each ball is
  // drawn at the origin after a translate and the highlight sits where
  // the ramp says.
  const Paint shade = Paint::radial(
      {-6, -6}, 26,
      {{0.0f, {1.0f, 0.85f, 0.6f, 1}}, {1.0f, {0.75f, 0.25f, 0.35f, 1}}});

  /** THE GUEST, and every line in it style-less: the face and the ink
   *  arrive from the tree the pen stands in, and the card names only the
   *  size it wants them at — a one-field override across the door. */
  static compose::Element card(int frame) {
    return compose::box()
        .column()
        .gap(4)
        .padding(14)
        .corners({12})
        .fill(compose::Fill::color({0.16f, 0.20f, 0.34f, 0.92f}))
        .font({.size = 15})
        .child(compose::text(u8"a compose card, retained"))
        .child(compose::text(compose::toUtf8("frame " + std::to_string(frame)))
                   .font({.size = 12})
                   .key("frame"));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(720, 480);
    ctx.background({0.047f, 0.055f, 0.094f, 1});
    ctx.captureAt(2.5);
    ctx.composer.render(
        compose::box()
            .absolute()
            .inset(0)
            // What the pen and its guest are set in. Nothing below names a
            // face, a size or a colour except where it means to change one.
            .font({.size = kDisplay, .track = 1.5f})
            .ink({1, 1, 1, 1})
            .child(compose::graphics("p5_mixed_forms.loop",
                                     [this](Pen& pen) { loop(pen); })
                       .absolute()
                       .inset(0)));
  }

  void loop(Pen& pen) {
    pen.noStroke();
    pen.textAlign(sigil::draw::CENTER, sigil::draw::CENTER);
    pen.background(12, 14, 24, 40);
    const float t = (float)pen.millis() / 1000.0f * kDrift;
    // The material is the pen's own decision, scoped: after the pop the
    // fill is the ink again, so the line below is set in it.
    pen.push();
    pen.fill(shade);
    for (int i = 0; i < kCount; ++i) {
      const float x = pen.noise((float)i * 0.37f, t) * pen.width;
      const float y = pen.noise(t, (float)i * 0.53f + 9.0f) * pen.height;
      pen.push();
      pen.translate(x, y);
      pen.circle(0, 0, 44);
      pen.pop();
    }
    pen.pop();
    pen.text("shaped by weave, drawn by a pen", pen.width / 2, 60);
    pen.element(
        card(pen.frameCount),
        SkRect::MakeXYWH(pen.width / 2 - 150, pen.height - 120, 300, 80));
  }
};

SIGIL_SKETCH(P5MixedForms, "Draw",
             "a p5 loop with a material fill, type shaped in the font the "
             "tree set, and a compose card retained inside it \xe2\x80\x94 "
             "the cascade crossing the door both ways.")
