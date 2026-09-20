/** @file
 * backdrop — what is already painted beneath a node, filtered before the
 * node paints: the frosted panel over a lattice.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;
namespace skia = sigil::material::skia;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {620, 280};
constexpr SkColor4f kGround = hexColor(0x10161b);
constexpr SkColor4f kInk = hexColor(0xf0f5f8);
constexpr SkColor4f kVeil = hexColor(0xdcecf4, 0.18f);
constexpr SkColor4f kLight = hexColor(0x26414d);
constexpr SkColor4f kDark = hexColor(0x14242c);
constexpr float kTile = 28;

/** The ground the panels stand on: a lattice of real elements, so a
 *  filtered backdrop is visibly a filter rather than a wash. */
Element lattice() {
  return box().column().children({each(10, [](size_t row) {
    return box().row().height(kTile).children({each(23, [row](size_t column) {
      return box().width(kTile).height(kTile).fill((row + column) % 2 == 0
                                                       ? kLight
                                                       : kDark);
    })});
  })});
}

Element panel(const char* caption, Element plate) {
  return plate.width(210)
      .height(120)
      .corners({14})
      .fill(kVeil)
      .justify(Justify::Center)
      .alignItems(Align::Center)
      .children({text(caption).font({.size = 14, .color = kInk})});
}

}  // namespace

struct BackdropVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return stack().children({
        lattice().cover(),
        box()
            .cover()
            .row()
            .gap(20)
            .padding(30)
            .justify(Justify::Center)
            .alignItems(Align::Center)
            .children({panel("the veil alone", box()),
                       // The lattice under this panel is blurred before
                       // the panel's own translucent fill goes down.
                       panel("backdrop(blur)",
                             box().backdrop(skia::Effect::blur(7)))}),
    });
  }
};

SIGIL_SKETCH(BackdropVerb, "Reference · Compose",
             "a translucent panel over a lattice, with and without the "
             "backdrop filter")
