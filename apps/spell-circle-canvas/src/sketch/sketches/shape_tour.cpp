/** @file
 * shape_tour — THE SILHOUETTE SHELF: every `shapes::` generator, once,
 * at two sizes.
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
 * The rows are the four kinds the header files are split into:
 *   GENERATORS  svg, polygon, star, circle, annulus, squircle, blob, arc,
 *               sector, parallelogram, arrow — closed figures inscribed in
 *               the box, and the two open ones (arc, and the curves
 *               below) which have no inside and must be stroked.
 *   CURVES      lissajous, harmonograph, rose, spiral, trochoid — sampled
 *               parametrics normalised to fill the box.
 *   OPERATORS   rounded, chamfered, notched — generators OVER generators.
 *               `rounded(star(5), 8)` is a five-point star with
 *               consistently rounded points, which is what a box-corner
 *               radius cannot do for a silhouette that has no box corners,
 *               and the wrapper is comparable whenever what it wraps is.
 *
 * Closed figures are filled and outlined; the open ones are stroked only,
 * because an open path has no inside and a fill would close it across the
 * chord.
 *
 * EDIT THESE FIRST
 *   kLarge / kSmall — the two boxes every generator is inscribed in.
 *   any generator's parameters — the cell's caption is the call, so an
 *                edited number and its picture stay together.
 */

#include <include/core/SkPaint.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr float kLarge = 96;   // the big box every generator is drawn in
constexpr float kSmall = 44;   // …and the small one, from the same value
constexpr float kCell = 168;   // one cell's width
constexpr float kBed = 118;    // the drawn strip's height

constexpr SkColor4f kGround{0.945f, 0.937f, 0.918f, 1};
constexpr SkColor4f kBedTone{0.902f, 0.890f, 0.863f, 1};
constexpr SkColor4f kInk{0.114f, 0.106f, 0.098f, 1};
constexpr SkColor4f kAsh{0.376f, 0.365f, 0.345f, 1};
constexpr SkColor4f kRule{0.749f, 0.733f, 0.706f, 1};
constexpr SkColor4f kBody{0.827f, 0.318f, 0.220f, 1};
constexpr SkColor4f kLine{0.129f, 0.298f, 0.451f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Below,
          .label = label(11, kInk, 0.4f),
          .note = label(9.5f, kAsh, 0.2f),
          .gap = 7,
          .noteGap = 3,
          .noteMeasure = kCell};
}

/** ONE CELL: the generator drawn at both sizes on one baseline, then the
 *  call that made it and what it is. `closed` decides whether the figure
 *  is filled — an open path has no inside, and filling one closes it
 *  across the chord. */
