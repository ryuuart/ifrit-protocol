/** @file
 * filter — the node's own rendered layer post-processed: the whole
 * subtree, its children included, goes through the filter.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilsketch/canvas/Sketch.h>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace skia = sigil::material::skia;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {640, 250};
constexpr material::Color kGround = hexColor(0x0f1318);
constexpr material::Color kPlate = hexColor(0x1d2730);
constexpr material::Color kInk = hexColor(0x9fe3d4);
constexpr material::Color kAsh = hexColor(0x8ea0ad);

/** The same subtree — a plate, a word and a disc — under each filter. */
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
                     .column()
                     .gap(8)
                     .justifyContent(Justify::Center)
                     .alignItems(Align::Center)
                     .children({text("SIGNAL").font(
                                    {.size = 26,
                                     .color = material::skia::toSkColor(kInk),
                                     .track = 3}),
                                box().width(58).height(4).fill(kInk)}),
                 text(caption).font(
                     {.size = 12, .color = material::skia::toSkColor(kAsh)})});
}

}  // namespace

struct FilterVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().row().gap(16).padding(24).children({
        cell("no filter", box()),
        cell("filter(blur)", box().filter(skia::Effect::blur(3))),
        cell("filter(glow)",
             box().filter(skia::Effect::glow(hexColor(0x3fd6b0), 9))),
    });
  }
};

SIGIL_SKETCH(FilterVerb, "Reference · Compose",
             "one subtree drawn plain, blurred, and glowing")
