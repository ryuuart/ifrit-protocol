/** @file
 * text_on_path — a run of type set on a curve, and every option that
 * decides where it sits.
 *
 * `textOnPath` makes a path the BASELINE of a text leaf: the run is shaped
 * once, broken across the path's contours, and each glyph stood on the
 * curve and turned to it. The path resolves against the TEXT NODE'S OWN
 * box, so the leaf carries the plate's size; the faint guide under each
 * run is the same generator in the same box.
 *
 * WHERE ALONG IT. `at` is a fraction of the path's length and `align`
 * says what it pins — the run's start, its middle or its end — so one
 * number places a run of any length. On a closed path the fraction wraps.
 *
 * OFF THE BASELINE. `offset` rides the type off the curve, positive to the
 * left of travel, which on a clockwise ring is outward. A ring's lower
 * half reads upside down by the engraver's convention; `autoFlip` turns
 * those glyphs over. `orient` stands the glyphs along the radius instead,
 * the way an astrolabe's limb is lettered.
 *
 * OPEN PATHS AND FIT. An open path is walked end to end and a glyph
 * pushed past either end is dropped. A `fit` resolves the run against the
 * baseline's length at layout: `Size` scales the type until it spans the
 * ring and closes on its own seam, `Spacing` keeps the size and stretches
 * the advances instead.
 *
 * EXACT TANGENT. By default a glyph's turn is snapped to a ladder of
 * angles, because each distinct rotation is a distinct glyph-atlas strike;
 * `exactTangent` lifts the ladder, for large static lettering where the
 * steps would show. The two are drawn once each as textures and held
 * against each other with `media::difference`, which says where they part
 * most, and that spot is shown magnified from both.
 *
 * EDIT THESE FIRST
 *   kRing — the side of the square every ring is inscribed in, px.
 *   kLabel, kDisplay — the two type sizes.
 *   kMagnification — how far the compared spot is enlarged.
 */

// TAGS: Typography/Lettering

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Curves.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/difference/Difference.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <string>
#include <utility>

namespace material = sigil::material;
namespace media = sigil::media;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1560};
constexpr float kCell = 240;     // one plate's width
constexpr float kPicture = 200;  // …and its height
constexpr float kRing = 160;     // the square every ring is inscribed in

constexpr float kLabel = 15;    // the running size
constexpr float kDisplay = 64;  // the display size
constexpr float kDetail = 260;  // the compared letter's size
constexpr SkISize kRaster = {328, 420};
constexpr SkISize kCrop = {26, 20};
constexpr float kMagnification = 8;

/** THE INSCRIPTION'S VOICE, as a class beside the page's registers: the
 *  old-style serif, untracked — stated, because the page's running
 *  register is tracked and an inscription is not. */
StyleSheet voices() {
  return sketch::kit::theme().styleSheet() +
         StyleSheet{rule(".inscription")
                        .font({.face = weave::ports::face({"Iowan Old Style",
                                                           "Georgia",
                                                           "Times New Roman",
                                                           "serif"}),
                               .size = kLabel,
                               .track = 0.0f})};
}

/** A run on @p baseline, filling the ring's square. */
Element run(const char* words, TextPath baseline, float size = kLabel) {
  return text(words)
      .styleClass("inscription")
      .font({.size = size, .color = sketch::kit::theme().palette.figure})
      .textOnPath(std::move(baseline));
}

/** ONE PLATE: the guide — the same generator, faint, in the same square
 *  box — under the run set on it. */
Element plate(Shape guide, Element lettering) {
  const float down = (kPicture - kRing) / 2;
  const float across = (kCell - kRing) / 2;
  return sketch::kit::well({.width = kCell, .height = kPicture})
      .children(
          {box()
               .inset(down, across)
               .shape(guide)
               .stroke(stroke(1.0f, Fill::color(sketch::kit::theme().palette.rule))),
           std::move(lettering).inset(down, across)});
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Element figure) {
  return {.title = title,
          .control = call,
          .figure = std::move(figure),
          .note = note};
}

/** A ring plate: @p words on the inscribed circle, set as @p baseline
 *  says. */
sketch::kit::ComparisonCase ring(const char* title, const char* call,
                                 const char* note, const char* words,
                                 TextPath baseline) {
  baseline.path = shapes::circle();
  return cell(title, call, note,
              plate(shapes::circle(), run(words, std::move(baseline))));
}

