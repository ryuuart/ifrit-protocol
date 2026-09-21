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
#include <sigilcompose/kit/Document.h>
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

constexpr SkSize kCanvas = {1100, 840};
constexpr float kCell = 240;
constexpr float kPicture = 190;
constexpr float kPlaque = 148;

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

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Element body) {
  return {.title = title,
          .control = call,
          .figure = sketch::kit::cell(kSpecimen, "", "",
                                      std::move(body).absolute().inset((kPicture - kPlaque) / 2, (kCell - kPlaque) / 2)),
          .note = note};
}

}  // namespace

struct BorderWeave {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    const sketch::kit::Theme& sheet = sketch::kit::theme();

    const Border bracket{.width = kWidth,
                         .fill = Fill::color(sheet.palette.figure),
                         .inset = kInset,
                         .mode = Border::Mode::Bracket,
                         .corner = kArm};

    ctx.composer.render(sketch::kit::page(
        {.title = "An edge, dressed six ways",
         .subtitle = "First change where a rule appears. Then test what "
                     "happens when strands cross—or when no corner exists.",
         .footer = "The decoration follows the silhouette; a chamfer or "
                   "another outline does not require a new layout."},
        box().column().gap(24).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  FOLLOW THE SILHOUETTE",
                  .note = "The same inset and weight in each mode"}),
             sketch::kit::comparison(
                 {.cases = {cell("CONTINUOUS", "width = 1.8 · inset = 7",
                                 "A continuous rule follows every chamfer.",
                                 plaque().foreground(decorations::border(
                                     kWidth, Fill::color(sheet.palette.figure),
                                     kInset))),
                            cell("CORNERS ONLY", "mode = Bracket · arm = 18",
                                 "Keep the first and last 18 px around each "
                                 "detected turn.",
                                 plaque().foreground(bracket)),
                            cell("BETWEEN CORNERS", "mode = Gapped · arm = 18",
                                 "Remove those same corner intervals.",
                                 plaque().foreground(Border{
                                     .width = kWidth,
                                     .fill = Fill::color(sheet.palette.figure),
                                     .inset = kInset,
                                     .mode = Border::Mode::Gapped,
                                     .corner = kArm})),
                            cell("TWO RULES", "weightedCorners + border",
                                 "Weight the turns, then add a second inset "
                                 "rule.",
                                 plaque().layerStyle(decorations::doubleBorder(
                                     decorations::weightedCorners(
                                         kWidth, kWidth * 3,
                                         Fill::color(sheet.palette.figure),
                                         kArm, kInset),
                                     decorations::border(
                                         0.9f, Fill::color(kCool), 14))))},
                  .measure = 1020,
                  .gap = 20}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(24)
                 .children(
                     {box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "02  STRANDS MUST CROSS", .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {cell(
                                    "WOVEN BORDER",
                                    "braid(3, 5, 34) · alternate",
                                    "Three phased waves trade sides; "
                                    "alternating crossings make the braid.",
                                    plaque().stroke(Decoration(brush::weave(
                                        kit::braid(
                                            kStrands, kAmplitude, kWavelength,
                                            Decoration(brush::solid(
                                                2.0f,
                                                Fill::color(
                                                    sheet.palette.figure)))),
                                        crossing::alternate()))))},
                                .measure = 240,
                                .gap = 18})}),
                      box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "03  A CURVE WITHOUT CORNERS",
                                .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {cell(
                                    "THE CIRCLE COUNTEREXAMPLE",
                                    "Bracket · circular outline",
                                    "No tangent break means no detected "
                                    "corner, so brackets disappear.",
                                    plaque(true).foreground(bracket))},
                                .measure = 240,
                                .gap = 18})}),
                      box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "READING THE EDGE", .note = ""}),
                           document::caption(
                               "The filled plaque is the source silhouette. "
                               "The light line is a decoration following that "
                               "outline.")
                               .width(360),
                           document::caption(
                               "Corner modes operate on intervals of the "
                               "contour. They need no separately positioned "
                               "corner elements.")
                               .width(360),
                           document::caption(
                               "Parallel rails never braid. The strands must "
                               "exchange sides for a crossing rule to matter.")
                               .width(360),
                           document::caption(
                               "The corner rule scans tangent breaks, not "
                               "the number of vertices describing the path.")
                               .width(360)})})})));
  }
};

SIGIL_SKETCH(BorderWeave, "Kit · API",
             "one plaque under each of Border's four modes and a woven "
             "braid, and the same brackets on a circle, where the corner "
             "scan finds no corner at all")
