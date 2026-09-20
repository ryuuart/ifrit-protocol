/** @file
 * ink — the colour text under a node is set in, and the colour every
 * mark that names none is painted in.
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

constexpr SkSize kCanvas = {600, 260};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kPlate = hexColor(0x1b2229);
constexpr SkColor4f kPale = hexColor(0xd9e3ea);
constexpr SkColor4f kAmber = hexColor(0xe0a03c);

/** One panel. Nothing inside it names a colour: the text takes the ink,
 *  the stroke takes the ink because it names none, and the square takes
 *  it by asking for the ink in force outright. */
Element panel(const char* caption) {
  return box()
      .column()
      .gap(12)
      .padding(18)
      .basis(0)
      .grow(1)
      .corners({10})
      .fill(kPlate)
      .stroke(stroke(1.5f))
      .children({text(caption).font({.size = 15}),
                 box().height(30).width(30).corners({4}).fill(
                     Fill::currentInk())});
}

}  // namespace

struct InkVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .row()
        .gap(18)
        .padding(24)
        .alignItems(Align::Stretch)
        // The ink for everything under this node…
        .ink(kPale)
        .children({panel("The ink inherited"),
                   // …and a subtree that sets its own.
                   panel("The ink set here").ink(kAmber)});
  }
};

SIGIL_SKETCH(InkVerb, "Reference · Compose",
             "the ink inherited down a subtree, and one subtree setting its "
             "own")
