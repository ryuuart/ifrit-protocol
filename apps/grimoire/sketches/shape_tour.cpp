/** @file
 * shape_tour — THE SILHOUETTE SHELF: every `shapes::` generator and
 * operator, once, at two sizes.
 *
 * A node's outline is a VALUE. `Element::shape()` takes anything with
 * `path(SkSize)` and an `operator==`, and the whole shelf below is such
 * values — parameters and nothing else, so a shaped node prunes while its
 * generator and its box are unchanged. A raw callable cannot compare, so
 * a node handed one repaints for ever; that is the reason the shelf
 * exists rather than a folder of lambdas.
 *
 * EVERY CELL IS DRAWN TWICE, at 96 px and at 44 px, from ONE value. A
 * generator is written in the box's own coordinates, so the small copy is
 * not the large one scaled: it is the same construction inscribed in a
 * smaller box. Where the two disagree — a corner radius that is a fixed
 * number of pixels, a chamfer that eats a small box whole — the pair says
 * so, and that is exactly the question a shelf like this is read for.
 *
 * The rows run from generators to operators:
 *   GENERATORS  svg, polygon, star, circle, annulus, squircle, blob, arc,
 *               sector, parallelogram, arrow — closed figures inscribed in
 *               the box, and the open arc, which has no inside and must be
 *               stroked. A traced `svg` path maps its own bounds onto the
 *               box: corner to corner, or fitted with its proportions
 *               kept, which a wide box shows as one pair.
 *   OPERATORS   rounded, chamfered, notched — generators OVER generators.
 *               `rounded(star(5), 8)` is a five-point star with
 *               consistently rounded points, which is what a box-corner
 *               radius cannot do for a silhouette that has no box corners,
 *               and the wrapper is comparable whenever what it wraps is.
 *               A chamfer and a notch take a per-corner MASK, because a
 *               treatment on two corners and square on the other two is
 *               the common case and no radius expresses it.
 *
 * The parametric curves, which are sampled rather than inscribed, have a
 * shelf of their own.
 *
 * EDIT THESE FIRST
 *   kLarge / kSmall — the two boxes every generator is inscribed in.
 *   any generator's parameters — the cell's caption is the call, so an
 *                edited number and its picture stay together.
 */

// TAGS: Drawing/Primitives

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
using Corner = sigil::geometry::shapes::Corner;

using namespace sigil::compose;

namespace {

constexpr float kLarge = 96;  // the big box every generator is drawn in
constexpr float kSmall = 44;  // …and the small one, from the same value
constexpr float kCell = 168;  // one cell's width
constexpr float kBed = 118;   // the drawn strip's height

constexpr material::Color kBody{0.90f, 0.55f, 0.37f, 1};
constexpr material::Color kLine{0.56f, 0.76f, 0.88f, 1};

/** ONE CELL: the generator drawn at both sizes on one baseline, then the
 *  call that made it and what it is. `closed` decides whether the figure
 *  is filled — an open path has no inside, and filling one closes it
 *  across the chord. */
template <class Outline>
Element cell(Outline outline, const char* call, const char* note,
             bool closed = true) {
  // Both copies stand on ONE baseline, so the small one is a construction
  // in a smaller box rather than a scaled picture of the large one.
  const auto figure = [outline, closed](float side, float left) {
    return kit::at(box()
                       .shape(outline)
                       .fill(closed ? Fill::color(kBody) : Fill::none())
                       .stroke(stroke(1.4f, Fill::color(kLine))),
                   left, kBed - 8 - side, side, side);
  };
  return sketch::kit::caption(
      kCell, call, note,
      box()
          .width(kCell)
          .height(kBed)
          .fill(var("palette-cell-ground"))
          .children({figure(kLarge, 8), figure(kSmall, kLarge + 22)}),
      sigil::compose::kit::Caption::Where::Below);
}

/** ONE WIDE CELL: the traced path in a box far wider than its own
 *  bounds, keylined so the box and the figure read apart — mapped corner
 *  to corner, or fitted with its proportions kept. */
Element wideCell(const char* d, bool preserveAspect, const char* call,
                 const char* note) {
  return sketch::kit::caption(
      kCell, call, note,
      box()
          .width(kCell)
          .height(kBed)
          .fill(var("palette-cell-ground"))
          .children(
              {kit::at(box()
                           .stroke(stroke(1.0f, Fill::color(kLine)))
                           .children({box()
                                          .inset(0)
                                          .shape(shapes::svg(d, preserveAspect))
                                          .fill(Fill::color(kBody))}),
                       8, kBed - 8 - 60, kCell - 16, 60)}),
      sigil::compose::kit::Caption::Where::Below);
}

Element row(std::vector<Element> cells) {
  return kit::cells({.cells = std::move(cells), .gap = 12});
}

/** A heart, traced in any vector tool and pasted in as its `d` — the
 *  door every silhouette that was drawn rather than derived comes
 *  through. Parsed once; the parsed path is the comparable value. */
const char* kHeartD =
    "M50 88 C 18 62 4 44 4 28 C 4 12 16 4 28 4 C 38 4 46 10 50 18 "
    "C 54 10 62 4 72 4 C 84 4 96 12 96 28 C 96 44 82 62 50 88 Z";

}  // namespace

