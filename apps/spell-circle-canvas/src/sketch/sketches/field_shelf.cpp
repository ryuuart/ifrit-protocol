/** @file
 * field_shelf — every shader field the library ships, twice: once at
 * stock, once with the dial that changes what it MEANS turned.
 *
 * A field is a surface evaluated per pixel rather than baked as a tile,
 * so every parameter is a uniform and each one answers a Material like
 * any other. Two of the five have a shape worth knowing before reaching
 * for them.
 *
 * `noise` and `grain` are not the same generator at two settings.
 * `noise` is Skia's own Perlin, whose three channels are INDEPENDENT
 * fields — right for a displacement source, wrong for grain, which would
 * read as a hue shift. `grain` is value-noise fBm collapsed to ONE
 * channel, so a blend mode over a coloured surface reads as light: paper
 * tooth, film grain, stone veining, worn metal.
 *
 * `crtOverlay` shades nothing. It is black with the alpha carrying the
 * scanlines and the corner falloff, so it darkens what is UNDER it and
 * is drawn as the last layer over the frame it ages. Both crt cells here
 * paint a ground first, which is what a caller would be doing.
 *
 * `ripple` resamples its `content` child through a sine displacement, so
 * it is the one field on the shelf with a slot to fill.
 *
 * EDIT THESE FIRST
 *   kSpacing  — the halftone lattice pitch, px.
 *   kNoiseHz  — features per px for the two noise fields.
 *   kSeed     — the seed every generated field is offset by.
 */

// TAGS: Materials/Shaders

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace field = sigil::material::field;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1160};
constexpr float kCell = 240;
constexpr float kPicture = 180;

constexpr float kSpacing = 9;       // the halftone lattice pitch, px
constexpr float kNoiseHz = 0.035f;  // features per px
constexpr float kSeed = 4;          // the seed every generated field offsets by

constexpr SkColor4f kScreen{0.72f, 0.80f, 0.62f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.09f, 0.095f, 0.11f, 1};
  return look;
}

SkPath whole() {
  return SkPathBuilder().addRect(SkRect::MakeWH(kCell, kPicture)).detach();
}

/** What `ripple` warps: a ruled grid, baked once, so the displacement is
 *  legible as a displacement rather than as a texture. */
material::Texture ruled() {
  return material::Texture::produce("field_shelf.ruled", [] {
    constexpr int kSide = 200;
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SkColor4f{0.10f, 0.13f, 0.18f, 1}.toSkColor());
    SkPaint line;
    line.setAntiAlias(true);
    line.setStyle(SkPaint::kStroke_Style);
    line.setStrokeWidth(2);
    line.setColor4f({0.55f, 0.82f, 0.92f, 1});
    for (int i = 0; i <= kSide; i += 20) {
      canvas->drawLine((float)i, 0, (float)i, kSide, line);
      canvas->drawLine(0, (float)i, kSide, (float)i, line);
    }
    return surface->makeImageSnapshot();
  });
}

material::Material rippled(float amplitude, float wavelength, bool vertical) {
  material::Material warp = field::ripple(amplitude, wavelength, 0, vertical);
  // The grid repeats, so a displacement that reads past the edge finds
  // more grid rather than a smeared last row.
  warp.slot("content", material::Texture(ruled()).tile(SkTileMode::kRepeat));
  return warp;
}

sketch::kit::ComparisonCase cell(
    const char* caseTitle, const char* call, const char* note,
    std::function<void(SkCanvas&, const material::FrameData&)> draw) {
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture},
              custom(call,
                     [draw = std::move(draw)](SkCanvas& canvas,
                                              const PaintContext& pc) {
                       draw(canvas, {.resolution = {pc.size.width(),
                                                    pc.size.height()}});
                     })),
          .note = note};
}

/** A field on its own. */
sketch::kit::ComparisonCase plain(const char* caseTitle, const char* call,
                                  const char* note, material::Material paint) {
  return cell(caseTitle, call, note,
              [paint = std::move(paint), face = whole()](
                  SkCanvas& canvas, const material::FrameData& frame) {
                material::skia::fill(canvas, face, paint, frame);
              });
}

/** A field over a lit ground — what crtOverlay is for. */
sketch::kit::ComparisonCase aged(const char* caseTitle, const char* call,
                                 const char* note, material::Material paint) {
  return cell(caseTitle, call, note,
              [paint = std::move(paint), face = whole()](
                  SkCanvas& canvas, const material::FrameData& frame) {
                SkPaint ground;
                ground.setColor4f(kScreen);
                canvas.drawPath(face, ground);
                material::skia::fill(canvas, face, paint, frame);
              });
}

}  // namespace

