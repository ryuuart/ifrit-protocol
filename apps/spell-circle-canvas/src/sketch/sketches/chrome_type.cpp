/** @file
 * chrome_type — the layer styles on LETTERS: the same aqua, chrome and
 * gloss bundles a pill wears, dressing a word's glyph outline instead of
 * its box, beside the same styles on the box they used to get.
 */

// A DECORATION WAS NEVER ABOUT A BOX. Every layer style in the brush tier
// — the aqua gel, the y2k chrome, a bevel, an inner shadow, an outer glow
// — is drawn ACROSS AN OUTLINE, and until now the outline a text leaf
// handed them was its rectangle. That is why a chrome style on a word
// bevelled a slab behind the word.
//
// `boundary(Boundary::Glyphs)` hands them the glyph contours the placement
// produced instead. Nothing else changes: no new preset, no second code
// path, no per-style special case for text. The rows on this page are
// pairs — the same style value, once on the box and once on the letters —
// so the difference is the boundary and nothing else.
//
// The rows:
//
//   · CHROME — y2kChrome(), the whole bundle: shadow, palette ramp,
//     horizon sliver, chisel bevel, keyline. On glyphs the horizon
//     crosses every letter at the same height, because the ramp is read
//     off the node's box while the shape it fills is the letters.
//   · AQUA — aquaGel(), body and gloss. Its lens is a fraction of the
//     node's height, so on a word it reads as one lens across the whole
//     wordmark rather than one per letter.
//   · BEVEL + GLOW — the two plainest decorations, to show that the
//     boundary is a property of the NODE and not of any one style.
//
// EDIT THESE FIRST
//   kDisplay      — the display size the specimens are set at. Bigger
//                   letters give a bevel more room to read.
//   kWordmark     — the word itself. A wider one lengthens the horizon.

#include <sigilsketch/canvas/Sketch.h>

#include <sigilcompose/brush/Brush.h>
#include <sigilcompose/typography/Typography.h>

#include <string>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize{1180, 820};

namespace chrome {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;
constexpr float kMargin = 60;
constexpr float kDisplay = 76;
constexpr const char* kWordmark = "CHROME";

const SkColor4f kGround{0.086f, 0.090f, 0.106f, 1};
const SkColor4f kGroundLift{0.129f, 0.137f, 0.157f, 1};
const SkColor4f kPale{0.796f, 0.816f, 0.847f, 1};
const SkColor4f kFaint{0.796f, 0.816f, 0.847f, 0.42f};

sk_sp<SkTypeface> display() {
  static sk_sp<SkTypeface> face =
      pickFace({"Helvetica Neue", "Inter", "Arial Black", "Helvetica"},
               SkFontStyle::Bold());
  return face;
}
sk_sp<SkTypeface> grotesque() {
  static sk_sp<SkTypeface> face =
      pickFace({"Helvetica Neue", "Inter", "Helvetica", "Arial"});
  return face;
}

weave::TextStyle wordmark(SkColor4f colour = {0.7f, 0.73f, 0.78f, 1}) {
  return type({.face = display(), .size = kDisplay, .color = colour,
               .track = 1.5f, .weight = 800.0f});
}
weave::TextStyle label(float size = 9.0f, SkColor4f colour = kFaint,
                       float track = 1.8f) {
  return type(
      {.face = grotesque(), .size = size, .color = colour, .track = track});
}

}  // namespace chrome

struct ChromeType final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.background(chrome::kGround);
    ctx.captureAt(0.4);
    ctx.composer.render(describe());
  }

  /** One PAIR: the same style value on a box and on the letters, captioned
   *  with the one thing that differs. */
  Element pair(const char* name, const LayerStyle& style,
               SkColor4f letterInk = {0.72f, 0.75f, 0.80f, 1}) {
    namespace c = chrome;
    return box()
        .column()
        .gap(10)
        .child(text(toU8(name), c::label(9.5f, c::kPale, 2.6f)))
        .child(box()
                   .row()
                   .gap(26)
                   .alignItems(Align::Center)
                   // The box it used to get: the style dresses the node's
                   // own shape and the word sits inside it.
                   .child(box()
                              .column()
                              .gap(6)
                              .child(text(toU8("Boundary::Auto"),
                                          c::label(8, c::kFaint, 1.0f)))
                              .child(box()
                                         .padding(18)
                                         .corners({6})
                                         .style(style)
                                         .child(text(toU8(c::kWordmark),
                                                     c::wordmark(letterInk)))))
                   // The letters: the same value, the other boundary.
                   .child(box()
                              .column()
                              .gap(6)
                              .child(text(toU8("Boundary::Glyphs"),
                                          c::label(8, c::kPale, 1.0f)))
                              .child(box().padding(18).child(
                                  text(toU8(c::kWordmark),
                                       c::wordmark({0, 0, 0, 0}))
                                      .boundary(Boundary::Glyphs)
                                      .style(style)))));
  }

  Element describe() {
    namespace c = chrome;

    // The two plainest decorations, hand-bundled: what is under the shape
    // and what is over it, which is all a LayerStyle is.
    LayerStyle bevelAndGlow;
    bevelAndGlow.under.push_back(
        styles::OuterGlow{{0.45f, 0.72f, 1.0f, 0.85f}, 16.0f, 1.0f});
    bevelAndGlow.over.push_back(
        styles::BevelEmboss{.depth = 3.0f, .size = 4.0f, .angleDeg = 120.0f});

    return box()
        .fill(Material::linear({0, 0}, {0, c::kH},
                               {{0.0f, c::kGroundLift}, {1.0f, c::kGround}}))
        .child(box()
                   .absolute()
                   .inset(c::kMargin, c::kMargin - 16, 0, 0)
                   .column()
                   .gap(6)
                   .child(text(toU8("A DECORATION WAS NEVER ABOUT A BOX"),
                               c::label(12, c::kPale, 3.6f)))
                   .child(text(toU8("the same style value, twice \xe2\x80\x94 "
                                    "once dressing the node's shape, once "
                                    "dressing its glyph outline"),
                               c::label(10, c::kFaint, 0.3f))))
        .child(box()
                   .absolute()
                   .inset(c::kMargin, c::kMargin + 62, 0, 0)
                   .column()
                   .gap(38)
                   .child(pair("Y2K CHROME", styles::y2kChrome()))
                   .child(pair("AQUA GEL",
                               styles::aquaGel(hex(0x1E8FFF),
                                               {.expectedHeight = 108.0f})))
                   .child(pair("BEVEL + GLOW", bevelAndGlow)))
        .child(text(toU8("no new preset and no second code path: the style "
                         "is handed a different outline, and every style "
                         "already written follows"),
                    c::label(10, c::kFaint, 0.2f))
                   .absolute()
                   .inset(c::kMargin, c::kH - 36, 0, 0));
  }
};

}  // namespace

SIGIL_SKETCH_AS(ChromeType, "chrome_type", "Catalog \xc2\xb7 Type & grid",
                "layer styles dressing glyph outlines, beside the same on a box")
