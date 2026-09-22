/** @file
 * One SVG path in three matched viewport shapes.
 * Columns hold viewport dimensions constant; rows select corner-to-corner
 * mapping or aspect-preserving fit. The rule belongs to the requested box,
 * the yellow silhouette to the path. Parsing happens once when shapes::svg
 * constructs the comparable value.
 */

// TAGS: Geometry/Paths

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 860};
constexpr float kCell = 324;
constexpr float kPicture = 208;

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11, .mono = true};
  return look;
}

/** The traced outline: a lightning bolt, whose own bounds are taller
 *  than they are wide, so a wide box has to do something about it. */
constexpr const char* kBolt = "M62 4 L18 78 H44 L30 148 L86 62 H56 Z";

constexpr material::Color kBoxRule{0.26f, 0.28f, 0.33f, 1};
constexpr material::Color kFigure{0.98f, 0.80f, 0.34f, 1};

/** One specimen: the box the outline was asked to fill, keylined so the
 *  box and the figure are separately visible, with an ordinary node
 *  SHAPED by the svg value stretched over it. */
Element viewport(SkSize size, bool preserveAspect) {
  return sketch::kit::well(
      {.width = kCell,
       .height = kPicture,
       .content = sketch::kit::Well::Content{}},
      box()
          .width(size.width())
          .height(size.height())
          .stroke(stroke(1.0f, Fill::color(kBoxRule)))
          .children({box()
                         .flexGrow(1)
                         .alignSelf(Align::Stretch)
                         .shape(shapes::svg(kBolt, preserveAspect))
                         .fill(Fill::color(kFigure))}));
}

Element fitRow(bool preserveAspect) {
  return sketch::kit::comparison(
      {.cases =
           {{.title = "WIDE",
             .control = "270 × 96",
             .figure = viewport({270, 96}, preserveAspect),
             .note = preserveAspect
                         ? "The spare width stays outside the silhouette."
                         : "The silhouette widens to reach both edges."},
            {.title = "SQUARE",
             .control = "176 × 176",
             .figure = viewport({176, 176}, preserveAspect),
             .note = preserveAspect
                         ? "The bolt keeps the proportions of its path."
                         : "Equal box dimensions do not imply an undistorted "
                           "path."},
            {.title = "TALL",
             .control = "96 × 190",
             .figure = viewport({96, 190}, preserveAspect),
             .note =
                 preserveAspect
                     ? "A close aspect match leaves only a small margin."
                     : "A close aspect match makes the stretch less obvious."}},
       .measure = 1020,
       .gap = 24});
}

}  // namespace

struct SvgSilhouette {
  void setup(sketch::SketchContext& ctx) {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = "One outline, three viewports",
         .subtitle = "The grey rule is the requested box. The yellow shape "
                     "comes from one unchanged SVG path.",
         .footer = "The path is parsed once. preserveAspect changes how its "
                   "bounds map into a layout box."},
        box().column().gap(22).children(
            {sketch::kit::sectionHeader(
                 {.label = "STRETCH TO THE BOX", .note = "svg(d)"}),
             fitRow(false),
             sketch::kit::sectionHeader(
                 {.label = "KEEP THE PROPORTIONS", .note = "svg(d, true)"}),
             fitRow(true)})));
  }
};

SIGIL_SKETCH(SvgSilhouette, "Kit · API",
             "one traced d string in three boxes of different shape, mapped "
             "corner to corner and then fitted, so the one flag that "
             "decides between them is the only thing that moves")
