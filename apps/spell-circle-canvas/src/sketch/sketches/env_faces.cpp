/** @file
 * env_faces — the four doors into one panorama, and what a reflective
 * surface sees through each.
 *
 * `EnvironmentMap` is the panorama a surface sees when it looks past the
 * lights, and it has ONE internal form: equirectangular, u = azimuth,
 * v = 0 at the zenith. Every source resolves into that form while the
 * value is built — `studio()` and `sunset()` bake one with no assets,
 * `fromEquirect()` wraps a loaded lat-long panorama, `fromFaces()`
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

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/kit/Environments.h>
#include <sigilmaterial/kit/Surfaces.h>
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
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 654};
constexpr float kCell = 252;
constexpr float kPicture = 196;

constexpr int kFaceSide = 128;  // each cube face's resolution
constexpr float kBevel = 30;    // the disc's shoulder, px
constexpr SkColor4f kGroundColour{0.14f, 0.12f, 0.10f, 1};

constexpr SkColor4f kGround{0.06f, 0.06f, 0.075f, 1};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
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
const material::EnvironmentMap::Faces& faces() {
  static const material::EnvironmentMap::Faces six = {
      face({0.86f, 0.32f, 0.28f, 1}, 0.15f),  // +x
      face({0.30f, 0.52f, 0.88f, 1}, 0.30f),  // -x
      face({0.94f, 0.86f, 0.40f, 1}, 0.45f),  // +y
      face({0.58f, 0.61f, 0.68f, 1}, 0.60f),  // -y
      face({0.36f, 0.80f, 0.56f, 1}, 0.75f),  // +z
      face({0.72f, 0.44f, 0.86f, 1}, 0.86f),  // -z
  };
  return six;
}

/** The same six laid into one 6:1 row — the layout `fromCubeMap` names
 *  by aspect ratio, in the same +x -x +y -y +z -z order. */
sk_sp<SkImage> row() {
  static const sk_sp<SkImage> sheet = [] {
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(kFaceSide * 6, kFaceSide));
    for (int i = 0; i < 6; ++i)
      surface->getCanvas()->drawImage(faces()[(size_t)i],
                                      (float)(i * kFaceSide), 0);
    return surface->makeImageSnapshot();
  }();
  return sheet;
}

/** The disc every reflective cell shades, and the normals under it. */
SkPath disc() {
  static const SkPath path =
      SkPathBuilder()
          .addCircle(kCell * 0.5f, kPicture * 0.5f, kPicture * 0.40f)
          .detach();
  return path;
}

material::Texture shoulder() {
  static const material::Texture map = material::bevelNormals(disc(), kBevel);
  return map;
}

Element cell(const char* call, const std::string& note,
             std::function<void(SkCanvas&, const material::FrameData&)> draw) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well(
          {.width = kCell, .height = kPicture},
          custom(call, [draw = std::move(draw)](SkCanvas& canvas,
                                                const PaintContext& pc) {
            draw(canvas, {.resolution = {pc.size.width(), pc.size.height()}});
          })));
}

/** The panorama itself, fitted into the cell. */
Element panorama(const char* call, const std::string& note,
                 const material::EnvironmentMap& env, float roughness = 0) {
  return cell(call, note,
              [env, roughness](SkCanvas& canvas, const material::FrameData&) {
                const sk_sp<SkImage> image = env.image(roughness);
                if (!image) return;
                const float w = kCell - 16;
                const float h = w * 0.5f;
                canvas.drawImageRect(
                    image, SkRect::MakeXYWH(8, (kPicture - h) * 0.5f, w, h),
                    SkSamplingOptions(SkFilterMode::kLinear));
              });
}

/** A chrome disc reflecting the panorama. */
Element reflector(const char* call, const std::string& note,
                  const material::EnvironmentMap& env, float roughness = 0) {
  material::kit::ChromeParams params;
  params.roughness = roughness;
  params.contrast = 1.5f;
  return cell(call, note,
              [paint = material::kit::chrome(shoulder(), env, params)](
                  SkCanvas& canvas, const material::FrameData& frame) {
                material::skia::fill(canvas, disc(), paint, frame);
              });
}

}  // namespace

struct EnvFaces final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    material::skia::install();  // the SkSL compiler, once per process

    const material::EnvironmentMap studio =
        material::kit::studioEnvironment(384);
    const material::EnvironmentMap resampled =
        material::EnvironmentMap::fromFaces(faces());
    const material::EnvironmentMap unpacked =
        material::EnvironmentMap::fromCubeMap(row());
    const material::EnvironmentMap rewrapped =
        material::EnvironmentMap::fromEquirect(resampled.image(0));
    const material::EnvironmentMap grounded =
        resampled.withGround(kGroundColour);
    const SkColor4f mean = resampled.average();

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("ENVIRONMENT FACES \xc2\xb7 EnvironmentMap "
                       "studio, fromFaces, fromCubeMap, fromEquirect, "
                       "withGround"),
         .subtitle = toU8("dials \xc2\xb7 the face set (six baked here) "
                          "\xc2\xb7 the ground colour \xc2\xb7 the "
                          "roughness the reflection reads the panorama "
                          "at"),
         .footer = toU8("one internal form, four ways in: u is azimuth, "
                        "v is 0 at the zenith, and every source is "
                        "resampled into that while the value is built "
                        "rather than at each lookup")},
        kit::cells(
            {.cells =
                 {kit::cells(
                      {.cells =
                           {panorama("kit::studioEnvironment(384)",
                                     "baked with no assets \xc2\xb7 a "
                                     "graded sky, a floor bounce and "
                                     "three softboxes",
                                     studio),
                            panorama(
                                "fromFaces(six)",
                                kit::format(
                                    "six cube faces resampled into "
                                    "one equirect \xc2\xb7 average "
                                    "(%.2f %.2f %.2f)",
                                    (
                                        double)mean.fR,
                                    (
                                        double)mean.fG,
                                    (
                                        double)mean.fB),
                                resampled),
                            panorama(
                                "fromCubeMap(6:1 row)",
                                "the SAME six as one sheet, "
                                "unpacked by aspect ratio \xc2\xb7 "
                                "the layout is read, never "
                                "declared",
                                unpacked),
                            panorama(
                                "resampled.withGround(warm)",
                                "everything below the horizon "
                                "replaced IN the panorama, so the "
                                "blurs and the irradiance see it "
                                "too",
                                grounded)},
                       .gap =
                           14}),
                  kit::cells(
                      {.cells =
                           {reflector(
                                "kit::chrome(bevel, studio)",
                                "the two textures a reflective "
                                "surface is shaded from: a normal "
                                "map at the outline's bounds and "
                                "a panorama",
                                studio),
                            reflector(
                                "kit::chrome(bevel, fromFaces)",
                                "the same disc, the same "
                                "normals \xc2\xb7 the six faces "
                                "are legible in the rim because "
                                "the rim looks sideways",
                                resampled),
                            reflector(
                                "\xe2\x80\xa6"
                                " at roughness 0.45",
                                "image(roughness) is one of nine "
                                "wrap-aware blurs, picked by how "
                                "rough the surface says it is",
                                resampled,
                                0.45f),
                            reflector(
                                "kit::chrome(bevel, withGround)",
                                "the same reflection over a "
                                "panorama whose lower half is one "
                                "colour \xc2\xb7 which is what a "
                                "car park is replaced with",
                                grounded)},
                       .gap =
                           14})},
             .column = true,
             .gap = 18})));
  }
};

SIGIL_SKETCH(EnvFaces, "Kit \xc2\xb7 API",
             "four ways into one equirect panorama, and the same chrome "
             "disc reflecting each of them")
