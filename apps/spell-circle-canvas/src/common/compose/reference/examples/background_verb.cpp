/** @file
 * background — a decoration painted beneath the fill, which is where a
 * shadow and anything else the surface sits on top of belongs.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {620, 250};
constexpr material::Color kGround = hexColor(0x1a2027);
constexpr material::Color kPlate = hexColor(0xe9eef1);
constexpr material::Color kShade = hexColor(0x05080b, 0.55f);
constexpr material::Color kAsh = hexColor(0x8ea0ad);

Element cell(const char* caption, Element plate) {
  return box()
      .column()
      .gap(12)
      .flexBasis(0)
      .flexGrow(1)
      .alignItems(Align::Center)
      .justifyContent(Justify::Center)
      .children({plate.width(160).height(100).borderRadius({12}),
                 text(caption).font(
                     {.size = 12, .color = material::skia::toSkColor(kAsh)})});
}

}  // namespace

struct BackgroundVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().row().gap(18).padding(28).children({
        cell("the fill alone", box().fill(kPlate)),
        // Beneath the fill: the plate sits on the shadow.
        cell("background(shadow)",
             box().fill(kPlate).background(shadow(kShade, {0, 10}, 18))),
        // The same mark with nothing over it — a background under no fill
        // is the whole of what the node paints.
        cell("background with no fill",
             box().background(shadow(kShade, {0, 10}, 18))),
    });
  }
};

SIGIL_SKETCH(BackgroundVerb, "Reference · Compose",
             "a shadow beneath the fill, and the same mark with no fill "
             "over it")