struct ShapeShelf {
  void setup(sketch::Scene2DContext& ctx) {
    // A generator is a pure function of its parameters and the box.
    sketch::kit::stage(ctx,
                       {.styleSheet = sketch::kit::featureStyleSheet(
                                          sketch::kit::Density::Spacious) +
                                      rule(":root")
                                          .var("spacing-margin-horizontal", 30)
                                          .var("spacing-margin-top", 22)
                                          .var("spacing-caption-note-gap", 3),
                        .size = {1120, 980},
                        .captureAt = 0.05});

    Element generators = kit::cells(
        {.cells =
             {row({cell(shapes::svg(kHeartD, true), "svg(d, preserveAspect)",
                        "an SVG path-d, parsed once; the bounds map onto "
                        "the box"),
                   cell(shapes::polygon(6, 15), "polygon(6, 15°)",
                        "regular N-gon inscribed in the box, first vertex "
                        "up unless rotated"),
                   cell(shapes::star(5, 0.42f), "star(5, 0.42)",
                        "N points, inner radius as a ratio"),
                   cell(shapes::star(9, 0.62f, 0.35f),
                        "star(9, 0.62, waist 0.35)",
                        "a positive waist engraves the arms; a negative "
                        "one bulges them"),
                   cell(shapes::circle(), "circle()",
                        "the inscribed ellipse — exact conics, "
                        "not a sampled polyline"),
                   cell(shapes::circle(10.0f), "circle(inset 10)",
                        "pulled inside the box by a stated number of px")}),
              row({cell(shapes::annulus(0.55f), "annulus(0.55)",
                        "even-odd, so it fills as a ring"),
                   cell(shapes::squircle(4.0f), "squircle(4)",
                        "|x|^e + |y|^e = 1 · 4 is the app-icon "
                        "softness"),
                   cell(shapes::squircle(12.0f), "squircle(12)",
                        "large exponents approach the rectangle"),
                   cell(shapes::blob(7, 0.22f, 9), "blob(7, 0.22, 9)",
                        "seeded lobes on a Catmull-Rom loop — "
                        "chaos you can cache"),
                   cell(shapes::arc(200, 250), "arc(200°, 250°)",
                        "OPEN: it begins at its own start, so an "
                        "arc-length reveal needs no wrap arithmetic",
                        false),
                   cell(shapes::sector(200, 250, 0.45f),
                        "sector(200°, 250°, inner 0.45)",
                        "the closed, fillable wedge — the "
                        "coxcomb's mark")}),
              row({cell(shapes::parallelogram(18), "parallelogram(18°)",
                        "the top edge shifts by h·tan(skew), staying "
                        "inside the box"),
                   cell(shapes::arrow(0.34f, 0.42f), "arrow(0.34, 0.42)",
                        "along +x: a shaft of the height, a head of the "
                        "width"),
                   wideCell(kHeartD, false, "svg(d) · wide box",
                            "corner to corner: the bounds stretch to fill "
                            "both edges"),
                   wideCell(kHeartD, true, "svg(d, true) · wide box",
                            "fitted: the spare width stays outside the "
                            "silhouette"),
                   cell(shapes::rounded(shapes::star(5, 0.42f), 8),
                        "rounded(star(5, 0.42), 8)",
                        "a shape OVER a shape: the radius is px, so the "
                        "small copy is rounder in proportion"),
                   cell(shapes::rounded(shapes::polygon(3), 12),
                        "rounded(polygon(3), 12)",
                        "what a box-corner radius cannot do for a figure "
                        "with no box corners")}),
              row({cell(shapes::chamfered(14), "chamfered(14)",
                        "the cut is px too — at 44 px it takes "
                        "most of the box"),
                   cell(shapes::chamfered(14, Corner::Diagonal),
                        "chamfered(14, Diagonal)",
                        "a corner mask: only the diagonal pair is cut, "
                        "which reads as a tab"),
                   cell(shapes::notched(26, 10), "notched(26, 10)",
                        "a notch per corner, width and depth in px"),
                   cell(shapes::notched(26, 10,
                                        Corner::TopLeft | Corner::TopRight),
                        "notched(…, TopLeft | TopRight)",
                        "the mask picks the top pair; the bottom stays "
                        "square"),
                   cell(shapes::rounded(shapes::notched(26, 10), 5),
                        "rounded(notched(26, 10), 5)",
                        "operators compose: the notch stays, its new "
                        "corners soften")})},
         .column = true,
         .gap = 16});

    ctx.composer.render(sketch::kit::page(
        {.title = "The silhouette shelf",
         .subtitle = "Every generator and operator, each drawn at 96 px and "
                     "44 px from the same parameters.",
         .footer = "Closed silhouettes are filled; the open arc is stroked. "
                   "Pixel-based corners change proportion at the smaller "
                   "size."},
        std::move(generators)));
  }
};

SIGIL_SKETCH(ShapeShelf, "Specimen",
             "the silhouette shelf — every shapes:: generator and operator "
             "in a labelled cell, each drawn twice from one value so a "
             "px-keyed parameter shows what it does to a small box, with a "
             "traced svg stretched and fitted in a wide box and the corner "
             "masks a chamfer and a notch take")
