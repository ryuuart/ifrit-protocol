/** @file
 * env_faces — the four doors into one panorama, and what a reflective
 * surface sees through each.
 *
 * `EnvironmentMap` is the panorama a surface sees when it looks past the
 * lights, and it has ONE internal form: equirectangular, u = azimuth,
 * v = 0 at the zenith. Every source resolves into that form while the
 * value is built — `studio()` and `sunset()` bake one with no assets,
 * `fromEquirectangular()` wraps a loaded lat-long panorama, `fromFaces()`
 * resamples six cube faces, and `fromCubeMap()` unpacks one sheet (a 4:3
 * or 3:4 cross, a 6:1 row or a 1:6 column) into the same.
 *
 * A cube map arrives as an ordinary image because that is what the image
 * library decodes. DDS and KTX — the containers that hold six surfaces
 * in one file — decode nowhere in this tree, so a cube map is unpacked
 * to a sheet or to six files before it gets here.
 *
 * `withGround(colour)` replaces everything below the horizon IN the
 * panorama, so the blurs and the irradiance see it too. That is what a
 * photographed sky wants when its lower half is a tripod and a car park.
 *
 * The bottom row is the same construction every reflective surface uses:
 * a bevel normal map placed at the outline's bounds, so the recipe reads
 * the normal under the pixel it shades, and an environment beside it.
 *
 * EDIT THESE FIRST
 *   kFaceSide — the resolution each cube face is baked at.
 *   kBevel    — the shoulder the disc's normals come from, px.
 *   kGround   — the colour withGround() replaces the lower half with.
 */

// TAGS: Materials/Lighting

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/kit/Environments.h>
#include <sigilmaterial/kit/Reflections.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <array>
#include <string>

namespace sketch = sigil::sketch;
namespace material = sigil::material;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1220};
constexpr float kCell = 328;
constexpr float kPicture = 220;

constexpr int kFaceSide = 128;  // each cube face's resolution
constexpr float kBevel = 30;    // the disc's shoulder, px
constexpr SkColor4f kGroundColour{0.14f, 0.12f, 0.10f, 1};

constexpr SkColor4f kGround{0.06f, 0.06f, 0.075f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.06f, 0.06f, 0.075f, 1};
  look.palette.cellGround = {0.085f, 0.09f, 0.105f, 1};
  return look;
}

/** One cube face: a flat ground under a bar and a disc, in the face's own
 *  colour, so the resample and the unpack can be told apart by eye. */
sk_sp<SkImage> face(SkColor4f tint, float bar) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kFaceSide, kFaceSide));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(tint.toSkColor());
  SkPaint mark;
  mark.setAntiAlias(true);
  mark.setColor4f({1, 1, 1, 0.85f});
  canvas->drawRect(
      SkRect::MakeXYWH(0, kFaceSide * bar, kFaceSide, kFaceSide * 0.10f), mark);
  mark.setStyle(SkPaint::kStroke_Style);
  mark.setStrokeWidth(kFaceSide * 0.08f);
  mark.setColor4f({tint.fR * 0.45f, tint.fG * 0.45f, tint.fB * 0.45f, 1});
  canvas->drawCircle(kFaceSide * 0.5f, kFaceSide * 0.5f, kFaceSide * 0.24f,
                     mark);
  return surface->makeImageSnapshot();
}

/** The six faces in the order every graphics API names them: +x, -x, +y,
 *  -y, +z, -z, each looking outward with +y up. */
material::EnvironmentMap::Faces faces() {
  return {
      face({0.86f, 0.32f, 0.28f, 1}, 0.15f),  // +x
      face({0.30f, 0.52f, 0.88f, 1}, 0.30f),  // -x
      face({0.94f, 0.86f, 0.40f, 1}, 0.45f),  // +y
      face({0.58f, 0.61f, 0.68f, 1}, 0.60f),  // -y
      face({0.36f, 0.80f, 0.56f, 1}, 0.75f),  // +z
      face({0.72f, 0.44f, 0.86f, 1}, 0.86f),  // -z
  };
}

/** The same six laid into one 6:1 row — the layout `fromCubeMap` names
 *  by aspect ratio, in the same +x -x +y -y +z -z order. */
sk_sp<SkImage> row(const material::EnvironmentMap::Faces& six) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kFaceSide * 6, kFaceSide));
  for (int i = 0; i < 6; ++i)
    surface->getCanvas()->drawImage(six[(size_t)i], (float)(i * kFaceSide), 0);
  return surface->makeImageSnapshot();
}

/** The disc every reflective cell shades, and the normals under it. */
SkPath disc() {
  return SkPathBuilder()
      .addCircle(kCell * 0.5f, kPicture * 0.5f, kPicture * 0.40f)
      .detach();
}

material::Texture shoulder() { return material::bevelNormals(disc(), kBevel); }

