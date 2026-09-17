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

constexpr SkSize kCanvas = {1100, 660};
constexpr float kCell = 252;
constexpr float kPicture = 190;

constexpr float kRadius = 22;      // the rounding radius, px
constexpr float kCut = 30;         // the chamfer, px
constexpr float kNotchWidth = 38;  // the notch's width, px
constexpr float kNotchDepth = 18;  // …and its depth

constexpr SkColor4f kPlate{0.20f, 0.22f, 0.27f, 1};
constexpr SkColor4f kEdge{0.92f, 0.84f, 0.66f, 1};

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::specimenTheme();
  look.type.captionLabel = {.size = 11, .mono = true};
  return look;
}

/** One specimen: the cut plate filled and keylined inside a cell, so the
 *  treatment reads both as a silhouette and as an edge. */
Element cell(const char* call, const char* note, Shape cut) {
  return sketch::kit::cell(
      {.plate = {.width = kCell, .height = kPicture, .clip = false}}, call,
      note,
      box()
          .inset(30, 22, 30, 22)
          .shape(std::move(cut))
          .fill(Fill::color(kPlate))
          .stroke(stroke(1.6f, Fill::color(kEdge))));
}

}  // namespace

struct CornerNotched {
  void setup(sketch::SketchContext& ctx) {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = "Corner treatments",
         .subtitle = "dials · the radius (22 px) · the "
                     "chamfer (30 px) · the notch (38 by 18 "
                     "px) · the mask",
         .footer = "every cell here is one value away from the "
                   "box at the top left — a radius, a "
                   "cut, a bite, or the mask that says which "
                   "corners take one"},
        kit::cells(
            {.cells =
                 {kit::cells(
                      {.cells =
                           {cell("parallelogram(0)",
                                 "no lean — the clean "
                                 "four-point box every cell below is "
                                 "one value away from",
                                 shapes::parallelogram(0)),
                            cell("rounded(parallelogram(0), 22)",
                                 "the WRAPPER: any silhouette in, every "
                                 "sharp corner of it rounded, the "
                                 "wrapped value still comparable",
                                 shapes::rounded(shapes::parallelogram(0),
                                                 kRadius)),
                            cell("chamfered(30)",
                                 "the 45° cut on all four "
                                 "— the corner that reads as "
                                 "machined metal",
                                 shapes::chamfered(kCut)),
                            cell("chamfered(30, Corner::Diagonal)",
                                 "top-left and bottom-right only "
                                 "— the asymmetric pair that "
                                 "reads as a tab",
                                 shapes::chamfered(kCut, Corner::Diagonal))},
                       .gap = 14}),
                  kit::cells(
                      {.cells =
                           {cell("notched(38, 18)",
                                 "the rectangular bite on all four "
                                 "— the stencil corner, the "
                                 "fixing lug",
                                 shapes::notched(kNotchWidth, kNotchDepth)),
                            cell("notched(38, 18, TopLeft|TopRight)",
                                 "the mask is a bit set, so any union "
                                 "of corners is a value · two "
                                 "lugs on the top edge",
                                 shapes::notched(
                                     kNotchWidth, kNotchDepth,
                                     Corner::TopLeft |
                                         Corner::TopRight)),
                            cell("rounded(star(6, 0.5), 10)",
                                 "the wrapper over a shape with NO box "
                                 "corners · twelve sharp turns, "
                                 "every one rounded the same",
                                 shapes::
                                     rounded(shapes::star(6, 0.5f), 10)),
                            cell("rounded(notched(38, 18), 7)",
                                 "a wrapper over a cut · the "
                                 "bites stay, and the eight corners "
                                 "they made soften",
                                 shapes::
                                     rounded(shapes::notched(kNotchWidth, kNotchDepth), 7))},
                       .gap =
                           14})},
             .column = true,
             .gap = 18})));
  }
};

SIGIL_SKETCH(CornerNotched, "Kit · API",
             "rounding as a wrapper over any silhouette, the chamfer and "
             "the notch as shapes a frame is cut to, and the per-corner "
             "mask both of them take")
