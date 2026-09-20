/** @file
 * stroke — the node's boundary dressed by a brush: the whole of it, two
 * rings stacked on it, and one run of it claimed by a span.
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

constexpr SkSize kCanvas = {620, 240};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kPlate = hexColor(0x1c232b);
constexpr SkColor4f kAsh = hexColor(0x8ea0ad);
constexpr SkColor4f kEdge = hexColor(0x6fb3a6);
constexpr SkColor4f kHalo = hexColor(0x2f5f6a);

Element cell(const char* caption, Element plate) {
  return box()
      .column()
      .gap(10)
      .basis(0)
      .grow(1)
      .alignItems(Align::Center)
      .children(
          {plate.width(pct(100)).height(120).borderRadius({10}).fill(kPlate),
           text(caption).font({.size = 12, .color = kAsh})});
}

}  // namespace

struct StrokeVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().row().gap(18).padding(24).children({
        cell("stroke(brush)", box().stroke(stroke(2, Fill::color(kEdge)))),
        // Repeated calls append: two calls are two rings.
        cell("two calls, two rings",
             box()
                 .stroke(stroke(6, Fill::color(kHalo)))
                 .stroke(stroke(2, Fill::color(kEdge)))),
        // A span-qualified pass claims the run it resolves to. The
        // plate's corners are rounded, and a fillet turns by less at
        // each step than the 30 degrees a break defaults to, so the
        // angle comes down to what this silhouette actually turns.
        cell(
            "stroke(where, what)",
            box().stroke(spans::corners(22, 7), stroke(2, Fill::color(kEdge)))),
    });
  }
};

SIGIL_SKETCH(StrokeVerb, "Reference · Compose",
             "the boundary dressed whole, twice over, and on one claimed "
             "run")
