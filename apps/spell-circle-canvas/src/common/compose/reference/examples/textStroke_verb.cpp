/** @file
 * textStroke — the letterforms thickened by a pass beneath their fill,
 * which is what keeps a caption legible over a busy ground.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {600, 250};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kBusy = hexColor(0x6a8f7f);
constexpr SkColor4f kInk = hexColor(0xf4f7f9);
constexpr SkColor4f kOutline = hexColor(0x121a1e);
constexpr SkColor4f kAsh = hexColor(0x8ea0ad);

/** The same word over the same ground, once plain and once engraved. */
Element cell(const char* caption, Element label) {
  return box()
      .column()
      .gap(10)
      .basis(0)
      .grow(1)
      .alignItems(Align::Center)
      .children({box()
                     .width(pct(100))
                     .height(120)
                     .borderRadius({10})
                     .fill(kBusy)
                     .justify(Justify::Center)
                     .alignItems(Align::Center)
                     .children({std::move(label)}),
                 text(caption).font({.size = 12, .color = kAsh})});
}

}  // namespace

struct TextStrokeVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().row().gap(18).padding(24).children({
        cell("the letterforms alone",
             text("LEGIBLE").font({.size = 34, .color = kInk, .track = 1})),
        cell("textStroke(4, outline)",
             text("LEGIBLE")
                 .font({.size = 34, .color = kInk, .track = 1})
                 .textStroke(4, Fill::color(kOutline))),
    });
  }
};

SIGIL_SKETCH(TextStrokeVerb, "Reference · Compose",
             "a caption over a busy ground, with and without the stroke "
             "under its fill")
