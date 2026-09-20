/** @file
 * fill — the three forms of the verb on one sheet: a colour, a paint,
 * and the ink in force.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;
namespace skia = sigil::material::skia;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {600, 220};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kLabel = hexColor(0x8ea0ad);
constexpr SkColor4f kAccent = hexColor(0xe2714b);

/** One labelled swatch: the square, and the spelling under it. */
Element swatch(Element square, const char* spelling) {
  return box()
      .column()
      .gap(10)
      .alignItems(Align::Center)
      .children({square.width(120).height(120).borderRadius({12}),
                 text(spelling).font({.size = 13, .color = kLabel})});
}

}  // namespace

struct FillVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .row()
        .gap(28)
        .padding(24)
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .ink(kAccent)
        .children({
            swatch(box().fill(kAccent), "fill(colour)"),
            swatch(
                box().fill(skia::Paint::linearUnit(
                    {0, 0}, {1, 1},
                    {{0.0f, hexColor(0x2f6f8f)}, {1.0f, hexColor(0x8f2f4f)}})),
                "fill(paint)"),
            swatch(box().fill(Fill::currentInk()), "fill(the ink in force)"),
        });
  }
};

SIGIL_SKETCH(FillVerb, "Reference · Compose",
             "the fill verb in its three forms: a colour, a paint, and the "
             "ink in force")
