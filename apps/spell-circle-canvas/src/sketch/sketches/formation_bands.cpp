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
 * inside it. There is no defensible default beyond Centered, so the
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

#include <include/core/SkPath.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/path/Band.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>

namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 748};
constexpr float kCell = 340;
constexpr float kPicture = 248;

constexpr float kAmplitude = 11;   // the wave law's swing, px
constexpr float kWavelength = 54;  // px per cycle of it
constexpr float kRail = 15;        // the constant offset, px

constexpr SkColor4f kSpine{0.44f, 0.70f, 0.95f, 1};
constexpr SkColor4f kFigure{0.86f, 0.80f, 0.66f, 1};
constexpr SkColor4f kBandFill{0.95f, 0.62f, 0.30f, 0.34f};
constexpr SkColor4f kBandEdge{0.95f, 0.62f, 0.30f, 1};

/** The house sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.type.captionLabel = {.size = 12, .track = 1.2f};
  look.type.captionNote = {.size = 10.5f, .mono = true};
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

SkPaint strokePaint(SkColor4f color, float width) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setColor4f(color);
  return p;
}

SkPaint fillPaint(SkColor4f color) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color);
  return p;
}

Element cell(const char* call, const std::string& note,
             std::function<void(SkCanvas&)> draw) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well(
          {.width = kCell, .height = kPicture, .clip = false},
          custom(call, [draw = std::move(draw)](SkCanvas& canvas,
                                                const PaintContext&) {
            draw(canvas);
          })));
}

/** The spine drawn under everything, so each cell says what it added. */
void ghost(SkCanvas& canvas) {
  canvas.drawPath(spine(), strokePaint(kSpine, 1.2f));
}

/** A closed region: filled, then its own boundary drawn, because a band
 *  is a region and its two rails at once. The spine goes on TOP of it,
 *  since which side of the spine the mark took is the whole subject. */
void region(SkCanvas& canvas, const SkPath& p) {
  canvas.drawPath(p, fillPaint(kBandFill));
  canvas.drawPath(p, strokePaint(kBandEdge, 1.3f));
  canvas.drawPath(spine(), strokePaint(kSpine, 1.4f));
}

}  // namespace

struct FormationBands final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const path::Profile wave = path::profile::wave(kAmplitude, kWavelength);

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("FORMATION BANDS \xc2\xb7 Profile + profileOffset "
                       "+ bandRegion"),
         .subtitle = toU8("dials \xc2\xb7 the formation (Centered, "
                          "Outward, Inward) \xc2\xb7 the amplitude "
                          "(11 px) \xc2\xb7 the wavelength (54 px per "
                          "cycle)"),
         .footer = toU8("positive across is LEFT of travel, which on a "
                        "clockwise path is outside it \xe2\x80\x94 so "
                        "Outward and Inward are not a sign the caller "
                        "picks but a side the formation names")},
        kit::cells(
            {.cells =
                 {kit::cells(
                      {.cells =
                           {cell("profileOffset(spine, profile::self())",
                                 kit::format(
                                     "across \xe2\x89\xa1 0 \xc2\xb7 "
                                     "max() %.0f px \xe2\x80\x94 the "
                                     "boundary itself, which is the "
                                     "law every other law is measured "
                                     "against",
                                     (double)path::profile::self().max()),
                                 [](SkCanvas& canvas) {
                                   ghost(canvas);
                                   canvas.drawPath(
                                       path::profileOffset(
                                           spine(), path::profile::self()),
                                       strokePaint(kFigure, 2.4f));
                                 }),
                            cell("profileOffset(spine, "
                                 "profile::offset(15))",
                                 kit::format(
                                     "across \xe2\x89\xa1 15 \xc2\xb7 "
                                     "max() %.0f px \xc2\xb7 a "
                                     "constant law delegates to "
                                     "parallel, so the corners take an "
                                     "arc outside and a miter inside",
                                     (
                                         double)path::profile::offset(kRail)
                                         .max()),
                                 [](SkCanvas& canvas) {
                                   ghost(canvas);
                                   canvas.drawPath(
                                       path::profileOffset(
                                           spine(),
                                           path::profile::offset(kRail)),
                                       strokePaint(kFigure, 2.4f));
                                 }),
                            cell("profileOffset(spine, "
                                 "profile::wave(11, 54))",
                                 kit::format(
                                     "one rail of the wave law \xc2\xb7 "
                                     "max() %.0f px, which is what "
                                     "bleed and cull are sized from",
                                     (
                                         double)wave.max()),
                                 [wave](SkCanvas& canvas) {
                                   ghost(canvas);
                                   canvas.drawPath(
                                       path::profileOffset(spine(), wave),
                                       strokePaint(kFigure, 2.4f));
                                 })},
                       .gap = 14}),
                  kit::cells(
                      {.cells =
                           {cell("bandRegion(spine, wave, "
                                 "Formation::Centered)",
                                 "both rails at \xc2\xb1"
                                 "across, closed "
                                 "per contour \xc2\xb7 a law that "
                                 "crosses zero pinches the band shut "
                                 "wherever it does",
                                 [wave](SkCanvas& canvas) {
                                   region(canvas,
                                          path::bandRegion(
                                              spine(), wave,
                                              path::Formation::Centered));
                                 }),
                            cell("bandRegion(spine, wave, "
                                 "Formation::Outward)",
                                 "the spine (blue) is the INNER rail "
                                 "\xc2\xb7 the whole mark stands "
                                 "outside the figure it was measured "
                                 "from",
                                 [wave](SkCanvas& canvas) {
                                   region(canvas,
                                          path::bandRegion(
                                              spine(), wave,
                                              path::Formation::Outward));
                                 }),
                            cell("bandRegion(spine, wave, "
                                 "Formation::Inward)",
                                 "the spine (blue) is the OUTER rail "
                                 "\xc2\xb7 the mark falls entirely "
                                 "within the figure, which is what a "
                                 "milled groove wants",
                                 [wave](SkCanvas& canvas) {
                                   region(canvas, path::bandRegion(
                                                      spine(), wave,
                                                      path::Formation::Inward));
                                 })},
                       .gap = 14})},
             .column = true,
             .gap = 18})));
  }
};

SIGIL_SKETCH(FormationBands, "Kit \xc2\xb7 API",
             "a width law walked as one rail by profileOffset and closed as "
             "a region by bandRegion, in each of the three formations")
