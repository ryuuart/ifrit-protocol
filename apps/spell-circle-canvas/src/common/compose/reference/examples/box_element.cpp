/** @file
 * box — the flex container, and the same factory as a leaf.
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

constexpr SkSize kCanvas = {640, 260};
constexpr material::Color kGround = hexColor(0x14181d);
constexpr material::Color kCard = hexColor(0x1e252c);
constexpr material::Color kInk = hexColor(0xdde6ec);
constexpr material::Color kAsh = hexColor(0x8ea0ad);
constexpr material::Color kAccent = hexColor(0x5fb0a4);

/** A card: a column of boxes, one of which is a bare leaf standing in as
 *  a rule. Nothing here is a kit component — every line is a factory and
 *  a verb. */
Element card(const char* title, const char* body) {
  return box()
      .column()
      .gap(10)
      .padding(18)
      .flexBasis(0)
      .flexGrow(1)
      .borderRadius({10})
      .fill(kCard)
      .children({text(title).font(
                     {.size = 17, .color = material::skia::toSkColor(kInk)}),
                 box().height(2).width(36).fill(kAccent),
                 text(body).font(
                     {.size = 13, .color = material::skia::toSkColor(kAsh)})});
}

}  // namespace

struct BoxElement {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .row()
        .gap(18)
        .padding(22)
        .alignItems(Align::Stretch)
        .children({card("Describe", "A box with no children is a leaf."),
                   card("Contain", "A box with children is a flex line."),
                   card("Space", "Gap and padding are the air between.")});
  }
};

SIGIL_SKETCH(BoxElement, "Reference · Compose",
             "box as a flex container and as a bare leaf")
