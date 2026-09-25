/** @file
 * paint_shelf — the paint leaves a plain gradient cannot spell, and the
 * two dials that decide what a paint's coordinates MEAN.
 *
 * A radial with a `.focus` is the offset-focus radial, and it is what a
 * moved radial is not: moving a radial's centre couples the falloff to the
 * displacement, so the whole ramp slides including its outer edge. Here
 * the outer circle stays put and only the hot spot moves, which is what
 * a highlight displaced off a sphere actually does.
 *
 * `sweep` runs from a start angle around the centre, and its angles
 * CLAMP rather than wrap: a window that leaves [0, 360) paints the part
 * outside it in the nearest stop's flat colour, because no canvas angle
 * ever reaches past 360.
 *
 * `buffer` is content that changes without re-describing: a
 * caller-owned raster the paint samples, published with `commit()`. The
 * recipe compares by (source, revision), so an identical re-describe
 * between commits prunes and nothing repaints — which is the whole
 * point, since the alternative gives up the node's picture caching and
 * its decorations.
 *
 * `worldSpace` moves the coordinates a paint is evaluated in from the
 * node's own box to the ROOT's. The bottom-right pair is one paint on
 * two nodes: unflagged each node gets its own copy of the ramp, flagged
 * both read one field that runs across the page.
 *
 * EDIT THESE FIRST
 *   kFocus  — how far the focused radial's hot spot is displaced, px.
 *   kWindow — the conic's start and end angles.
 */

// TAGS: Materials/Color

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <memory>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1220};
constexpr float kCell = 328;
constexpr float kPicture = 210;

constexpr float kFocus = 44;       // the focus's hot spot displacement, px
constexpr float kWindowFrom = 45;  // the sweep window that does not fill a turn
constexpr float kWindowTo = 315;

SkPoint middle() { return {kCell * 0.5f, kPicture * 0.5f}; }

/** The one ramp every radial cell runs, so what differs between them is
 *  the geometry of the falloff and never the colours. */
std::vector<material::ColorStop> ember() {
  return {{0.0f, {1.00f, 0.96f, 0.82f, 1}},
          {0.35f, {0.98f, 0.62f, 0.24f, 1}},
          {1.0f, {0.12f, 0.10f, 0.16f, 1}}};
}

/** A wheel of hues for the two sweeps, ending where it began so the seam
 *  at the start angle is the only edge in it. */
std::vector<material::ColorStop> wheel() {
  return {{0.00f, {0.94f, 0.34f, 0.32f, 1}},
          {0.25f, {0.94f, 0.82f, 0.32f, 1}},
          {0.50f, {0.36f, 0.86f, 0.56f, 1}},
          {0.75f, {0.40f, 0.60f, 0.96f, 1}},
          {1.00f, {0.94f, 0.34f, 0.32f, 1}}};
}

sketch::kit::ComparisonCase cell(const char* caseTitle, const char* call,
                                 const char* note, Element body) {
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well({.width = kCell, .height = kPicture},
                                      std::move(body)),
          .note = note};
}

/** One paint across the whole cell. */
sketch::kit::ComparisonCase swatch(const char* caseTitle, const char* call,
                                   const char* note, material::Paint fill) {
  return cell(caseTitle, call, note,
              box().children({box().cover().fill(std::move(fill))}));
}

}  // namespace

struct PaintShelf {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // The caller-owned raster: drawn once here and published. A running
    // sketch would draw into it and commit() again; the node's picture
    // caching survives either way.
    auto pixels = std::make_shared<material::skia::PixelBuffer>(120, 90);
    {
      SkCanvas& into = pixels->canvas();
      into.clear(SkColor4f{0.09f, 0.12f, 0.18f, 1}.toSkColor());
      SkPaint mark;
      mark.setAntiAlias(true);
      for (int i = 0; i < 9; ++i) {
        mark.setColor4f({0.30f + 0.07f * (float)i, 0.86f - 0.05f * (float)i,
                         0.92f - 0.03f * (float)i, 1});
        into.drawCircle(14.0f + 12.0f * (float)i,
                        22.0f + 46.0f * (i % 2 == 0 ? 0.0f : 1.0f),
                        6.0f + (float)i, mark);
      }
      pixels->commit();
    }

