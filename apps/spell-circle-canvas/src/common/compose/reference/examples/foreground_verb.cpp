/** @file
 * foreground — a decoration painted over the children, which is what a
 * keyline that must survive the content it sits on needs.
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

constexpr SkSize kCanvas = {620, 250};
constexpr SkColor4f kGround = hexColor(0x14181d);
constexpr SkColor4f kPlate = hexColor(0x223039);
constexpr SkColor4f kInk = hexColor(0xe8eef2);
constexpr SkColor4f kKey = hexColor(0xf0f4f7);
constexpr SkColor4f kAsh = hexColor(0x8ea0ad);

/** A tile whose child fills the whole box, so a mark under the children
 *  would be covered and a mark over them is not. */
Element tile(const char* caption, Element plate) {
  return box()
      .column()
      .gap(10)
      .flexBasis(0)
      .flexGrow(1)
      .alignItems(Align::Center)
      .children(
          {plate.width(pct(100)).height(120).borderRadius({10}).children(
               {box()
                    .cover()
                    .borderRadius({10})
                    .fill(kPlate)
                    .justifyContent(Justify::Center)
                    .alignItems(Align::Center)
                    .children(
                        {text("A FULL-BLEED CHILD")
                             .font({.size = 14, .color = kInk, .track = 2})})}),
           text(caption).font({.size = 12, .color = kAsh})});
}

}  // namespace

struct ForegroundVerb {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().row().gap(18).padding(24).children({
        // Under the fill and under the children: the child covers it.
        tile("background(keyline)",
             box().background(
                 stroke(3, Fill::color(kKey), PathFormat::Align::Inner))),
        // Over the children: the keyline stands.
        tile("foreground(keyline)",
             box().foreground(
                 stroke(3, Fill::color(kKey), PathFormat::Align::Inner))),
    });
  }
};

SIGIL_SKETCH(ForegroundVerb, "Reference · Compose",
             "a keyline under and over a full-bleed child")
