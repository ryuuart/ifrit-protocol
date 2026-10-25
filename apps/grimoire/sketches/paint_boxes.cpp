/** @file
 * paint_boxes — WHICH BOX A PAINT IS STRETCHED OVER. One ramp, authored
 * in the unit square, read against every `PaintBox`: the ink on a
 * subtree over the element, the subtree and the canvas; the ink of one
 * passage laid once and then restarted on every glyph, word and line;
 * and a fill over the canvas through a row of cards, beside the
 * material's own spelling of root coordinates, `worldSpace`.
 *
 * Because the ramp is the same description everywhere, what changes from
 * cell to cell is the box alone. A text leaf's element box is its
 * text-metric box, which is why the element row repeats the whole ramp
 * per word; a unit's box runs across its advances and from its cap top
 * down to its baseline.
 *
 * EDIT THESE FIRST
 *   ramp() — the one paint every cell reads.
 *   kPassage — the two lines the unit row inks.
 */
// TAGS: Typography/Paint

#include <sigilcompose/core/Core.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <string_view>
#include <utility>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 990};
constexpr float kMeasure = 1020;
constexpr std::u8string_view kPassage = u8"EMBER GLASS\nSALT WATER";

/** The one ramp every cell is painted with, authored in the unit square
 *  so that every cell is a statement about the BOX and never about the
 *  ramp. */
material::Material ramp() {
  return material::linearGradient({0, 0}, {1, 0},
                                  {{0.0f, {0.96f, 0.29f, 0.24f, 1}},
                                   {0.5f, {0.98f, 0.76f, 0.19f, 1}},
                                   {1.0f, {0.16f, 0.45f, 0.93f, 1}}});
}

sigil::weave::Type display(float size) {
  return {.face = sigil::weave::ports::face(
              {"Avenir Next Heavy", "Helvetica Neue Bold", "Arial Black"}),
          .size = size,
          .track = size * 0.02f};
}

/** A case whose figure is @p picture centred in a well @p width wide. */
sketch::kit::ComparisonCase specimen(const char* title, const char* control,
                                     const char* note, float width,
                                     float height, Element picture) {
  return {.title = title,
          .control = control,
          .figure = sketch::kit::well({.width = width,
                                       .height = height,
                                       .content = sketch::kit::Well::Content{}},
                                      std::move(picture)),
          .note = note};
}

/** Three words under one box that states the ink. */
Element subtree(PaintBox over) {
  return box()
      .row()
      .gap(14)
      .ink(ramp(), over)
      .children({text(u8"ONE").font(display(42)),
                 text(u8"TWO").font(display(42)),
                 text(u8"SIX").font(display(42))});
}

/** Two lines of two words, inked with the ramp over @p unit. */
Element passage(PaintBox unit) {
  return text(kPassage).font(display(26)).ink(ramp(), unit);
}

/** A row of cards, each filled with the SAME paint over the canvas: what
 *  a card shows is decided by where it stands. */
Element cards() {
  return box().row().gap(12).children({each(3, [](int) {
    return box().width(96).height(96).borderRadius({10}).fill(ramp(),
                                                              PaintBox::Canvas);
  })});
}

/** Two nodes filled with the ramp as a material, anchored to each node's
 *  own box or, with `worldSpace`, to the root's. */
Element pair(bool rootAnchored) {
  return box().row().gap(12).children({each(2, [&](int) {
    return box().width(150).height(96).borderRadius({10}).fill(
        ramp().worldSpace(rootAnchored));
  })});
}

}  // namespace

struct PaintBoxes {
  void setup(sketch::Scene2DContext& ctx) {
    sketch::kit::stage(ctx, {.theme = sketch::kit::studyTheme(),
                             .size = kCanvas,
                             .captureAt = 0.05});
    const float third = (kMeasure - 36) / 3, quarter = (kMeasure - 54) / 4;

    ctx.composer.render(sketch::kit::page(
        {.title = "The box a paint is stretched over",
         .subtitle = "One unit-square ramp read against every paint box · "
                     "only the box changes from cell to cell",
         .footer = "A text leaf's element box is its text-metric box, which "
                   "is why the element row repeats the whole ramp per "
                   "word."},
        box().column().gap(30).children(
            {sketch::kit::section(
                 {.label = "INK OVER A BOX",
                  .note = "box().ink(ramp, PaintBox::…) over three words"},
                 sketch::kit::comparison(
                     {.cases =
                          {specimen("ELEMENT", "PaintBox::Element",
                                    "Each word maps the ramp onto its "
                                    "own metrics.",
                                    third, 120, subtree(PaintBox::Element)),
                           specimen("SUBTREE", "PaintBox::Subtree",
                                    "One ramp across the row; each word "
                                    "its own slice.",
                                    third, 120, subtree(PaintBox::Subtree)),
                           specimen("CANVAS", "PaintBox::Canvas",
                                    "The field belongs to the canvas, "
                                    "not to the row.",
                                    third, 120, subtree(PaintBox::Canvas))},
                      .measure = kMeasure,
                      .gap = 18})),
             sketch::kit::section(
                 {.label = "INK RESTARTED ON EACH UNIT",
                  .note = "text(…).ink(ramp, PaintBox::…) over one passage"},
                 sketch::kit::comparison(
                     {.cases = {specimen("THE PASSAGE", "PaintBox::Element",
                                         "Laid once across the whole of it.",
                                         quarter, 130,
                                         passage(PaintBox::Element)),
                                specimen("EACH GLYPH", "PaintBox::Glyph",
                                         "Afresh on every letter.", quarter,
                                         130, passage(PaintBox::Glyph)),
                                specimen("EACH WORD", "PaintBox::Word",
                                         "Afresh on every word.", quarter, 130,
                                         passage(PaintBox::Word)),
                                specimen("EACH LINE", "PaintBox::Line",
                                         "Afresh on every line.", quarter, 130,
                                         passage(PaintBox::Line))},
                      .measure = kMeasure,
                      .gap = 18})),
             sketch::kit::section(
                 {.label = "THE ROOT'S COORDINATES",
                  .note = "A fill over the canvas box, and the material "
                          "anchored to the root, read the same field"},
                 sketch::kit::comparison(
                     {.cases = {specimen(
                                    "FILL · CANVAS BOX",
                                    "fill(ramp, PaintBox::Canvas)",
                                    "Three cards, one paint: windows cut in "
                                    "a single gradient.",
                                    third, 130, cards()),
                                specimen(
                                    "MATERIAL · EACH NODE",
                                    "ramp.worldSpace(false)",
                                    "Each node repeats the whole ramp in its "
                                    "own box.",
                                    third, 130, pair(false)),
                                specimen("MATERIAL · THE ROOT",
                                         "ramp.worldSpace(true)",
                                         "Both nodes sample one field anchored "
                                         "to the page.",
                                         third, 130, pair(true))},
                      .measure = kMeasure,
                      .gap = 18}))})));
  }
};

SIGIL_SKETCH(PaintBoxes, "Study · Paint",
             "one unit-square ramp read against every paint box — ink over "
             "the element, subtree and canvas, ink restarted on each glyph, "
             "word and line, and a canvas fill beside a root-anchored "
             "material")
