/** @file
 * custom — a box whose content is one paint program, in the keyed
 * spelling that prunes.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {520, 280};
constexpr material::Color kGround = hexColor(0x14181d);
constexpr material::Color kCell = hexColor(0x1b2229);
constexpr material::Color kAsh = hexColor(0x8ea0ad);

/** The program: rings measured off the box the node was laid out at,
 *  which is what `PaintContext::size` carries. */
void rings(SkCanvas& canvas, const PaintContext& context) {
  const SkPoint centre{context.size.width() * 0.5f,
                       context.size.height() * 0.5f};
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  for (int ring = 1; ring <= 7; ++ring) {
    const float t = (float)ring / 7;
    paint.setStrokeWidth(1.0f + 2.0f * t);
    paint.setColor4f(hexColor(0x6fb3a6, 1.0f - 0.9f * t), nullptr);
    canvas.drawCircle(centre, 14.0f * (float)ring, paint);
  }
}

}  // namespace

struct CustomElement {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .column()
        .gap(10)
        .padding(24)
        .alignItems(Align::Center)
        .children({
            // The key is the program's identity: one key, one drawing.
            // A custom leaf sizes like an empty box, so this one states
            // its own dimensions.
            custom("reference/rings", rings)
                .width(pct(100))
                .height(190)
                .borderRadius({10})
                .fill(kCell)
                .overflow(Overflow::Clip),
            text("custom(key, program)")
                .font({.size = 12, .color = material::skia::toSkColor(kAsh)}),
        });
  }
};

SIGIL_SKETCH(CustomElement, "Reference · Compose",
             "custom: a box whose content is one keyed paint program")
