/** @file
 * border_weave — the rule around a plaque, and the strands that cross on
 * it.
 *
 * `Border` is one decoration with four modes, and all four FOLLOW THE
 * SILHOUETTE: chamfer the node's outline and the brackets land on the
 * chamfers with no further instruction, which is the whole advantage over
 * four absolutely-placed corner elements. `Continuous` is an ordinary
 * rule; `Bracket` paints only within `corner` px of each corner and
 * `Gapped` paints everything except that; `Weighted` runs continuous but
 * thickens near the turns. `inset` moves the rule inside the outline —
 * negative moves it out — and a second `Border` at a different inset is
 * the whole of a double frame.
 *
 * WHAT COUNTS AS A CORNER is a tangent break of more than
 * `cornerAngleDeg`, and this is the first thing that surprises people: a
 * gently rounded corner has no hard break, so brackets vanish on it and a
 * gapped rule runs all the way round. The last cell is the bracket value
 * on a circle, where there is no break anywhere, and it draws nothing.
 *
 * `brush::weave` is the other half: strands that may trade sides, plus a
 * rule for who passes over whom where they meet. Crossings are found, not
 * declared — which is why the strands here are WAVES at evenly spread
 * phases (`kit::braid`). Parallels are rails and never cross, so an
 * offset strand set cannot braid at all.
 *
 * EDIT THESE FIRST
 *   kWidth, kInset — the rule's weight and how far inside the outline.
 *   kArm — the arc length a bracket keeps or a gap omits, px.
 *   kStrands, kAmplitude, kWavelength — the braid.
 */

// TAGS: Geometry/Diagrams, Patterns/Ornament

#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace crossing = sigil::geometry::path::crossing;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 424};
constexpr float kCell = 163;
constexpr float kPicture = 176;
constexpr float kPlaque = 128;

constexpr float kWidth = 1.8f;  // the rule's weight, px
constexpr float kInset = 7;     // how far inside the outline it runs
constexpr float kArm = 18;      // a bracket's arm, a gap's omission, px
constexpr int kStrands = 3;     // the braid
constexpr float kAmplitude = 5;
constexpr float kWavelength = 34;
constexpr float kChamfer = 14;

constexpr SkColor4f kPlate{0.15f, 0.155f, 0.175f, 1};
constexpr SkColor4f kCool{0.46f, 0.70f, 0.86f, 1};

/** The plaque every cell dresses: a chamfered box, so each corner is a
 *  real tangent break the corner scan can find — except in the last cell,
 *  which is the inscribed circle, a silhouette with no break anywhere. */
Element plaque(bool round = false) {
  Element node = box().width(kPlaque).height(kPlaque).fill(Fill::color(kPlate));
  if (round)
    node.shape(shapes::circle());
  else
    node.shape(shapes::chamfered(kChamfer));
  return node;
}

/** The plate every specimen on this sheet stands on. */
const sketch::kit::Cell kSpecimen{
    .plate = {.width = kCell, .height = kPicture}};

Element cell(const char* call, const char* note, Element body) {
  return sketch::kit::cell(
      kSpecimen, call, note,
      std::move(body).absolute().inset(
          (kCell - kPlaque) / 2, (kPicture - kPlaque) / 2,
          (kCell - kPlaque) / 2, (kPicture - kPlaque) / 2));
}

}  // namespace

struct BorderWeave final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    const sketch::kit::Theme& sheet = sketch::kit::theme();

    const Border bracket{.width = kWidth,
                         .fill = Fill::color(sheet.palette.figure),
                         .inset = kInset,
                         .mode = Border::Mode::Bracket,
                         .corner = kArm};

    ctx.composer.render(sketch::kit::page(
        {.title = "THE RULE AND THE STRANDS · Border's four "
                  "modes, brush::weave over one outline",
         .subtitle = "dials · the width (1.8 px) and inset "
                     "(7 px) · the corner arm (18 px) "
                     "· the strand count (3), amplitude and "
                     "wavelength · the chamfer",
         .footer = "a crossing is DISCOVERED and not declared, so "
                   "the strands of a weave must be waves: n "
                   "oscillations of equal amplitude at evenly "
                   "spread phases must trade sides, and parallels "
                   "are rails that never cross"},
        kit::cells(
            {.cells =
                 {cell("border(1.8, ink, inset 7)",
                       "Continuous · an ordinary rule 7 px inside "
                       "the outline, following the chamfers because it "
                       "follows the silhouette",
                       plaque().foreground(decorations::border(
                           kWidth, Fill::color(sheet.palette.figure), kInset))),
                  cell("Border::Mode::Bracket",
                       "only within 18 px of each corner · the "
                       "four L's, landing on the chamfers with no "
                       "further instruction",
                       plaque().foreground(bracket)),
                  cell("Border::Mode::Gapped",
                       "everything EXCEPT within 18 px · the open "
                       "corner, which is the complement of the one "
                       "above",
                       plaque().foreground(
                           Border{.width = kWidth,
                                  .fill = Fill::color(sheet.palette.figure),
                                  .inset = kInset,
                                  .mode = Border::Mode::Gapped,
                                  .corner = kArm})),
                  cell("doubleBorder(weighted, rule)",
                       "two rules as ONE style value · the outer "
                       "thickens near each turn, the inner is the same "
                       "value at another inset",
                       plaque().style(decorations::doubleBorder(
                           decorations::weightedCorners(
                               kWidth, kWidth * 3,
                               Fill::color(sheet.palette.figure), kArm, kInset),
                           decorations::border(0.9f, Fill::color(kCool), 14)))),
                  cell("weave(braid(3), alternate())",
                       "three waves at phases k/3 around the same "
                       "outline, with the rule saying who passes over "
                       "whom where they meet",
                       plaque().stroke(Decoration(brush::weave(
                           kit::braid(kStrands, kAmplitude, kWavelength,
                                      Decoration(brush::solid(
                                          2.0f,
                                          Fill::color(sheet.palette.figure)))),
                           crossing::alternate())))),
                  cell("Bracket on a CIRCLE",
                       "a curve has no tangent break, so the corner scan "
                       "finds nothing and the brackets vanish entirely "
                       "· correct, and surprising",
                       plaque(true).foreground(bracket))},
             .gap = 10})));
  }
};

SIGIL_SKETCH(BorderWeave, "Kit · API",
             "one plaque under each of Border's four modes and a woven "
             "braid, and the same brackets on a circle, where the corner "
             "scan finds no corner at all")
