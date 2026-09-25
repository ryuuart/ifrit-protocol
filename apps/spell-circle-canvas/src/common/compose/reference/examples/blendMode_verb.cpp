/** @file
 * blendMode — how a node's paint meets what is already on the canvas.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {640, 250};
constexpr material::Color kGround = material::hexColor(0x14181d);
constexpr material::Color kBed = material::hexColor(0xc4763c);
constexpr material::Color kDisc = material::hexColor(0x4f8fd8);
constexpr material::Color kAsh = material::hexColor(0x8ea0ad);

/** One bed with one disc over it, the disc blended as named. */
Element cell(const char* caption, material::BlendMode mode) {
  return box()
      .column()
      .gap(10)
      .flexBasis(0)
      .flexGrow(1)
      .alignItems(Align::Center)
      .children(
          {stack()
               .width(pct(100))
               .height(120)
               .borderRadius({10})
               .overflow(Overflow::Clip)
               .children({box().cover().fill(kBed), box()
                                                        .width(84)
                                                        .height(84)
                                                        .borderRadius({42})
                                                        .fill(kDisc)
                                                        .blendMode(mode)
                                                        .left(pct(30))
                                                        .top(18)}),
           text(caption).font({.size = 12, .color = kAsh})});
}

}  // namespace

struct BlendModeVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().row().gap(16).padding(24).children({
        cell("Normal", material::BlendMode::Normal),
        cell("Multiply", material::BlendMode::Multiply),
        cell("Screen", material::BlendMode::Screen),
        cell("Difference", material::BlendMode::Difference),
    });
  }
};

SIGIL_SKETCH(BlendModeVerb, "Reference · Compose",
             "one disc over one bed in four blend modes")
