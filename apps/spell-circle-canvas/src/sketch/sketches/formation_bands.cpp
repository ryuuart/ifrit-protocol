/** @file
 * formation_bands — a width law, the rail it walks, and the region
 * between two rails.
 *
 * Every varying-width mark in the tree is this pair. A `Profile` is the
 * LAW — `across(along)` in the spine's own frame, where `along` is a
 * fraction of arc length and positive `across` is LEFT of travel, which
 * with y pointing down is outside a clockwise path. `profileOffset`
 * walks one rail of that law; `bandRegion` walks both and closes them,
 * per contour, so a milled groove, a ribbon and a tapered strand are one
 * geometry rather than three.
 *
 * `Formation` is the only thing left to say once the law is fixed:
 * whether the band straddles the spine, stands outside it, or stands
 * inside it. There is no defensible default beyond Center, so the
 * three are named.
 *
 * The rails go through `parallel`, which repairs real vertices — an arc
 * outside a turn, a miter inside — instead of leaving the spur a naive
 * sample-and-displace leaves on the inside of every corner. That is why
 * the subject here is a hexagon and not a circle.
 *
 * EDIT THESE FIRST
 *   kAmplitude  — how far the wave law swings, px.
 *   kWavelength — px per cycle of it.
 *   kRail       — the constant offset the second cell walks, px.
 */

// TAGS: Geometry/Paths

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/path/Band.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace shapers = sigil::geometry::shapers;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 940};
constexpr float kCell = 328;
constexpr float kPicture = 236;

constexpr float kAmplitude = 11;   // the wave law's swing, px
constexpr float kWavelength = 54;  // px per cycle of it
constexpr float kRail = 15;        // the constant offset, px

constexpr material::Color kSpine{0.44f, 0.70f, 0.95f, 1};
constexpr material::Color kFigure{0.86f, 0.80f, 0.66f, 1};
constexpr material::Color kBandFill{0.95f, 0.62f, 0.30f, 0.34f};
constexpr material::Color kBandEdge{0.95f, 0.62f, 0.30f, 1};

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 12, .track = 1.2f};
  look.spacing.captionGap = 8;
  return look;
}

/** The subject: a regular hexagon, clockwise, with six real corners —
 *  the arrangement where the rails' vertex repair is visible and the
 *  outside and inside of a closed path are unambiguous. */
SkPath spine() {
  const float art = kPicture - 76;
  return shapes::polygon(6)
      .path({art, art})
      .makeTransform(
          SkMatrix::Translate((kCell - art) * 0.5f, (kPicture - art) * 0.5f));
}

SkPaint strokePaint(material::Color color, float width) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setColor4f(material::skia::toSkColor(color));
  return p;
}

SkPaint fillPaint(material::Color color) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(material::skia::toSkColor(color));
  return p;
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const std::string& note,
                                 std::function<void(SkCanvas&)> draw) {
  return {.title = title,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture, .clip = false},
              custom(call, [draw = std::move(draw)](
                               SkCanvas& canvas) { draw(canvas); })),
          .note = note};
}

/** ONE RAIL CELL: the spine under it, and @p law walked as a single rail
 *  over it. */
sketch::kit::ComparisonCase railCell(const char* title, const char* call,
                                     const std::string& note,
                                     path::Profile law) {
  return cell(title, call, note, [law = std::move(law)](SkCanvas& canvas) {
    canvas.drawPath(spine(), strokePaint(kSpine, 1.2f));
    canvas.drawPath(path::profileOffset(spine(), law),
                    strokePaint(kFigure, 2.4f));
  });
}

/** ONE BAND CELL: @p law closed as a region in the formation it names —
 *  filled, then its own boundary drawn, because a band is a region and
 *  its two rails at once. The spine goes on TOP of it, since which side
 *  of the spine the mark took is the whole subject. */
sketch::kit::ComparisonCase bandCell(const char* title, const char* call,
                                     const char* note, path::Profile law,
                                     path::Formation how) {
  return cell(title, call, note, [law = std::move(law), how](SkCanvas& canvas) {
    const SkPath region = path::bandRegion(spine(), law, how);
    canvas.drawPath(region, fillPaint(kBandFill));
    canvas.drawPath(region, strokePaint(kBandEdge, 1.3f));
    canvas.drawPath(spine(), strokePaint(kSpine, 1.4f));
  });
}

}  // namespace

struct FormationBands {
  void setup(sketch::SketchContext& ctx) {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const path::Profile wave = path::Profile(
        shapers::Wave{.amplitude = kAmplitude, .wavelength = kWavelength});

    ctx.composer.render(sketch::kit::page(
        {.title = "From a rail to a band",
         .subtitle = "One clockwise hexagon. First displace its outline; then "
                     "choose which side becomes a region.",
         .footer = "A formation names a side of the source contour, rather "
                   "than asking the caller to guess a sign."},
        box().column().gap(24).children(
            {box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(18)
                 .children(
                     {box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "SOURCE CONTOUR",
                                .note = "Every offset starts on this rail"}),
                           sketch::kit::comparison(
                               {.cases = {railCell("ZERO OFFSET",
                                                   "profile::self()",
                                                   "The warm outline is the "
                                                   "origin of every offset.",
                                                   path::profile::self())},
                                .measure = 328,
                                .gap = 18})}),
                      box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "OFFSET PROFILES",
                                .note = "A rail is one displaced outline"}),
                           sketch::kit::comparison(
                               {.cases = {railCell(
                                              "CONSTANT", "profile::offset(15)",
                                              "A constant 15 px offset follows "
                                              "the outside of the hexagon.",
                                              path::profile::offset(kRail)),
                                          railCell("VARYING", "wave(11, 54)",
                                                   "A wave varies the offset "
                                                   "by up to 11 px.",
                                                   wave)},
                                .measure = 674,
                                .gap = 18})})}),
             sketch::kit::sectionHeader(
                 {.label = "TURN THE PROFILE INTO A REGION",
                  .note = "Blue: source spine · amber: filled band"}),
             sketch::kit::comparison(
                 {.cases = {bandCell("BOTH SIDES", "Formation::Center",
                                     "The region straddles the spine and "
                                     "pinches where the width crosses zero.",
                                     wave, path::Formation::Center),
                            bandCell("OUTSIDE ONLY", "Formation::Outer",
                                     "The blue spine is the inner rail. The "
                                     "entire region sits outside.",
                                     wave, path::Formation::Outer),
                            bandCell("INSIDE ONLY", "Formation::Inner",
                                     "The blue spine is the outer rail. The "
                                     "entire region sits inside.",
                                     wave, path::Formation::Inner)},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(FormationBands, "Kit · API",
             "a width law walked as one rail by profileOffset and closed as "
             "a region by bandRegion, in each of the three formations")