template <class Shape>
Element cell(Shape shape, const char* call, const char* note,
             bool closed = true) {
  return kit::cell(
      voice(), toU8(call), toU8(note),
      custom([shape, closed](SkCanvas& canvas, const PaintContext& paint) {
        SkPaint fill;
        fill.setAntiAlias(true);
        fill.setColor4f(closed ? kBody : SkColor4f{0, 0, 0, 0});
        SkPaint line;
        line.setAntiAlias(true);
        line.setStyle(SkPaint::kStroke_Style);
        line.setStrokeWidth(1.4f);
        line.setColor4f(kLine);

        const float baseline = paint.size.height() - 8;
        const auto draw = [&](float side, float left) {
          const SkPath path = shape.path({side, side}).makeTransform(
              SkMatrix::Translate(left, baseline - side));
          if (closed) canvas.drawPath(path, fill);
          canvas.drawPath(path, line);
        };
        draw(kLarge, 8);
        draw(kSmall, kLarge + 22);
      })
          .width(kCell)
          .height(kBed)
          .fill(Fill::color(kBedTone)));
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

struct ShapeShelf final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1120, 840);
    ctx.background(kGround);
    // A generator is a pure function of its parameters and the box.
    ctx.captureAt(0.05);

    Element generators = kit::cells(
        {.cells =
             {row({cell(shapes::svg(kHeartD, true), "svg(d, preserveAspect)",
                        "an SVG path-d, parsed once; the bounds map onto "
                        "the box"),
                   cell(shapes::polygon(6, 15), "polygon(6, 15\xc2\xb0)",
                        "regular N-gon inscribed in the box, first vertex "
                        "up unless rotated"),
                   cell(shapes::star(5, 0.42f), "star(5, 0.42)",
                        "N points, inner radius as a ratio"),
                   cell(shapes::star(9, 0.62f, 0.35f),
                        "star(9, 0.62, waist 0.35)",
                        "a positive waist engraves the arms; a negative "
                        "one bulges them"),
                   cell(shapes::circle(), "circle()",
                        "the inscribed ellipse \xe2\x80\x94 exact conics, "
                        "not a sampled polyline"),
                   cell(shapes::circle(10.0f), "circle(inset 10)",
                        "pulled inside the box by a stated number of px")}),
              row({cell(shapes::annulus(0.55f), "annulus(0.55)",
                        "even-odd, so it fills as a ring"),
                   cell(shapes::squircle(4.0f), "squircle(4)",
                        "|x|^e + |y|^e = 1 \xc2\xb7 4 is the app-icon "
                        "softness"),
                   cell(shapes::squircle(12.0f), "squircle(12)",
                        "large exponents approach the rectangle"),
                   cell(shapes::blob(7, 0.22f, 9), "blob(7, 0.22, 9)",
                        "seeded lobes on a Catmull-Rom loop \xe2\x80\x94 "
                        "chaos you can cache"),
                   cell(shapes::arc(200, 250), "arc(200\xc2\xb0, 250\xc2\xb0)",
                        "OPEN: it begins at its own start, so an "
                        "arc-length reveal needs no wrap arithmetic",
                        false),
                   cell(shapes::sector(200, 250, 0.45f),
                        "sector(200\xc2\xb0, 250\xc2\xb0, inner 0.45)",
                        "the closed, fillable wedge \xe2\x80\x94 the "
                        "coxcomb's mark")}),
              row({cell(shapes::parallelogram(18), "parallelogram(18\xc2\xb0)",
                        "the top edge shifts by h\xc2\xb7tan(skew), staying "
                        "inside the box"),
                   cell(shapes::arrow(0.34f, 0.42f), "arrow(0.34, 0.42)",
                        "along +x: a shaft of the height, a head of the "
                        "width"),
                   cell(shapes::lissajous(3, 2, 90), "lissajous(3, 2, 90)",
                        "a sampled parametric, normalised to fill the box",
                        false),
                   cell(shapes::harmonograph(3, 4, 60),
                        "harmonograph(3, 4, 60)",
                        "the damped pair \xe2\x80\x94 the figure decays "
                        "inward", false),
                   cell(shapes::rose(5, 1), "rose(5)",
                        "r = cos(k\xce\xb8): odd k gives k petals, even k "
                        "gives 2k", false),
                   cell(shapes::spiral(3.5f), "spiral(3.5)",
                        "Archimedean by default; logarithmic on request",
                        false)}),
              row({cell(shapes::trochoid(5, 3, 5), "trochoid(5, 3, 5)",
                        "the spirograph pair \xe2\x80\x94 a circle rolling "
                        "outside another", false),
                   cell(shapes::trochoid(5, 3, 5, true),
                        "trochoid(5, 3, 5, inside)",
                        "\xe2\x80\xa6" "and rolling inside it", false),
                   cell(shapes::rounded(shapes::star(5, 0.42f), 8),
                        "rounded(star(5, 0.42), 8)",
                        "a shape OVER a shape: the radius is px, so the "
                        "small copy is rounder in proportion"),
                   cell(shapes::rounded(shapes::polygon(3), 12),
                        "rounded(polygon(3), 12)",
                        "what a box-corner radius cannot do for a figure "
                        "with no box corners"),
                   cell(shapes::chamfered(14), "chamfered(14)",
                        "the cut is px too \xe2\x80\x94 at 44 px it takes "
                        "most of the box"),
                   cell(shapes::notched(26, 10), "notched(26, 10)",
                        "a notch per corner, width and depth in px")})},
         .column = true,
         .gap = 16});

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("THE SILHOUETTE SHELF \xc2\xb7 every shapes:: "
                           "generator, at two sizes"),
             .subtitle = toU8("one comparable VALUE per cell, drawn at 96 px "
                              "and at 44 px from the same parameters "
                              "\xe2\x80\x94 a generator is written in the "
                              "box's coordinates, so the small copy is a "
                              "construction and not a scaling"),
             .footer = toU8("closed figures are filled and outlined; the "
                            "open ones are stroked only, since an open path "
                            "has no inside \xc2\xb7 anything with "
                            "path(SkSize) and operator== belongs on this "
                            "shelf"),
             .titleStyle = label(15, kInk, 2.0f),
             .subtitleStyle = label(11, kAsh, 0.5f),
             .footerStyle = label(10, kAsh, 0.2f),
             .marginX = 30,
             .marginTop = 22,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            std::move(generators))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(ShapeShelf, "Specimen",
             "the silhouette shelf \xe2\x80\x94 every shapes:: generator, "
             "curve and operator in a labelled cell, each drawn twice from "
             "one value so a px-keyed parameter shows what it does to a "
             "small box")
