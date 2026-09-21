/** @file
 * overlay — the middle slot: over the fill, under the content and the
 * children. The same mark in the foreground slot is drawn beside it, so
 * the difference is the picture rather than a sentence.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {600, 250};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kPlate = hexColor(0xd9a441);
constexpr SkColor4f kInk = hexColor(0x1a1206);
constexpr SkColor4f kVignette = hexColor(0x1a1206, 0.45f);
constexpr SkColor4f kAsh = hexColor(0x8ea0ad);

/** A band 56 px wide inside the node's own boundary — wide enough to
 *  cross the digit standing in the middle of it, so which slot it is in
 *  is visible rather than described. */
PathFormat band() {
  return stroke(56, Fill::color(kVignette), PathFormat::Align::Inner);
}

Element cell(const char* caption, Element plate) {
  return box()
      .column()
      .gap(10)
      .flexBasis(0)
      .flexGrow(1)
      .alignItems(Align::Center)
      .children({plate.width(pct(100))
                     .height(120)
                     .borderRadius({10})
                     .fill(kPlate)
                     .overflow(Overflow::Clip)
                     .justifyContent(Justify::Center)
                     .alignItems(Align::Center)
                     .children({text("47").font({.size = 52, .color = kInk})}),
                 text(caption).font({.size = 12, .color = kAsh})});
}

}  // namespace

struct OverlayVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().row().gap(18).padding(24).children({
        cell("overlay — over the fill, under the digit", box().overlay(band())),
        cell("foreground — over the digit too", box().foreground(band())),
    });
  }
};

SIGIL_SKETCH(OverlayVerb, "Reference · Compose",
             "one mark in the middle slot and in the foreground slot, over "
             "the same content")