sketch::kit::ComparisonCase cell(
    const char* caseTitle, const char* call, const std::string& note,
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

/** The panorama itself, fitted into the cell. */
sketch::kit::ComparisonCase panorama(
    const char* caseTitle, const char* call, const std::string& note,
    const material::EnvironmentMap& environment, float roughness = 0) {
  return cell(
      caseTitle, call, note,
      [environment, roughness](SkCanvas& canvas, const material::FrameData&) {
        const sk_sp<SkImage> image = environment.image(roughness);
        if (!image) return;
        const float w = kCell - 16;
        const float h = w * 0.5f;
        canvas.drawImageRect(image,
                             SkRect::MakeXYWH(8, (kPicture - h) * 0.5f, w, h),
                             SkSamplingOptions(SkFilterMode::kLinear));
      });
}

/** A chrome disc reflecting the panorama. */
sketch::kit::ComparisonCase reflector(
    const char* caseTitle, const char* call, const std::string& note,
    const material::EnvironmentMap& environment, float roughness = 0) {
  // The face is captured BY VALUE: this program is invoked at paint time,
  // long after the frame that described it.
  return cell(
      caseTitle, call, note,
      [paint = material::kit::chrome(
           shoulder(), environment, {.roughness = roughness, .contrast = 1.5f}),
       face = disc()](SkCanvas& canvas, const material::FrameData& frame) {
        material::skia::fill(canvas, face, paint, frame);
      });
}

}  // namespace

struct EnvFaces {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const material::EnvironmentMap studio =
        material::kit::studioEnvironment(384);
    // The six faces are baked ONCE and read twice — once as faces, once
    // laid into the 6:1 row the cube-map layout is named by.
    const material::EnvironmentMap::Faces six = faces();
    const material::EnvironmentMap resampled =
        material::EnvironmentMap::fromFaces(six);
    const material::EnvironmentMap unpacked =
        material::EnvironmentMap::fromCubeMap(row(six));
    const material::EnvironmentMap rewrapped =
        material::EnvironmentMap::fromEquirectangular(resampled.image(0));
    const material::EnvironmentMap grounded =
        resampled.withGround(kGroundColour);
    const SkColor4f mean = resampled.average();

    ctx.composer.render(sketch::kit::page(
        {.title = "From a sky to a reflection",
         .subtitle = "Three source conditions, one reflective surface",
         .footer = "The reflection pairs preserve the same disc, normal map "
                   "and view; only the panorama changes."},
        box().column().gap(24).children(
            {sketch::kit::sectionHeader(
                 {.label = "PANORAMAS",
                  .note = "Source conditions read across; their reflections "
                          "read directly below."}),
             sketch::kit::comparison(
                 {.cases =
                      {panorama("STUDIO", "kit::studioEnvironment(384)",
                                "Procedural studio: sky, floor bounce and "
                                "three softboxes.",
                                studio),
                       panorama(
                           "SIX FACES", "fromFaces(six)",
                           "Six directional faces resampled into one panorama.",
                           resampled),
                       panorama(
                           "GROUND REPLACEMENT", "resampled.withGround(warm)",
                           "The lower hemisphere is replaced before filtering.",
                           grounded)},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::sectionHeader(
                 {.label = "THE SAME BEVEL", .note = ""}),
             sketch::kit::comparison(
                 {.cases =
                      {reflector("SOFTBOXES", "kit::chrome(bevel, studio)",
                                 "A bevel normal and the studio panorama shade "
                                 "this disc.",
                                 studio),
                       reflector(
                           "DIRECTIONAL COLOUR",
                           "kit::chrome(bevel, fromFaces)",
                           "The same bevel reflects the coloured cube faces.",
                           resampled),
                       reflector(
                           "WARM LOWER HEMISPHERE",
                           "kit::chrome(bevel, withGround)",
                           "The warm lower hemisphere is visible in the rim.",
                           grounded)},
                  .measure = 1020,
                  .gap = 18}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(18)
                 .children(
                     {box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "IMPORT AND FILTER", .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {panorama("ALTERNATE PACKING",
                                                   "fromCubeMap(6:1 row)",
                                                   "The same six faces "
                                                   "unpacked from a 6:1 strip.",
                                                   unpacked),
                                          reflector("ROUGHNESS 0.45",
                                                    "…"
                                                    " at roughness 0.45",
                                                    "Roughness selects a "
                                                    "wider, wrap-aware blur.",
                                                    resampled, 0.45f)},
                                .measure = 674,
                                .gap = 18})}),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "ONE INTERNAL FORM", .note = ""}),
                           document::caption(
                               "Every input becomes an equirectangular "
                               "panorama: azimuth across the image, zenith at "
                               "the top. Import layout is resolved once, "
                               "before shading.")
                               .width(328),
                           document::caption(
                               kit::formatted(
                                   "Mean radiance\nR %.2f  G %.2f  B %.2f",
                                   (double)mean.fR, (double)mean.fG,
                                   (double)mean.fB))
                               .width(328)})})})));
  }
};

SIGIL_SKETCH(EnvFaces, "Kit · API",
             "four ways into one equirect panorama, and the same chrome "
             "disc reflecting each of them")