    // The one paint the last two cells share: a diagonal unit ramp, so
    // "the node's own box" and "the root's box" are two visibly
    // different readings of the same description.
    const auto field = [](bool world) {
      material::Paint p = material::Paint::linearGradient(
          {0, 0}, {1, 1},
          {{0.0f, {0.16f, 0.20f, 0.34f, 1}},
           {0.5f, {0.44f, 0.78f, 0.86f, 1}},
           {1.0f, {0.96f, 0.72f, 0.34f, 1}}});
      return p.worldSpace(world);
    };
    // TWO NODES, ONE DESCRIPTION — which is the whole of what worldSpace
    // is read by.
    const auto pair = [&](bool world) {
      return box().row().padding(34, 18).gap(16).children({each(2, [&](int) {
        return box().flexGrow(1).alignSelf(Align::Stretch).fill(field(world));
      })});
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "What do paint coordinates mean?",
         .subtitle =
             "A focus, an angular window, a raster source, and a shared field",
         .footer = "The examples keep the colour stops fixed while changing "
                   "the coordinate or source contract."},
        box().column().gap(26).children(
            {sketch::kit::section(
                 {.label = "MOVE THE FOCUS, KEEP THE EDGE",
                  .note =
                      "Read the outer circle as well as the bright centre."},
                 sketch::kit::comparison(
                     {.cases =
                          {swatch(
                               "RADIAL REFERENCE",
                               "Paint::radial(centre, 92, ember)",
                               "The hot spot and the outer circle share a "
                               "centre.",
                               material::Paint::radialGradient(
                                   middle(), 92, ember(),
                                   {.units = material::GradientUnits::Pixels})),
                           swatch(
                               "CONICAL · LEFT",
                               "conical(focus, 0, centre, 92, ember)",
                               "Move the focus while keeping the outer circle "
                               "fixed.",
                               material::Paint::radialGradient(
                                   middle(), 92, ember(),
                                   {.units = material::GradientUnits::Pixels,
                                    .focus = glm::vec2{middle().fX - kFocus,
                                                       middle().fY -
                                                           kFocus * 0.6f}})),
                           swatch(
                               "CONICAL · RIGHT",
                               "…"
                               "with the focus moved "
                               "across",
                               "Move the focus across the same fixed circle.",
                               material::Paint::radialGradient(
                                   middle(), 92, ember(),
                                   {.units = material::GradientUnits::Pixels,
                                    .focus = glm::vec2{middle().fX +
                                                           1.3f * kFocus,
                                                       middle().fY +
                                                           0.8f * kFocus}}))},
                      .measure = 1020,
                      .gap = 18})),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(18)
                 .children(
                     {sketch::kit::section(
                          {.label = "AN ANGULAR WINDOW", .note = ""},
                          sketch::kit::comparison(
                              {.cases =
                                   {swatch("FULL TURN",
                                           "Paint::sweep(centre, wheel)",
                                           "The colour ramp completes a "
                                           "full turn.",
                                           material::Paint::conicGradient(
                                               middle(), wheel(),
                                               {.units = material::
                                                    GradientUnits::Pixels})),
                                    swatch("CLAMPED WINDOW",
                                           "sweep(centre, wheel, 45, 315)",
                                           "Angles outside 45°–315° clamp "
                                           "to the nearest stop.",
                                           material::Paint::conicGradient(
                                               middle(), wheel(),
                                               {.units = material::
                                                    GradientUnits::Pixels,
                                                .startDegrees = kWindowFrom,
                                                .endDegrees = kWindowTo}))},
                               .measure = 674,
                               .gap = 18})),
                      sketch::kit::section(
                          {.label = "PUBLISHED PIXELS", .note = ""},
                          sketch::kit::comparison(
                              {.cases = {swatch(
                                   "RASTER BUFFER", "Paint::buffer(pixels)",
                                   "Caller-owned pixels, published by "
                                   "commit().",
                                   material::skia::buffer(
                                       pixels, material::Repeat::Repeat,
                                       material::Repeat::Repeat))},
                               .measure = 328,
                               .gap = 18}))}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(18)
                 .children(
                     {sketch::kit::section(
                          {.label = "WHO OWNS THE COORDINATES?", .note = ""},
                          sketch::kit::comparison(
                              {.cases = {cell("EACH NODE",
                                              "linearUnit(…"
                                              ").worldSpace(false)",
                                              "Each node repeats the whole "
                                              "ramp in its own box.",
                                              pair(false)),
                                         cell("THE ROOT",
                                              "linearUnit(…"
                                              ").worldSpace(true)",
                                              "Both nodes sample one field "
                                              "anchored to the page.",
                                              pair(true))},
                               .measure = 674,
                               .gap = 18})),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "ONE PAINT, TWO BOXES", .note = ""}),
                           document::caption(
                               "A local ramp starts again in each box. A "
                               "world-space ramp runs through the page, so "
                               "the boxes become windows onto different parts "
                               "of a single field.")
                               .width(328),
                           document::caption(
                               "The buffer is another kind of source: its "
                               "revision changes when the caller publishes "
                               "new pixels.")
                               .width(328)})})})));
  }
};

SIGIL_SKETCH(PaintShelf, "Specimen",
             "the offset-focus radial beside the plain one, the sweep and "
             "its clamped window, a caller-owned raster, and one ramp read "
             "in the node's box and then in the root's")
