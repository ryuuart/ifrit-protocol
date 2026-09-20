/** @file
 * stack — the overlap container: every child shares the box, painted in
 * zIndex then declaration order.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {520, 300};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kPlate = hexColor(0x223039);
constexpr SkColor4f kInk = hexColor(0xe8eef2);
constexpr SkColor4f kBadge = hexColor(0xd8603f);
constexpr SkColor4f kWash = hexColor(0x0d1116, 0.55f);

}  // namespace

struct StackElement {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().padding(26).children({
        stack().flexGrow(1).borderRadius({12}).fill(kPlate).clip().children({
            // The plate's own ground, filling the box it stands in.
            box().cover().fill(kPlate),
            // A scrim over it: a covering child painted after the
            // ground and before the corner pieces.
            box().cover().fill(kWash),
            // A pinned corner piece — an absolute child keeps its
            // insets, so two edges place it and its content sizes it.
            box()
                .top(14)
                .right(14)
                .padding(8, 5)
                .borderRadius({4})
                .fill(kBadge)
                .children({text("NEW").font({.size = 12, .color = kInk})}),
            // A caption pinned to the other three edges.
            text("Every child shares the box.")
                .font({.size = 15, .color = kInk})
                .left(18)
                .right(18)
                .bottom(16),
        }),
    });
  }
};

SIGIL_SKETCH(StackElement, "Reference · Compose",
             "stack: children sharing one box, pinned by their own insets")
