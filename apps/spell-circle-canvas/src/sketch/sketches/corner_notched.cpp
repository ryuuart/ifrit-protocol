/** @file
 * corner_notched — the three corner treatments, and the mask that says
 * which corners get one.
 *
 * Rounding is the only corner treatment the kernel offers, and it is a
 * WRAPPER: `rounded(inner, radius)` takes any silhouette and rounds
 * every sharp corner it has, box corners or not. It holds the wrapped
 * value rather than erasing it, so wrapping a generator gives a
 * generator that still compares by its parameters — and wrapping a bare
 * callable gives something that compares to nothing, which is the same
 * escape hatch the callable already was.
 *
 * The other two are shapes a frame is CUT to. A chamfer is the 45°
 * corner that reads as machined metal; a notch is the rectangular bite
 * that reads as a stencil or a fixing lug. Both take a per-corner MASK
 * rather than one number, because a treatment on two corners and square
 * on the other two is the common case and no radius expresses it — and
 * the two diagonals are named, since that pair is what reads as a tab.
 *
 * Both cuts are clamped: a chamfer to half the short side, a notch to
 * 0.45 of it, so an over-large value degenerates rather than turning the
 * path inside out.
 *
 * EDIT THESE FIRST
 *   kRadius — the rounding radius, px.
 *   kCut    — the chamfer, px.
 *   kNotch  — the notch's width and depth, px.
 */

// TAGS: Geometry/Paths

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using Corner = sigil::geometry::shapes::Corner;

namespace {

constexpr SkSize kCanvas = {1100, 820};
constexpr float kCell = 240;
constexpr float kPicture = 176;

constexpr float kRadius = 22;      // the rounding radius, px
constexpr float kCut = 30;         // the chamfer, px
constexpr float kNotchWidth = 38;  // the notch's width, px
constexpr float kNotchDepth = 18;  // …and its depth

constexpr SkColor4f kPlate{0.20f, 0.22f, 0.27f, 1};
constexpr SkColor4f kEdge{0.92f, 0.84f, 0.66f, 1};

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11, .mono = true};
  return look;
}

/** One specimen: the cut plate filled and keylined inside a cell, so the
 *  treatment reads both as a silhouette and as an edge. */
sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Shape cut) {
  return {.title = title,
          .control = call,
          .figure = sketch::kit::cell(
              {.plate = {.width = kCell, .height = kPicture, .clip = false}},
              "", "",
              box()
                  .inset(30, 22, 30, 22)
                  .shape(std::move(cut))
                  .fill(Fill::color(kPlate))
                  .stroke(stroke(1.6f, Fill::color(kEdge)))),
          .note = note};
}

}  // namespace

struct CornerNotched {
  void setup(sketch::SketchContext& ctx) {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = "Cut, select, round",
         .subtitle = "A shape operation changes the outline. A corner mask "
                     "chooses where it acts.",
         .footer = "Rounding wraps an existing outline; chamfers and notches "
                   "construct a new one."},
        box().column().gap(24).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  CHANGE THE OUTLINE",
                  .note = "One source · three operations"}),
             sketch::kit::comparison(
                 {.cases = {cell("REFERENCE", "parallelogram(0)",
                                 "The same rectangle starts every comparison.",
                                 shapes::parallelogram(0)),
                            cell("ROUND", "rounded(outline, 22)",
                                 "The outline stays comparable; each sharp "
                                 "turn receives a radius.",
                                 shapes::rounded(shapes::parallelogram(0),
                                                 kRadius)),
                            cell("CHAMFER", "chamfered(30)",
                                 "A straight cut replaces each corner.",
                                 shapes::chamfered(kCut)),
                            cell("NOTCH", "notched(38, 18)",
                                 "Each corner loses a rectangular bite.",
                                 shapes::notched(kNotchWidth, kNotchDepth))},
                  .measure = 1020,
                  .gap = 20}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {box().column().width(500).gap(20).children(
                          {sketch::kit::sectionHeader(
                               {.label = "02  CHOOSE THE CORNERS", .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {cell("DIAGONAL PAIR",
                                               "Corner::Diagonal",
                                               "Only the diagonal pair "
                                               "receives the cut.",
                                               shapes::chamfered(
                                                   kCut, Corner::Diagonal)),
                                          cell("TOP PAIR", "TopLeft | TopRight",
                                               "A corner mask selects just the "
                                               "top pair.",
                                               shapes::notched(
                                                   kNotchWidth, kNotchDepth,
                                                   Corner::TopLeft |
                                                       Corner::TopRight))},
                                .measure = 500,
                                .gap = 20})}),
                      box().column().width(500).gap(20).children(
                          {sketch::kit::sectionHeader(
                               {.label = "03  COMPOSE TREATMENTS", .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {cell("ROUND A STAR",
                                               "rounded(star, 10)",
                                               "Rounding also works on "
                                               "non-rectangular outlines.",
                                               shapes::rounded(
                                                   shapes::star(6, 0.5f), 10)),
                                          cell("ROUND A NOTCH",
                                               "rounded(notched, 7)",
                                               "The notch remains; its new "
                                               "corners soften.",
                                               shapes::rounded(
                                                   shapes::notched(kNotchWidth,
                                                                   kNotchDepth),
                                                   7))},
                                .measure = 500,
                                .gap = 20})})})})));
  }
};

SIGIL_SKETCH(CornerNotched, "Kit · API",
             "rounding as a wrapper over any silhouette, the chamfer and "
             "the notch as shapes a frame is cut to, and the per-corner "
             "mask both of them take")
