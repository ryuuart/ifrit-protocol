/** @file
 * ocio_view — a colour transform as an ordinary material, and the one
 * indirection that makes it one.
 *
 * OCIO's own GPU codegen emits GLSL, HLSL, MSL and OSL and never SkSL,
 * so it cannot shade here directly. Each factory builds a CPU processor
 * for the transform it was asked for, bakes it ONCE into a 3D LUT, holds
 * that LUT as a texture, and applies it through a trilinear recipe whose
 * one open slot — `content` — is the layer being transformed. So a
 * renderer binds its output to that slot and pays one sample per pixel
 * and nothing of OCIO proper per frame.
 *
 * The colour contract: what the content carries is treated as the
 * transform's INPUT space. For a display/view transform, author in the
 * config's scene-linear role and the view maps linear to display.
 *
 * The LUTs bake to F16 rather than F32 because an F32 texture is not
 * linearly filterable on Apple GPUs — a trilinear sampler over one would
 * fall back to point sampling and band.
 *
 * A build that found no OpenColorIO still LINKS: `available()` is false
 * and every factory answers the empty material a bad config would, which
 * is what the last cell reads back.
 *
 * EDIT THESE FIRST
 *   kGamma   — the exponent the plumbing test applies.
 *   kLutSize — the 3D LUT's side. 33 is the usual working size.
 *   kConfig  — the config an ocio:// URI names.
 */

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace ocio = sigil::material::ocio;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 640};
constexpr float kCell = 341;
constexpr float kPicture = 200;

constexpr float kGamma = 2.2f;  // the exponent the plumbing test applies
constexpr int kLutSize = 33;    // the 3D LUT's side
constexpr const char* kConfig = "ocio://default";

SkPath whole() {
  static const SkPath path =
      SkPathBuilder().addRect(SkRect::MakeWH(kCell, kPicture)).detach();
  return path;
}

/** THE SUBJECT every cell transforms: a linear step wedge over three
 *  primary ramps, baked once. A step wedge is what a transform is read
 *  off — a smooth gradient hides where a curve lifts the shadows. */
material::Texture wedge() {
  return material::Texture::produce("ocio_view.wedge", [] {
    constexpr int kW = 320, kH = 180;
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kW, kH));
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorBLACK);
    SkPaint step;
    step.setAntiAlias(false);
    constexpr int kSteps = 10;
    for (int i = 0; i < kSteps; ++i) {
      const float v = (float)i / (float)(kSteps - 1);
      step.setColor4f({v, v, v, 1});
      canvas->drawRect(SkRect::MakeXYWH((float)i * kW / kSteps, 0,
                                        (float)kW / kSteps, kH * 0.5f),
                       step);
    }
    const SkColor4f primaries[3] = {
        {1, 0.15f, 0.10f, 1}, {0.15f, 1, 0.25f, 1}, {0.20f, 0.35f, 1, 1}};
    for (int band = 0; band < 3; ++band) {
      const SkPoint ends[2] = {{0, 0}, {kW, 0}};
      const SkColor4f stops[2] = {{0, 0, 0, 1}, primaries[band]};
      SkPaint ramp;
      ramp.setShader(SkShaders::LinearGradient(
          ends, SkGradient({{stops, 2}, {}, SkTileMode::kClamp}, {})));
      canvas->drawRect(
          SkRect::MakeXYWH(0, kH * (0.5f + (float)band / 6.0f), kW, kH / 6.0f),
          ramp);
    }
    return surface->makeImageSnapshot();
  });
}

/** The transform with the wedge bound into its one open slot, which is
 *  the whole shape of using this feature. */
material::Material through(material::Material transform) {
  material::Texture map = wedge();
  map.uv(SkMatrix::Scale(kCell / 320.0f, kPicture / 180.0f));
  transform.child("content", std::move(map));
  return transform;
}

Element cell(const char* call, const std::string& note,
             material::Material paint) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well(
          {.width = kCell, .height = kPicture},
          custom(call, [paint = std::move(paint)](SkCanvas& canvas,
                                                  const PaintContext& pc) {
            material::skia::fill(
                canvas, whole(), paint,
                {.resolution = {pc.size.width(), pc.size.height()}});
          })));
}

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.cellGround = {0.09f, 0.095f, 0.11f, 1};
  return look;
}

}  // namespace

struct OcioView final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    material::skia::install();  // the SkSL compiler, once per process

    const bool have = ocio::available();

    Element content = kit::cells(
        {.cells =
             {kit::cells(
                  {.cells =
                       {cell("the wedge, untransformed",
                             "a ten-step wedge over three primary "
                             "ramps \xc2\xb7 a step wedge is what a "
                             "transform is read off, since a smooth "
                             "ramp hides a lifted shadow",
                             through(ocio::exponent(1.0f, kLutSize))),
                        cell("ocio::exponent(2.2)",
                             "a plain gamma through the raw "
                             "config \xc2\xb7 needs no config file, "
                             "which is what makes it the plumbing "
                             "test",
                             through(ocio::exponent(kGamma, kLutSize))),
                        cell("ocio::exponent(1 / 2.2)",
                             "and its inverse \xc2\xb7 the two "
                             "compose back to the wedge above, "
                             "which is the whole check",
                             through(ocio::exponent(1.0f / kGamma, kLutSize)))},
                   .gap =
                       14}),
              kit::cells(
                  {.cells =
                       {cell("convert(config, lin_srgb, srgb_tx)",
                             "a colour-space conversion from the "
                             "same config sources \xc2\xb7 input is "
                             "whatever the content carries",
                             through(ocio::convert(
                                 kConfig,
                                 "lin_srgb",
                                 "srgb_tx",
                                 kLutSize))),
                        cell("viewTransform(config, display, view)",
                             "\"sRGB - Display\" and \"ACES 2.0 - "
                             "SDR 100 nits (Rec.709)\" \xc2\xb7 "
                             "author in the config's scene-linear "
                             "role and the view maps linear to "
                             "display",
                             through(ocio::viewTransform(
                                 kConfig,
                                 "sRGB - Display",
                                 "ACES 2.0 - SDR 100 nits (Rec.709)",
                                 kLutSize))),
                        cell("viewTransform(\xe2\x80\xa6"
                             ", bad view)",
                             "a bad name must not take the canvas "
                             "down \xc2\xb7 the error is reported, "
                             "an EMPTY material comes back, and it "
                             "paints nothing",
                             through(ocio::viewTransform(
                                 kConfig,
                                 "sRGB - Display",
                                 "no such view", kLutSize)))},
                   .gap = 14})},
         .column = true,
         .gap = 18});
    ctx.composer.render(sketch::kit::page(
        {.title = toU8("OCIO VIEW \xc2\xb7 ocio::exponent, convert, "
                       "viewTransform \xe2\x80\x94 each a baked 3D LUT"),
         .subtitle = toU8(kit::format(
             "dials \xc2\xb7 the exponent (%.1f) \xc2\xb7 the LUT side "
             "(%d) \xc2\xb7 the config (\"%s\") \xc2\xb7 the display "
             "and view names \xc2\xb7 available() is %s here",
             (double)kGamma, kLutSize, kConfig, have ? "true" : "false")),
         .footer = toU8("the transform is a material like any other: "
                        "one open slot, one trilinear sample per pixel, "
                        "and nothing of OCIO proper between the bake "
                        "and the frame")},
        std::move(content)));
  }
};

SIGIL_SKETCH(OcioView, "Kit \xc2\xb7 API",
             "one step wedge through a gamma and its inverse, a colour-space "
             "conversion, a display view, and the empty material a bad name "
             "answers with")
