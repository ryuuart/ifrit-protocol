/** @file
 * text — the content forms: the inheriting leaf, the leaf set in a whole
 * style, and mixed text as a comparable value.
 *
 * A reference example. It stands outside the sketch registry, so it is
 * photographed with `--frame` and never enters the plate sweep.
 */

#include <sigilcompose/core/Core.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/style/Style.h>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {620, 300};
constexpr material::Color kGround = hexColor(0x14181d);
constexpr material::Color kInk = hexColor(0xe2e9ee);
constexpr material::Color kAsh = hexColor(0x8ea0ad);
constexpr material::Color kAccent = hexColor(0xe0a03c);

/** A total style: a `weave::TextStyle` states every field itself, so a
 *  leaf set in one inherits nothing from the tree above it. */
weave::TextStyle stated() {
  weave::TextStyle style;
  style.shaping.fontSize = 26;
  style.paint.foreground.setColor4f(kAccent, nullptr);
  return style;
}

Element row(const char* caption, Element leaf) {
  return box().column().gap(6).children(
      {text(caption).font({.size = 12, .color = kAsh}), std::move(leaf)});
}

}  // namespace

struct TextElement {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .column()
        .gap(26)
        .padding(28)
        // The font and the ink everything under this node is set in.
        .font({.size = 20})
        .ink(kInk)
        .children({
            row("text(utf8)", text("Set in the font and ink in force.")),
            row("text(utf8, style)",
                text("Set in a style of its own.", stated())),
            row("text(rich)", text(weave::rich()
                                       .add("Mixed text as ")
                                       .add("one comparable value",
                                            weave::Type{.color = kAccent}))),
        });
  }
};

SIGIL_SKETCH(TextElement, "Reference · Compose",
             "the text leaf in its three content forms")
