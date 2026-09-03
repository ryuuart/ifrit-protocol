/** @file
 * svg_silhouette — a traced outline arrives as a string, and the one
 * flag that decides what happens to it in a box that is the wrong shape.
 *
 * `shapes::svg(d)` parses an SVG path-d with Skia's own parser: trace a
 * reference silhouette in any vector tool, paste the `d`, done. The
 * parse happens ONCE, at the call, and what the value then holds is the
 * parsed `SkPath` — which compares — so a node shaped by an svg() prunes
 * exactly like one shaped by a polygon.
 *
 * What the flag decides is how the path's own bounds map onto the node's
 * box. By default they map corner to corner, which STRETCHES: the same
 * `d` in a wide box and a tall one is two different figures.
 * `preserveAspect` fits and centres instead, so the figure keeps its
 * proportions and gives back the slack on the long axis.
 *
 * Both rows below are the same string in the same three boxes. Nothing
 * else on this sheet changes.
 *
 * EDIT THESE FIRST
 *   kBolt   — the `d` string. Anything SkParsePath reads.
 *   kBoxes  — the three box shapes it is asked to fill.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 684};
constexpr float kCell = 341;
constexpr float kPicture = 212;

/** The traced outline: a lightning bolt, whose own bounds are taller
 *  than they are wide, so a wide box has to do something about it. */
constexpr const char* kBolt =
    "M62 4 L18 78 H44 L30 148 L86 62 H56 Z";

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kBoxRule{0.26f, 0.28f, 0.33f, 1};
constexpr SkColor4f kFigure{0.98f, 0.80f, 0.34f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(11, kInk),
          .note = label(10.5f, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

/** One specimen: the box the outline was asked to fill, keylined so the
 *  box and the figure are separately visible, with an ordinary node
 *  SHAPED by the svg value stretched over it. */
Element cell(const char* call, const char* note, SkSize boxSize,
             bool preserveAspect) {
  return kit::cell(
      voice(), toU8(call), toU8(note),
      box()
          .width(Dim(kCell))
          .height(Dim(kPicture))
          .fill(Fill::color(kCellGround))
          .alignItems(Align::Center)
          .justify(Justify::Center)
          .child(box()
                     .width(Dim(boxSize.width()))
                     .height(Dim(boxSize.height()))
                     .stroke(stroke(1.0f, Fill::color(kBoxRule)))
                     .child(box()
                                .grow(1)
                                .alignSelf(Align::Stretch)
                                .shape(shapes::svg(kBolt, preserveAspect))
                                .fill(Fill::color(kFigure)))));
}

}  // namespace

struct SvgSilhouette final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("SVG SILHOUETTE \xc2\xb7 shapes::svg(d, "
                           "preserveAspect)"),
             .subtitle = toU8("dials \xc2\xb7 the d string (\"M62 4 L18 78 "
                              "H44 L30 148 L86 62 H56 Z\") \xc2\xb7 the fit "
                              "\xc2\xb7 the box it is asked to fill"),
             .footer = toU8("the parse happens once, at the call, and what "
                            "the value holds afterwards is the parsed path "
                            "\xe2\x80\x94 which compares, so a node shaped "
                            "by an svg() prunes like any other"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {kit::cells(
                          {.cells =
                               {cell("svg(d) in 270 \xc3\x97 96",
                                     "the default maps the path's own "
                                     "bounds corner to corner, so a wide "
                                     "box flattens the figure",
                                     {270, 96}, false),
                                cell("svg(d) in 176 \xc3\x97 176",
                                     "square: the bolt is taller than it is "
                                     "wide, so it is still stretched "
                                     "sideways here",
                                     {176, 176}, false),
                                cell("svg(d) in 96 \xc3\x97 190",
                                     "a tall box is nearly the path's own "
                                     "aspect, which is why this one looks "
                                     "right by accident",
                                     {96, 190}, false)},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {cell("svg(d, true) in 270 \xc3\x97 96",
                                     "preserveAspect fits and CENTRES "
                                     "instead \xc2\xb7 the slack goes to "
                                     "the long axis, not to the figure",
                                     {270, 96}, true),
                                cell("svg(d, true) in 176 \xc3\x97 176",
                                     "the same proportions in a square box "
                                     "\xc2\xb7 one figure, three boxes, no "
                                     "second d string",
                                     {176, 176}, true),
                                cell("svg(d, true) in 96 \xc3\x97 190",
                                     "and where the box already matched, "
                                     "the flag changes almost nothing "
                                     "\xe2\x80\x94 which is the tell",
                                     {96, 190}, true)},
                           .gap = 14})},
                 .column = true,
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(SvgSilhouette, "Kit \xc2\xb7 API",
             "one traced d string in three boxes of different shape, mapped "
             "corner to corner and then fitted, so the one flag that "
             "decides between them is the only thing that moves")
