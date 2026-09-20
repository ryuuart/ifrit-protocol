/** @file
 * crt_bloom — the primitive and the construction it names, on identical
 * content either side of one seam.
 *
 * A halo is a dim spread copy of the thing that glows, and the library
 * has a verb for exactly that: `Effect::glow(colour, sigma)` re-emits the
 * layer blurred beneath itself. This sheet puts that verb beside the
 * stack it names — the same word, the same size, the same two colours,
 * the same spread — so the only thing that differs across the seam is
 * which construction drew it.
 *
 *   LEFT   one node. `Effect::glow(halo, sigma)` on the headline itself.
 *     The halo is the headline's own coverage, so its weight, its spacing
 *     and its position cannot drift from the letters — there is nothing
 *     to keep in step.
 *   RIGHT  two nodes. The headline described TWICE: once sharp on top,
 *     once underneath, blurred and blended with `kPlus`. The blurred copy
 *     carries `Cache::Texture` — it never changes, and a blur re-run every
 *     frame over a headline that is standing still is the most expensive
 *     nothing in a scene.
 *
 * THE TWO ARE NOT THE SAME PICTURE, and the difference is the reason to
 * build one by hand. `glow` composites its halo UNDER the content, which
 * is a drop shadow at zero offset; the stack ADDS it, so the halo and the
 * letters sum where they overlap and the core blows out. A phosphor adds.
 * A shadow does not. Everything else about the two is identical, which is
 * what makes the comparison worth drawing rather than describing.
 *
 * THE TUBE is `field::crtOverlay`: hard scanlines at a stated pitch and a
 * corner falloff, in black, with the alpha carrying both, laid over each
 * panel as the last layer. It is the library's own tube — the sheet does
 * not draw one — so each construction is judged through the same glass.
 *
 * EDIT THESE FIRST
 *   kSigma      — how far both halos spread. One number, both
 *                 constructions, so they stay comparable.
 *   kPitch      — the tube's scanline period, px. Six is a phosphor
 *                 pitch; at two it is a screen door, at twenty a
 *                 venetian blind.
 *   kHalo/kCore — the two colours. They ADD on the right, so the halo is
 *                 a CONTRIBUTION and not a colour: darker than you
 *                 expect.
 */

// TAGS: Materials/Compositing

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr float kPanel = 440;   // one panel's drawn width, px
constexpr float kPanelH = 320;  // …and its height
/** The spread, shared by both constructions so the two are comparable. */
constexpr float kSigma = 14.0f;
/** The tube's scanline period, px. */
constexpr float kPitch = 6.0f;

constexpr SkColor4f kGround{0.02f, 0.03f, 0.05f, 1};
constexpr SkColor4f kCore{0.616f, 0.949f, 1.0f, 1};
constexpr SkColor4f kHalo{0.165f, 0.498f, 0.588f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.02f, 0.03f, 0.05f, 1};
  look.palette.ink = {0.90f, 0.93f, 0.97f, 1};
  look.palette.rule = {0.16f, 0.20f, 0.26f, 1};
  look.type.captionLabel = {.size = 13, .track = 0.4f};
  look.spacing.marginX = 40;
  look.spacing.marginTop = 40;
  return look;
}

/** THE WORD, identical either side of the seam: one size, one tracking,
 *  and the colour the construction paints it in. The face is STATED as the
 *  font context's own family, because the word is not set in the sheet's
 *  text face. */
Element headline(SkColor4f color) {
  return text(u8"PHOSPHOR")
      .font({.face = weave::defaultFace(),
             .size = 62,
             .color = color,
             .track = 1.5f});
}

/** THE TUBE, over whatever the panel drew: the library's own scanline
 *  and corner falloff, in black, so both panels are seen through one
 *  glass. */
Element tube() {
  return box().cover().zIndex(9).fill(
      mskia::Paint::recipe(field::crtOverlay(kPitch, 0.10f)));
}

/** A panel: the ground, the construction, the tube. Both panels are laid
 *  out identically, so the headline lands in the same place in each. */
Element panel(Element construction) {
  return stack()
      .width(kPanel)
      .height(kPanelH)
      .fill(Fill::color(kGround))
      .alignItems(Align::Center)
      .justifyContent(Justify::Center)
      .children({std::move(construction), tube()});
}

}  // namespace

struct CrtBloom {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // Nothing on the sheet reads the clock: both halos are static and the
    // tube is a function of the box, so the plate is the first moment.
    sketch::kit::stage(ctx, {.size = {1000, 740}, .captureAt = 0.05});

    // LEFT — one node. The effect owns the whole construction.
    Element primitive =
        panel(headline(kCore).filter(mskia::Effect::glow(kHalo, kSigma)));

    // RIGHT — two nodes in the same place, the second blurred and ADDED.
    // The blur is spelled as an axis-aligned directional blur rather than
    // a raw Skia filter: at 0° it IS a Gaussian blur, and it carries a
    // comparable recipe, so a re-described equal blur prunes where a
    // built filter can only compare by pointer.
    // The blurred copy is given the WHOLE panel to spread in. A blur is
    // clipped by its own node's box, so putting the effect on the tight
    // text node would cut the halo off square at the letters' bounds.
    Element built =
        panel(stack()
                  .alignItems(Align::Center)
                  .justifyContent(Justify::Center)
                  .children({kit::centred(headline(kHalo))
                                 .cover()
                                 .zIndex(1)
                                 .filter(mskia::Effect::directionalBlur(
                                     kSigma, 0.0f, kSigma))
                                 .blend(SkBlendMode::kPlus)
                                 .cache(Cache::Texture),
                             headline(kCore).zIndex(2)}));

    ctx.composer.render(sketch::kit::page(
        {.title = "A shadow or a phosphor?",
         .subtitle = "Two ways to build a bloom, seen through the same tube",
         .footer = "Read the letter interiors as well as the spread: the "
                   "compositing operation changes their brightness."},
        box().column().gap(24).children(
            {sketch::kit::sectionHeader(
                 {.label = "THE CONTROL",
                  .note = "Identical word · 62 px type · 14 px spread · 6 px "
                          "scanlines"}),
             sketch::kit::comparison(
                 {.cases = {{.title = "HALO UNDER THE CORE",
                             .control = "Effect::glow(halo, 14)",
                             .figure = std::move(primitive),
                             .note = "One node. The blurred coverage sits "
                                     "beneath the sharp letters."},
                            {.title = "HALO ADDED TO THE CORE",
                             .control = "directionalBlur(14) + kPlus",
                             .figure = std::move(built),
                             .note = "Two layers. Their light adds where they "
                                     "overlap, lifting the bright core."}},
                  .measure = 920,
                  .gap = 40}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(40)
                 .children({box().column().gap(12).children(
                                {sketch::kit::sectionHeader(
                                     {.label = "SOURCE OVER", .note = ""}),
                                 document::caption(
                                     "The sharp glyph covers the halo. This "
                                     "is a centred shadow, with its outline "
                                     "always tied to the source.")
                                     .width(440)}),
                            box().column().gap(12).children(
                                {sketch::kit::sectionHeader(
                                     {.label = "ADDITIVE LIGHT", .note = ""}),
                                 document::caption(
                                     "The halo contributes to the glyph as "
                                     "well as its surroundings. The static "
                                     "blurred layer is retained as a texture.")
                                     .width(440)})})})));
  }
};

SIGIL_SKETCH(
    CrtBloom, "Kit · API",
    "Effect::glow beside the stack it names — one node "
    "against two on identical content, under the same field::crtOverlay tube")
