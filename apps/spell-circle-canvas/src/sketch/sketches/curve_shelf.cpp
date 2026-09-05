/** @file
 * curve_shelf — every parametric curve the kit generates, one to a cell.
 *
 * These do not generate a closed SHAPE from parameters the way a polygon
 * or a squircle does; they generate a curve DEFINED by a parameter. Each
 * evaluates in a UNIT frame centred on the box — x and y in [-1, 1] —
 * and is then scaled onto the node's half-extents, so a curve keeps its
 * proportions when the box changes and an amplitude means the same thing
 * in every cell on this shelf.
 *
 * Each is a comparable VALUE, so a node shaped by one prunes like any
 * other. The exception is at the top left: the raw `parametric(fn, …)`
 * holds a caller's callable, which cannot compare, so a node shaped by
 * it re-records every render. The keyed spelling used here — a name plus
 * the sampling parameters — is the prunable one, on the author's
 * contract that one key always means one curve.
 *
 * Read the pairs. Rose is the rule about k: odd gives k petals, even
 * gives 2k. Spiral's flag is the whole difference between even spacing
 * and a constant angle. Trochoid's is whether the rolling circle runs
 * outside or inside the fixed one.
 *
 * EDIT THESE FIRST
 *   The frequency pair in each cell — a:b for the Lissajous and the
 *   harmonograph, k for the rose, R:r for the trochoid.
 *   kWeight — the stroke width every curve is drawn at, px.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Curves.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 598};
constexpr float kCell = 200;
constexpr float kPicture = 176;
constexpr float kWeight = 1.5f;  // every curve drawn at one width

constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

/** The house sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.captionWhere = kit::Caption::Where::Below;
  look.type.captionLabel = {.size = 11.5f, .mono = true};
  look.type.captionNote = {.size = 10.5f, .track = 0.2f};
  look.spacing.captionGap = 8;
  look.spacing.captionNoteGap = 3;
  return look;
}

/** A specimen's name is a legend under the thing it names, so the
 *  caption stands below the body on this shelf rather than around it. */

/** One specimen: the curve stroked inside a bordered plate, its call
 *  spelled under it and the rule it illustrates under that. */
Element cell(const char* call, const char* note, Shape curve) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture})
          .child(box()
                     .absolute()
                     .inset(12)
                     .shape(std::move(curve))
                     .stroke(stroke(kWeight, Fill::color(kFigure)))));
}

}  // namespace

struct CurveShelf final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("CURVE SHELF \xc2\xb7 shapes:: parametric, "
                       "lissajous, harmonograph, rose, spiral, trochoid"),
         .subtitle = toU8("dials \xc2\xb7 the two frequency parameters in "
                          "each cell \xc2\xb7 the sample count \xc2\xb7 "
                          "the stroke width (1.5 px, one for the shelf)"),
         .footer = toU8("every curve here evaluates in the unit frame "
                        "and is scaled onto the node's half-extents, so "
                        "a cell twice the size draws the same figure "
                        "twice as large and never a different one")},
        kit::cells(
            {.cells =
                 {kit::cells(
                      {.cells =
                           {cell("parametric(\"epicycle\", f)",
                                 "the KEYED escape hatch \xe2\x80\x94 "
                                 "your callable, comparable by name",
                                 shapes::parametric(
                                     "epicycle",
                                     [](float t) {
                                       return SkPoint{
                                           0.62f * std::cos(t) +
                                               0.34f * std::cos(7 * t),
                                           0.62f * std::sin(t) +
                                               0.34f * std::sin(7 * t)};
                                     },
                                     0.0f,
                                     6.2831853f, 1400)),
                            cell("lissajous(3, 2, 90)",
                                 "x = sin(a\xc2\xb7t + \xce\xb4), "
                                 "y = sin(b\xc2\xb7t)",
                                 shapes::lissajous(3, 2, 90)),
                            cell("lissajous(5, 4, 45)",
                                 "the ratio picks the family, "
                                 "\xce\xb4 the phase",
                                 shapes::lissajous(5, 4, 45)),
                            cell("harmonograph(3,2,0,.06,5)",
                                 "amplitudes DECAY, so a real pendulum "
                                 "figure spirals in",
                                 shapes::harmonograph(3, 2, 0, 0.06f, 5, 9)),
                            cell("rose(5)",
                                 "r = cos(k\xc2\xb7\xce\xb8) \xc2\xb7 "
                                 "odd k gives k petals",
                                 shapes::rose(5))},
                       .gap =
                           12}),
                  kit::cells(
                      {.cells =
                           {cell("rose(4)",
                                 "\xe2\x80\xa6"
                                 "and EVEN k gives 2k, "
                                 "which is the rule about this family",
                                 shapes::
                                     rose(4)),
                            cell("spiral(4)",
                                 "Archimedean \xe2\x80\x94 even "
                                 "spacing: a clock spring",
                                 shapes::
                                     spiral(4)),
                            cell("spiral(4, true, 0.34)",
                                 "logarithmic \xe2\x80\x94 a constant "
                                 "angle: a nautilus",
                                 shapes::
                                     spiral(4, true, 0.34f)),
                            cell("trochoid(5, 3, 5, false, 3)",
                                 "an EPItrochoid: the rolling circle "
                                 "runs outside the fixed one",
                                 shapes::
                                     trochoid(5, 3, 5, false, 3)),
                            cell("trochoid(5, 3, 5, true, 3)",
                                 "\xe2\x80\xa6"
                                 "and the same three "
                                 "numbers with it running inside",
                                 shapes::
                                     trochoid(5, 3, 5, true, 3))},
                       .gap =
                           12})},
             .column = true,
             .gap = 16})));
  }
};

SIGIL_SKETCH(CurveShelf, "Specimen",
             "every parametric curve the geometry kit generates, one "
             "captioned cell each, each drawn in the unit frame its own "
             "cell scales")
