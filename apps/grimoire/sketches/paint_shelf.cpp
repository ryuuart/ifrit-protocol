/** @file
 * paint_shelf — the paint leaves a plain gradient cannot spell: a focus
 * off the centre, an angular window, and pixels the caller publishes.
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
 * EDIT THESE FIRST
 *   kFocus  — how far the focused radial's hot spot is displaced, px.
 *   kWindow — the conic's start and end angles.
 */

// TAGS: Materials/Color

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <memory>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 900};
constexpr float kCell = 328;
constexpr float kPicture = 210;

constexpr float kFocus = 44;  // the focus's hot spot displacement, px
constexpr float kWindowFrom =
    135;  // the sweep window that does not fill a turn
constexpr float kWindowTo = 405;

glm::vec2 middle() { return {kCell * 0.5f, kPicture * 0.5f}; }

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
                                   const char* note, material::Material fill) {
  return cell(caseTitle, call, note,
              box().children({box().cover().fill(std::move(fill))}));
}

}  // namespace

struct PaintShelf {
  void setup(sketch::Scene2DContext& ctx) {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.styleSheet = sketch::kit::featureStyleSheet(
                                 sketch::kit::Density::Spacious),
                             .size = kCanvas,
                             .captureAt = 0.05});

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

    ctx.composer.render(sketch::kit::page(
        {.title = "Paints a plain gradient cannot spell",
         .subtitle = "A focus, an angular window and a raster source",
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
                               sigil::material::radialGradient(
                                   middle(), 92, ember(),
                                   {.units = material::GradientUnits::Pixels})),
                           swatch(
                               "CONICAL · LEFT",
                               "conical(focus, 0, centre, 92, ember)",
                               "Move the focus while keeping the outer circle "
                               "fixed.",
                               sigil::material::radialGradient(
                                   middle(), 92, ember(),
                                   {.units = material::GradientUnits::Pixels,
                                    .focus = glm::vec2{middle().x - kFocus,
                                                       middle().y -
                                                           kFocus * 0.6f}})),
                           swatch(
                               "CONICAL · RIGHT",
                               "…"
                               "with the focus moved "
                               "across",
                               "Move the focus across the same fixed circle.",
                               sigil::material::radialGradient(
                                   middle(), 92, ember(),
                                   {.units = material::GradientUnits::Pixels,
                                    .focus =
                                        glm::vec2{
                                            middle().x + 1.3f * kFocus,
                                            middle().y + 0.8f * kFocus}}))},
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
                              {.cases = {swatch("FULL TURN",
                                                "Paint::sweep(centre, wheel)",
                                                "The colour ramp completes a "
                                                "full turn.",
                                                sigil::material::conicGradient(
                                                    middle(), wheel(),
                                                    {.units = material::
                                                         GradientUnits::Pixels,
                                                     .startDegrees = 90,
                                                     .endDegrees = 450})),
                                         swatch(
                                             "CLAMPED WINDOW",
                                             "sweep(centre, wheel, 45, 315)",
                                             "Angles outside 45°–315° clamp "
                                             "to the nearest stop.",
                                             sigil::material::conicGradient(
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
                                   material::skia::base(material::skia::buffer(
                                       pixels, material::Repeat::Repeat,
                                       material::Repeat::Repeat)))},
                               .measure = 328,
                               .gap = 18}))}),
             document::caption(
                 "The buffer's revision changes when the caller publishes "
                 "new pixels; an identical description between commits "
                 "repaints nothing.")})));
  }
};

SIGIL_SKETCH(PaintShelf, "Specimen",
             "the offset-focus radial beside the plain one, the sweep and "
             "its clamped window, and a caller-owned raster published by "
             "commit()")
