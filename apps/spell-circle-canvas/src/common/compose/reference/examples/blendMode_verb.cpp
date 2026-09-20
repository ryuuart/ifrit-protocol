/** @file
 * blendMode — how a node's paint meets what is already on the canvas.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {640, 250};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kBed = hexColor(0xc4763c);
constexpr SkColor4f kDisc = hexColor(0x4f8fd8);
constexpr SkColor4f kAsh = hexColor(0x8ea0ad);

/** One bed with one disc over it, the disc blended as named. */
Element cell(const char* caption, SkBlendMode mode) {
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
               .clip()
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
        cell("kSrcOver", SkBlendMode::kSrcOver),
        cell("kMultiply", SkBlendMode::kMultiply),
        cell("kScreen", SkBlendMode::kScreen),
        cell("kDifference", SkBlendMode::kDifference),
    });
  }
};

SIGIL_SKETCH(BlendModeVerb, "Reference · Compose",
             "one disc over one bed in four blend modes")
