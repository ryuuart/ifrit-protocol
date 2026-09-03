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

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <functional>
#include <string>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
namespace field = sigil::material::field;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 636};
constexpr float kCell = 200;
constexpr float kPicture = 182;

constexpr float kSpacing = 9;      // the halftone lattice pitch, px
constexpr float kNoiseHz = 0.035f; // features per px
constexpr float kSeed = 4;         // the seed every generated field offsets by

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.09f, 0.095f, 0.11f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kScreen{0.72f, 0.80f, 0.62f, 1};

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
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

SkPath whole() {
  static const SkPath path =
      SkPathBuilder().addRect(SkRect::MakeWH(kCell, kPicture)).detach();
  return path;
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
  warp.child("content", material::Texture(ruled()).tile(SkTileMode::kRepeat));
  return warp;
}

Element cell(const char* call, const char* note,
             std::function<void(SkCanvas&, const material::FrameData&)> draw) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   custom(call,
                          [draw = std::move(draw)](SkCanvas& canvas,
                                                   const PaintContext& pc) {
                            draw(canvas, {.resolution = {pc.size.width(),
                                                         pc.size.height()}});
                          })
                       .width(kCell)
                       .height(kPicture)
                       .clip()
                       .fill(Fill::color(kCellGround)));
}

/** A field on its own. */
Element plain(const char* call, const char* note, material::Material paint) {
  return cell(call, note,
              [paint = std::move(paint)](SkCanvas& canvas,
                                         const material::FrameData& frame) {
                material::skia::fill(canvas, whole(), paint, frame);
              });
}

/** A field over a lit ground — what crtOverlay is for. */
Element aged(const char* call, const char* note, material::Material paint) {
  return cell(call, note,
              [paint = std::move(paint)](SkCanvas& canvas,
                                         const material::FrameData& frame) {
                SkPaint ground;
                ground.setColor4f(kScreen);
                canvas.drawPath(whole(), ground);
                material::skia::fill(canvas, whole(), paint, frame);
              });
}

}  // namespace

struct FieldShelf final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once
    material::skia::install();  // the SkSL compiler, once per process

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("FIELD SHELF \xc2\xb7 field:: halftoneRamp, noise, "
                           "grain, ripple, crtOverlay"),
             .subtitle = toU8("dials \xc2\xb7 the pitch (9 px) \xc2\xb7 the "
                              "frequency (0.035 features per px) \xc2\xb7 "
                              "the seed (4) \xc2\xb7 and, in the bottom row, "
                              "the one dial that changes what each means"),
             .footer = toU8("every parameter is a uniform, so a field is "
                            "shaded per pixel and never baked \xe2\x80\x94 "
                            "which is what lets halftoneRamp's drift be a "
                            "binding rather than a re-bake"),
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
                               {plain("halftoneRamp(9, 0.5, 4, ink)",
                                      "the dot radius swells from rMin at "
                                      "the top of the box to rMax at its "
                                      "bottom, in one pass",
                                      field::halftoneRamp(
                                          kSpacing, 0.5f, 4.0f,
                                          {0.94f, 0.90f, 0.80f, 1})),
                                plain("noise(0.035, 4, 4)",
                                      "Skia's Perlin, passed through a "
                                      "recipe \xc2\xb7 three INDEPENDENT "
                                      "channels, which is a displacement "
                                      "source",
                                      field::noise(kNoiseHz, 4, kSeed)),
                                plain("grain(0.035, 4, 4, 1)",
                                      "value-noise fBm collapsed to ONE "
                                      "channel \xc2\xb7 luminance, so a "
                                      "blend over colour reads as light",
                                      field::grain(kNoiseHz, 4, kSeed)),
                                plain("ripple(7, 96) over a ruled grid",
                                      "y shifted by a sine of x \xc2\xb7 the "
                                      "content slot is the caller's, and "
                                      "here it is a grid",
                                      rippled(7, 96, false)),
                                aged("crtOverlay()",
                                     "black with the alpha carrying hard "
                                     "scanlines and a corner falloff "
                                     "\xc2\xb7 it ages what is under it",
                                     field::crtOverlay())},
                           .gap = 12}),
                      kit::cells(
                          {.cells =
                               {plain("halftoneRamp(\xe2\x80\xa6, 30, 0.25, "
                                      "0.75)",
                                      "angleDeg turns the LATTICE and the "
                                      "ramp stays vertical \xc2\xb7 the "
                                      "swell band remapped to the middle "
                                      "half",
                                      field::halftoneRamp(
                                          kSpacing, 0.5f, 4.0f,
                                          {0.94f, 0.90f, 0.80f, 1}, 30, 0.25f,
                                          0.75f)),
                                plain("noise(0.035, 4, 4, true)",
                                      "the turbulence variant \xe2\x80\x94 "
                                      "the abs-value fold, which is sharper "
                                      "and veiny",
                                      field::noise(kNoiseHz, 4, kSeed, true)),
                                plain("grain(0.02, 4, 4, 1.6, 7)",
                                      "stretch divides the x frequency and "
                                      "multiplies the y one, so the fibre "
                                      "runs lengthwise",
                                      field::grain(0.02f, 4, kSeed, 1.6f, 7)),
                                plain("ripple(9, 70, vertical)",
                                      "\xe2\x80\xa6" "and with the flag, x "
                                      "shifted by a sine of y \xc2\xb7 the "
                                      "same field turned a quarter",
                                      rippled(9, 70, true)),
                                aged("crtOverlay(8, 0.16, 1.1, 1.9, 0.7)",
                                     "a coarser pitch, a harder line and a "
                                     "falloff that reaches most of the way "
                                     "in",
                                     field::crtOverlay(8, 0.16f, 1.1f, 1.9f,
                                                       0.7f))},
                           .gap = 12})},
                 .column = true,
                 .gap = 16}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(FieldShelf, "Specimen",
             "the five shader fields at stock on one shelf and, under each, "
             "the one dial that changes what it means")
