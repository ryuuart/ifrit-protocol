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

#include <sigilcompose/brush/Brush.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Gel.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize{1180, 820};

namespace chrome {

constexpr float kH = kSceneSize.fHeight;
constexpr float kMargin = 60;
constexpr float kDisplay = 76;
constexpr const char* kWordmark = "CHROME";

const SkColor4f kGround{0.086f, 0.090f, 0.106f, 1};
const SkColor4f kGroundLift{0.129f, 0.137f, 0.157f, 1};
const SkColor4f kPale{0.796f, 0.816f, 0.847f, 1};
const SkColor4f kFaint{0.796f, 0.816f, 0.847f, 0.42f};

weave::TextStyle wordmark(SkColor4f colour = {0.7f, 0.73f, 0.78f, 1}) {
  return weave::textStyle(
      {.face = weave::ports::face({"Helvetica Neue", "Inter", "Arial Black",
                                   "Helvetica"},
                                  SkFontStyle::Bold()),
       .size = kDisplay,
       .color = colour,
       .track = 1.5f,
       .weight = 800.0f});
}

/** This sheet's look: a grotesque throughout, the pale ink and the faint
 *  ash it is set in, and a caption that names the boundary over the
 *  specimen with nothing under it — the picture is the note. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look;
  look.palette = {
      .ground = kGround, .ink = kPale, .ash = kFaint, .rule = kFaint};
  look.type.sans =
      weave::ports::face({"Helvetica Neue", "Inter", "Helvetica", "Arial"});
  look.type.title = {.size = 12, .track = 3.6f};
  look.type.subtitle = {.size = 10, .track = 0.3f};
  look.type.footer = {.size = 10, .track = 0.2f};
  look.type.captionLabel = {.size = 8.5f, .track = 1.0f};
  look.type.captionNote = {.size = 8, .track = 0.3f};
  look.spacing.marginX = kMargin;
  look.spacing.marginTop = kMargin - 16;
  look.spacing.marginBottom = 30;
  look.spacing.captionGap = 8;
  look.spacing.captionNoteGap = 3;
  look.captionWhere = kit::Caption::Where::Above;
  return look;
}

}  // namespace chrome

struct ChromeType final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(chrome::sheetTheme());
    sketch::kit::stage(ctx, {.size = kSceneSize, .captureAt = 0.4});
    ctx.composer.render(describe());
  }

  /** One PAIR: the same style value on a box and on the letters, as two
   *  cells of one run so the two boundaries are captioned in one voice and
   *  the only thing that differs between them is the boundary. */
  Element pair(const char* name, const LayerStyle& style,
               SkColor4f letterInk = {0.72f, 0.75f, 0.80f, 1}) {
    namespace c = chrome;
    // The box it used to get: the style dresses the node's own shape and
    // the word sits inside it.
    Element onBox = sketch::kit::caption(
        0, toU8("Boundary::Auto"), toU8("the node's rectangle"),
        box()
            .padding(18)
            .corners({6})
            .style(style)
            .child(text(toU8(c::kWordmark), c::wordmark(letterInk))));
    // The letters: the same value, the other boundary.
    Element onGlyphs = sketch::kit::caption(
        0, toU8("Boundary::Glyphs"),
        toU8("the contours the placement produced"),
        box().padding(18).child(
            text(toU8(c::kWordmark), c::wordmark({0, 0, 0, 0}))
                .boundary(Boundary::Glyphs)
                .style(style)));
    // The pair's own name stands wider and larger than a cell's call.
    const sketch::kit::Theme& look = sketch::kit::theme();
    return kit::cell(
        {.where = kit::Caption::Where::Above,
         .label = look.sans(9.5f, c::kPale, 2.6f),
         .note = look.sans(8, c::kFaint, 0.3f),
         .gap = 10},
        toU8(name), u8"",
        kit::cells({.cells = {std::move(onBox), std::move(onGlyphs)},
                    .gap = 26,
                    .divider = Fill::color(c::kFaint),
                    .align = Align::Center}));
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

    std::vector<Element> rows;
    rows.push_back(pair("Y2K CHROME", kit::y2kChrome()));
    rows.push_back(pair(
        "AQUA GEL", kit::aquaGel(hex(0x1E8FFF), {.expectedHeight = 108.0f})));
    rows.push_back(pair("BEVEL + GLOW", bevelAndGlow));

    return sketch::kit::page(
        {.title = u8"A DECORATION WAS NEVER ABOUT A BOX",
         .subtitle = u8"the same style value, twice \u2014 once dressing the "
                     u8"node's shape, once dressing its glyph outline",
         .footer = u8"no new preset and no second code path: the style is "
                   u8"handed a different outline, and every style already "
                   u8"written follows",
         .ground = linearGradient({0, 0}, {0, c::kH},
                                  {c::kGroundLift, c::kGround})},
        kit::cells({.cells = std::move(rows), .column = true, .gap = 30}));
  }
};

}  // namespace

SIGIL_SKETCH_AS(ChromeType, "chrome_type", "Catalog \xc2\xb7 Type",
                "layer styles dressing glyph outlines, beside the same on a box")
