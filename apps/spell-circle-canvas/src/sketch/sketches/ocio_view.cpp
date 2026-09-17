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

// TAGS: Materials/Color

#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace ocio = sigil::material::ocio;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 940};
constexpr float kCell = 328;
constexpr float kPicture = 208;

constexpr float kGamma = 2.2f;  // the exponent the plumbing test applies
constexpr int kLutSize = 33;    // the 3D LUT's side
constexpr const char* kConfig = "ocio://default";

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
  transform.slot("content", std::move(map));
  return transform;
}

sketch::kit::ComparisonCase cell(const char* caseTitle, const char* call,
                                 const std::string& note,
                                 material::Material paint) {
  // THE TRANSFORM IS THE CELL'S GROUND: a well grounded in something
  // generated per pixel is what the ground slot is for, so nothing here
  // records a path or a paint.
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kCell,
               .height = kPicture,
               .ground = material::skia::Paint::recipe(std::move(paint))}),
          .note = note};
}

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.09f, 0.095f, 0.11f, 1};
  return look;
}

}  // namespace

struct OcioView {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const bool have = ocio::available();

    ctx.composer.render(sketch::kit::page(
        {.title = "Read a colour transform",
         .subtitle = "A step wedge reveals what the curve does to the signal",
         .footer = "The transform is baked into a 3D LUT once. Its content "
                   "slot receives the image to transform."},
        box().column().gap(28).children(
            {sketch::kit::sectionHeader(
                 {.label = "THE TRANSFER CURVE",
                  .note = kit::formatted(
                      "OCIO %s · %d³ LUT · input steps held constant",
                      have ? "available" : "unavailable", kLutSize)}),
             sketch::kit::comparison(
                 {.cases = {cell("IDENTITY", "the wedge, untransformed",
                                 "Ten equal input steps expose changes in "
                                 "shadow and highlight spacing.",
                                 through(ocio::exponent(1.0f, kLutSize))),
                            cell("GAMMA 2.2", "ocio::exponent(2.2)",
                                 "Exponent 2.2 darkens the middle steps.",
                                 through(ocio::exponent(kGamma, kLutSize))),
                            cell("INVERSE GAMMA", "ocio::exponent(1 / 2.2)",
                                 "The inverse exponent lifts the same steps.",
                                 through(
                                     ocio::exponent(1.0f / kGamma, kLutSize)))},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::sectionHeader(
                 {.label = "CONFIGURED COLOUR",
                  .note = "Conversions name their input and output spaces; a "
                          "display view also names a viewing transform."}),
             sketch::kit::comparison(
                 {.cases = {cell("COLOUR-SPACE CONVERSION",
                                 "convert(config, lin_srgb, srgb_tx)",
                                 "Convert linear sRGB into encoded sRGB.",
                                 through(ocio::convert(kConfig, "lin_srgb",
                                                       "srgb_tx", kLutSize))),
                            cell("DISPLAY / VIEW",
                                 "viewTransform(config, display, view)",
                                 "The scene-linear input passes through the "
                                 "selected ACES display view.",
                                 through(ocio::viewTransform(
                                     kConfig, "sRGB - Display",
                                     "ACES 2.0 - SDR 100 nits (Rec.709)",
                                     kLutSize))),
                            cell("EXPECTED EMPTY RESULT",
                                 "viewTransform(…"
                                 ", bad view)",
                                 "An unknown view reports an error and returns "
                                 "an empty material.",
                                 through(ocio::viewTransform(
                                     kConfig, "sRGB - Display", "no such view",
                                     kLutSize)))},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(OcioView, "Kit · API",
             "one step wedge through a gamma and its inverse, a colour-space "
             "conversion, a display view, and the empty material a bad name "
             "answers with")
