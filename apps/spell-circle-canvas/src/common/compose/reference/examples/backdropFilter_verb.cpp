/** @file
 * backdropFilter — what is already painted beneath a node, filtered before the
 * node paints: the frosted panel over a lattice.
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

constexpr SkSize kCanvas = {620, 280};
constexpr material::Color kGround = hexColor(0x10161b);
constexpr material::Color kInk = hexColor(0xf0f5f8);
constexpr material::Color kVeil = hexColor(0xdcecf4, 0.18f);
constexpr material::Color kLight = hexColor(0x26414d);
constexpr material::Color kDark = hexColor(0x14242c);
constexpr float kTile = 28;

/** The ground the panels stand on: a lattice of real elements, so a
 *  filtered backdrop is visibly a filter rather than a wash. */
Element lattice() {
  return box().column().children({each(10, [](size_t row) {
    return box().row().height(kTile).children({each(23, [row](size_t column) {
      return box().width(kTile).height(kTile).fill(
          (row + column) % 2 == 0 ? kLight : kDark);
    })});
  })});
}

Element panel(const char* caption, Element plate) {
  return plate.width(210)
      .height(120)
      .borderRadius({14})
      .fill(kVeil)
      .justifyContent(Justify::Center)
      .alignItems(Align::Center)
      .children({text(caption).font({.size = 14, .color = kInk})});
}

}  // namespace

struct BackdropFilterVerb {
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
            .justifyContent(Justify::Center)
            .alignItems(Align::Center)
            .children({panel("the veil alone", box()),
                       // The lattice under this panel is blurred before
                       // the panel's own translucent fill goes down.
                       panel("backdropFilter(blur)",
                             box().backdropFilter(skia::Effect::blur(7)))}),
    });
  }
};

SIGIL_SKETCH(BackdropFilterVerb, "Reference · Compose",
             "a translucent panel over a lattice, with and without the "
             "backdrop filter")
