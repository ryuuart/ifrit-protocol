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
 * what makes the comparison worth drawing rather than describing — and
 * why the seam is drawn and labelled rather than left to be inferred.
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

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

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
constexpr SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
constexpr SkColor4f kAsh{0.62f, 0.66f, 0.74f, 1};
constexpr SkColor4f kRule{0.16f, 0.20f, 0.26f, 1};
constexpr SkColor4f kSeam{0.95f, 0.62f, 0.24f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(13, kInk, 0.4f),
          .note = label(11, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kPanel};
}

Element headline(SkColor4f color) {
  return text(u8"PHOSPHOR", label(62, color, 1.5f));
}

/** THE TUBE, over whatever the panel drew: the library's own scanline
 *  and corner falloff, in black, so both panels are seen through one
 *  glass. */
Element tube() {
  return box()
      .absolute()
      .inset(0)
      .zIndex(9)
      .fill(mskia::Paint::recipe(field::crtOverlay(kPitch, 0.10f)));
}

/** A panel: the ground, the construction, the tube. Both panels are laid
 *  out identically, so the headline lands in the same place in each. */
Element panel(Element construction) {
  return stack()
      .width(kPanel)
      .height(kPanelH)
      .fill(Fill::color(kGround))
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .child(std::move(construction))
      .child(tube());
}

/** THE SEAM, labelled: the hairline the comparison is read across. */
Element seam() {
  return box()
      .column()
      .alignItems(Align::Center)
      .gap(6)
      .child(text(u8"SEAM", label(10, kSeam, 2.0f)))
      .child(box()
                 .width(2)
                 .height(kPanelH)
                 .fill(Fill::color({kSeam.fR, kSeam.fG, kSeam.fB, 0.55f})));
}

}  // namespace

struct CrtBloom final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1000, 500);
    ctx.background(kGround);
    // Nothing on the sheet reads the clock: both halos are static and the
    // tube is a function of the box.
    ctx.captureAt(0.05);

    // LEFT — one node. The effect owns the whole construction.
    Element primitive =
        panel(headline(kCore).effect(mskia::Effect::glow(kHalo, kSigma)));

    // RIGHT — two nodes in the same place, the second blurred and ADDED.
    // The blur is spelled as an axis-aligned directional blur rather than
    // a raw Skia filter: at 0° it IS a Gaussian blur, and it carries a
    // comparable recipe, so a re-described equal blur prunes where a
    // built filter can only compare by pointer.
    Element built =
        panel(stack()
                  .alignItems(Align::Center)
                  .justify(Justify::Center)
                  .child(headline(kHalo)
                             .absolute()
                             .zIndex(1)
                             .effect(mskia::Effect::directionalBlur(
                                 kSigma, 0.0f, kSigma))
                             .blend(SkBlendMode::kPlus)
                             .cache(Cache::Texture))
                  .child(headline(kCore).zIndex(2)));

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("CRT BLOOM \xc2\xb7 Effect::glow beside the stack "
                           "it names"),
             .subtitle = toU8("identical content either side of the seam "
                              "\xe2\x80\x94 one word, one size, one spread, "
                              "one tube; only the construction differs"),
             .footer = toU8("glow composites its halo UNDER the letters, "
                            "which is a drop shadow at zero offset; the "
                            "stack ADDS it, so the core blows out \xc2\xb7 a "
                            "phosphor adds, a shadow does not"),
             .titleStyle = label(15, kInk, 2.0f),
             .subtitleStyle = label(11, kAsh, 0.6f),
             .footerStyle = label(10.5f, kAsh, 0.2f),
             .marginX = 30,
             .marginTop = 22,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells = {kit::cell(voice(),
                                     toU8("Effect::glow(halo, 14)"),
                                     toU8("one node \xe2\x80\x94 the halo is "
                                          "the headline's own coverage, so "
                                          "nothing can drift out of step "
                                          "with the letters"),
                                     std::move(primitive)),
                           seam(),
                           kit::cell(
                               voice(),
                               toU8("directionalBlur(14, 0\xc2\xb0, 14) + "
                                    "kPlus"),
                               toU8("two nodes \xe2\x80\x94 the same "
                                    "headline described twice, the lower "
                                    "copy blurred, added and baked to a "
                                    "texture because it never changes"),
                               std::move(built))},
                 .gap = 22}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(CrtBloom, "Kit \xc2\xb7 API",
             "Effect::glow beside the stack it names \xe2\x80\x94 one node "
             "against two on identical content, either side of a labelled "
             "seam and under the same field::crtOverlay tube")
