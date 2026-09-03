// p5_mixed_forms.cpp — a p5 loop that reaches for the other forms: balls
// shaded by a material, a line of type shaped by weave, and a compose
// card retained among them.
//
// The door goes both ways. `pen.fill(Paint)` puts a material under a p5
// verb, `pen.textFont(Type)` puts weave's shaping under `text`, and
// `pen.element(card, box)` keeps a compose tree alive inside the loop —
// reconciled every frame, so its text shapes once, its layout holds and
// the keyed line updates in place.
//
// EDIT THESE FIRST
//   kCount   how many balls drift on the noise field
//   kDrift   how fast the field moves them

#include <sigilcompose/Compose.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/draw/Draw.h>
#include <sigilweave/style/Type.h>

#include <string>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace weave = sigil::weave;
using sigil::material::skia::Paint;
using namespace sigil::draw;

namespace {

constexpr int kCount = 24;
constexpr float kDrift = 0.35f;

struct P5MixedForms final : sketch::DrawSketch {
  // One shaded ball: a radial ramp in the pen's space, so each ball is
  // drawn at the origin after a translate and the highlight sits where
  // the ramp says.
  const Paint shade = Paint::radial(
      {-6, -6}, 26,
      {{0.0f, {1.0f, 0.85f, 0.6f, 1}}, {1.0f, {0.75f, 0.25f, 0.35f, 1}}});

  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(720, 480);
    ctx.background(12, 14, 24);
    ctx.captureAt(2.5);
    ctx.pen.noStroke();
    ctx.pen.textFont(weave::Type{.size = 30, .track = 1.5f});
    ctx.pen.textAlign(CENTER, CENTER);
  }

  compose::Element card(int frame) {
    return compose::box()
        .padding(14)
        .corners({12})
        .fill(compose::Fill::color({0.16f, 0.20f, 0.34f, 0.92f}))
        .child(compose::text(
            u8"a compose card, retained",
            weave::textStyle({.size = 16, .color = compose::hex(0xffffff)})))
        .child(compose::text(compose::toU8("frame " + std::to_string(frame)),
                             weave::textStyle(
                                 {.size = 13, .color = compose::hex(0x9fb0d0)}))
                   .key("frame"));
  }

  void draw(Pen& pen) override {
    pen.background(12, 14, 24, 40);
    const float t = (float)pen.millis() / 1000.0f * kDrift;
    pen.fill(shade);
    for (int i = 0; i < kCount; ++i) {
      const float x = pen.noise((float)i * 0.37f, t) * pen.width;
      const float y = pen.noise(t, (float)i * 0.53f + 9.0f) * pen.height;
      pen.push();
      pen.translate(x, y);
      pen.circle(0, 0, 44);
      pen.pop();
    }
    pen.fill(255);
    pen.text("shaped by weave, drawn by a pen", pen.width / 2, 60);
    pen.element(
        card(pen.frameCount),
        SkRect::MakeXYWH(pen.width / 2 - 150, pen.height - 120, 300, 80));
  }
};

}  // namespace

SIGIL_SKETCH(P5MixedForms, "Draw",
             "A p5 loop with a material fill, shaped type and a compose card "
             "retained inside it.")
