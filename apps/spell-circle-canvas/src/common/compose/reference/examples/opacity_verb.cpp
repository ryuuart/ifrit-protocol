/** @file
 * opacity — the node and everything under it faded as ONE group, which
 * is why the overlapping children inside a faded card do not show
 * through each other.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {620, 240};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kPlate = hexColor(0x6fb3a6);
constexpr SkColor4f kDisc = hexColor(0x27343d);
constexpr SkColor4f kAsh = hexColor(0x8ea0ad);

/** A card with an overlapping child, at one opacity. */
Element card(float value, const char* caption) {
  return box()
      .column()
      .gap(10)
      .basis(0)
      .grow(1)
      .alignItems(Align::Center)
      .children({stack()
                     .width(pct(100))
                     .height(120)
                     .opacity(value)
                     .children({box().cover().corners({10}).fill(kPlate),
                                box()
                                    .width(64)
                                    .height(64)
                                    .corners({32})
                                    .fill(kDisc)
                                    .left(20)
                                    .top(28)}),
                 text(caption).font({.size = 12, .color = kAsh})});
}

}  // namespace

struct OpacityVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().row().gap(18).padding(24).children({
        card(1.0f, "opacity(1)"),
        card(0.55f, "opacity(0.55)"),
        card(0.2f, "opacity(0.2)"),
    });
  }
};

SIGIL_SKETCH(OpacityVerb, "Reference · Compose",
             "one card at three opacities, faded as a group")