/** The large letter both tangent settings draw, on an oval, ridden inside
 *  it so the face stays on the plate. */
Element letter(float size, bool exact, float at, float offset) {
  return text("R")
      .styleClass("inscription")
      .font({.size = size, .color = material::Color{1, 1, 1, 1}})
      .textOnPath({.path = shapes::circle(),
                   .at = at,
                   .align = TextPath::Align::Center,
                   .offset = offset,
                   .exactTangent = exact});
}

}  // namespace

struct TextOnPath {
  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide look(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    const material::Color figure = sketch::kit::theme().palette.figure;

    // THE TWO SETTINGS DRAWN ONCE EACH and held against each other: the
    // difference names the pixel where they part most, and both are
    // shown magnified around it.
    const auto drawn = [&](bool exact) {
      return ctx.texture(
          box().applyStyleSheet(voices()).children(
              {letter(kDetail, exact, 0.78f, -140).inset(4).translateY(110)}),
          kRaster);
    };
    const material::Texture snapped = drawn(false);
    const material::Texture exact = drawn(true);
    const media::PixelDifference apart =
        media::difference(snapped.frameAt().image, exact.frameAt().image);
    const float cropLeft = (float)std::clamp(apart.x - kCrop.width() / 2, 0,
                                             kRaster.width() - kCrop.width());
    const float cropTop = (float)std::clamp(apart.y - kCrop.height() / 2, 0,
                                            kRaster.height() - kCrop.height());
    const auto magnified = [&](const material::Texture& drawing) {
      return sketch::kit::well(
          {.width = kCell,
           .height = kPicture,
           .content = sketch::kit::Well::Content{}},
          image(drawing.source(), material::Fit::Stretch)
              .imageRegion(sigil::geometry::path::fromSk(SkRect::MakeXYWH(
                  cropLeft, cropTop, (float)kCrop.width(),
                  (float)kCrop.height())))
              .imageRendering(material::Sampling::Nearest)
              .width(kCrop.width() * kMagnification)
              .height(kCrop.height() * kMagnification));
    };
    const auto display = [&](bool exactTangent) {
      return sketch::kit::well({.width = kCell, .height = kPicture})
          .children({text("Ravello")
                         .styleClass("inscription")
                         .font({.size = kDisplay, .color = figure})
                         .inset(14)
                         .textOnPath({.path = shapes::circle(),
                                      .at = 0.75f,
                                      .align = TextPath::Align::Center,
                                      .offset = -45,
                                      .exactTangent = exactTangent})});
    };
    const std::string parted =
        kit::formatted("%d pixels differ · widest gap %d of 255",
                       apart.differingPixels, apart.worst);

    ctx.composer.render(
        sketch::kit::page(
            {.title = "Type on a curve",
             .subtitle = "One run, one baseline. Each option moves the run "
                         "along it, off it, or round it.",
             .footer = "The baseline resolves against the text node's own "
                       "box; the faint guide is the same generator in the "
                       "same box."},
            box().column().gap(26).children(
                {sketch::kit::section(
                     {.label = "01  WHERE ALONG IT",
                      .note = "at = 0.75 on a closed ring · what align pins "
                              "there"},
                     sketch::kit::comparison(
                         {.cases =
                              {ring("START", "at 0.75 · Align::Start",
                                    "The run begins at the fraction.",
                                    "BEGINS HERE AND RUNS ON",
                                    {.at = 0.75f,
                                     .align = TextPath::Align::Start}),
                               ring("CENTRE", "at 0.75 · Align::Center",
                                    "The run straddles it.",
                                    "BEGINS HERE AND RUNS ON",
                                    {.at = 0.75f,
                                     .align = TextPath::Align::Center}),
                               ring("END", "at 0.75 · Align::End",
                                    "The run finishes at it.",
                                    "BEGINS HERE AND RUNS ON",
                                    {.at = 0.75f,
                                     .align = TextPath::Align::End}),
                               ring("WRAPPED", "at 1.6 · Align::Center",
                                    "A closed path wraps the fraction: 1.6 "
                                    "is 0.6.",
                                    "ROUND AND ROUND",
                                    {.at = 1.6f,
                                     .align = TextPath::Align::Center})},
                          .measure = 1020,
                          .gap = 20})),
                 sketch::kit::section(
                     {.label = "02  OFF THE BASELINE",
                      .note = "Offset · the lower half, as engraved and "
                              "flipped · the radius"},
                     sketch::kit::comparison(
                         {.cases =
                              {ring("OUTWARD", "offset = 12",
                                    "Positive is left of travel: outside a "
                                    "clockwise ring.",
                                    "RIDING OUTSIDE",
                                    {.at = 0.75f,
                                     .align = TextPath::Align::Center,
                                     .offset = 12}),
                               ring("AS ENGRAVED", "at 0.25 · autoFlip off",
                                    "Glyph-up points outward everywhere, so "
                                    "the lower half reads inverted.",
                                    "UNDER THE RING",
                                    {.at = 0.25f,
                                     .align = TextPath::Align::Center}),
                               ring("FLIPPED", "at 0.25 · autoFlip = true",
                                    "The lower half turned to read right way "
                                    "up, as signage sets it.",
                                    "UNDER THE RING",
                                    {.at = 0.25f,
                                     .align = TextPath::Align::Center,
                                     .autoFlip = true}),
                               ring("ALONG THE RADIUS",
                                    "orient = Radial",
                                    "Each glyph stands like a spoke, the way "
                                    "a compass rose is lettered.",
                                    "NORTH · EAST · SOUTH · WEST · ",
                                    {.at = 0.0f,
                                     .orient = TextPath::Orient::Radial,
                                     .fit = TextPath::Fit::Spacing})},
                          .measure = 1020,
                          .gap = 20})),
                 sketch::kit::section(
                     {.label = "03  OPEN PATHS AND FIT",
                      .note = "An arc and a spiral walked end to end · a run "
                              "fitted to a ring"},
                     sketch::kit::comparison(
                         {.cases =
                              {cell("AN OPEN ARC", "arc(200°, 340°) · Start",
                                    "Walked from its own start; glyphs past "
                                    "the end are dropped.",
                                    plate(shapes::arc(200, 340),
                                          run("OVER THE ARCH OF THE DOOR",
                                              {.path = shapes::arc(200, 340),
                                               .at = 0.0f}))),
                               cell("A SPIRAL", "spiral(3.2) · at 0.42",
                                    "Every turn is one coordinate; the run "
                                    "winds in.",
                                    plate(shapes::spiral(3.2f),
                                          run("a tight spiral carries its "
                                              "whole run",
                                              {.path = shapes::spiral(3.2f),
                                               .at = 0.42f,
                                               .align =
                                                   TextPath::Align::Center}))),
                               ring("FITTED BY SIZE", "fit = Size",
                                    "The type grows until the run closes on "
                                    "its own seam.",
                                    "ROUND THE WHOLE RING · ",
                                    {.fit = TextPath::Fit::Size}),
                               ring("FITTED BY SPACING", "fit = Spacing",
                                    "The size holds; every advance stretches "
                                    "to close the ring.",
                                    "ROUND THE WHOLE RING · ",
                                    {.fit = TextPath::Fit::Spacing})},
                          .measure = 1020,
                          .gap = 20})),
                 sketch::kit::section(
                     {.label = "04  EXACT TANGENT",
                      .note = parted},
                     sketch::kit::comparison(
                         {.cases =
                              {cell("SNAPPED", "64 px · exactTangent = false",
                                    "The default: each turn on the rotation "
                                    "ladder.",
                                    display(false)),
                               cell("EXACT", "64 px · exactTangent = true",
                                    "The same letters on continuous "
                                    "tangents.",
                                    display(true)),
                               cell("SNAPPED, ENLARGED",
                                    "260 px · where they part most",
                                    "The edge where the two differ most, "
                                    "eight times.",
                                    magnified(snapped)),
                               cell("EXACT, ENLARGED",
                                    "260 px · the same pixels",
                                    "The same crop of the exact letter: its "
                                    "edge stands a pixel over.",
                                    magnified(exact))},
                          .measure = 1020,
                          .gap = 20}))}))
            .applyStyleSheet(voices()));
  }
};

SIGIL_SKETCH(TextOnPath, "Kit · API",
             "a run of type on a curve: where along it by at and align, off "
             "it by offset, flipped and radial on a ring, walked along an "
             "open arc and a spiral, fitted to close a ring, and the "
             "rotation ladder against exact tangents, magnified where the "
             "two part most")