struct FieldShelf {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = "Five fields, five questions",
         .subtitle = "Pattern, channel structure, displacement, and an overlay",
         .footer = "These fields are evaluated per pixel; their parameters "
                   "remain available as uniforms."},
        box().column().gap(28).children(
            {box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "HALFTONE",
                                .note = "Change the lattice, keep the vertical "
                                        "swell."}),
                           sketch::kit::comparison(
                               {.cases =
                                    {plain(
                                         "RADIUS RAMP",
                                         "halftoneRamp(9, 0.5, 4, ink)",
                                         "The dot radius grows down the page.",
                                         field::halftoneRamp(
                                             kSpacing, 0.5f, 4.0f,
                                             {0.94f, 0.90f, 0.80f, 1})),
                                     plain("ROTATED LATTICE",
                                           "halftoneRamp(…, 30, 0.25, 0.75)",
                                           "Turn the lattice; keep the ramp "
                                           "vertical and narrow its range.",
                                           field::halftoneRamp(
                                               kSpacing, 0.5f,
                                               4.0f, {0.94f, 0.90f, 0.80f, 1},
                                               30, 0.25f, 0.75f))},
                                .measure = 500,
                                .gap = 20})}),
                      box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "RIPPLE",
                                .note = "Change which coordinate moves."}),
                           sketch::kit::comparison(
                               {.cases = {plain(
                                              "VERTICAL DISPLACEMENT",
                                              "ripple(7, 96) over a ruled grid",
                                              "A sine in x displaces the grid "
                                              "vertically.",
                                              rippled(7, 96, false)),
                                          plain("HORIZONTAL DISPLACEMENT",
                                                "ripple(9, 70, vertical)",
                                                "A sine in y displaces the "
                                                "grid horizontally.",
                                                rippled(9, 70, true))},
                                .measure = 500,
                                .gap = 20})})}),
             sketch::kit::sectionHeader(
                 {.label = "NOISE IS NOT GRAIN",
                  .note = "Compare channel structure first; then change the "
                          "character of each field."}),
             sketch::kit::comparison(
                 {.cases =
                      {plain("PERLIN · RGB", "noise(0.035, 4, 4)",
                             "Independent colour channels suit displacement.",
                             field::noise(kNoiseHz, 4, kSeed)),
                       plain("TURBULENCE · RGB", "noise(0.035, 4, 4, true)",
                             "Absolute-value turbulence folds the field into "
                             "veins.",
                             field::noise(kNoiseHz, 4, kSeed, true)),
                       plain("GRAIN · MONO", "grain(0.035, 4, 4, 1)",
                             "A single luminance channel suits surface grain.",
                             field::grain(kNoiseHz, 4, kSeed)),
                       plain("FIBRE · MONO", "grain(0.02, 4, 4, 1.6, 7)",
                             "Stretch the field into long fibres.",
                             field::grain(0.02f, 4, kSeed, 1.6f, 7))},
                  .measure = 1020,
                  .gap = 20}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "A SCREEN OVERLAY", .note = ""}),
                           sketch::kit::comparison(
                               {.cases =
                                    {aged("STOCK TUBE", "crtOverlay()",
                                          "Alpha scanlines darken the green "
                                          "ground.",
                                          field::crtOverlay()),
                                     aged("COARSER TUBE",
                                          "crtOverlay(8, 0.16, 1.1, 1.9, 0.7)",
                                          "Coarser lines and a stronger corner "
                                          "falloff.",
                                          field::crtOverlay(8, 0.16f, 1.1f,
                                                            1.9f, 0.7f))},
                                .measure = 500,
                                .gap = 20})}),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "WHAT A FIELD DOES", .note = ""}),
                           text("Halftone, noise and grain generate colour. "
                                "Ripple samples a child image at displaced "
                                "coordinates. The CRT overlay instead supplies "
                                "black and alpha, changing the image beneath "
                                "it.")
                               .width(500)
                               .styleClass("captionNote"),
                           text("The paired wells keep the same size and seed. "
                                "Their controls name the one property being "
                                "explored.")
                               .width(500)
                               .styleClass("captionNote")})})})));
  }
};

SIGIL_SKETCH(FieldShelf, "Specimen",
             "the five shader fields at stock on one shelf and, under each, "
             "the one dial that changes what it means")
