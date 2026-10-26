/** @file
 * environment_maps — the doors into one panorama, worn by one World
 * body, then the environment's dials and the camera's exposure.
 *
 * `EnvironmentMap` is the panorama a surface sees when it looks past the
 * lights, and it has ONE internal form: equirectangular, u = azimuth,
 * v = 0 at the zenith. Every source resolves into that form while the
 * value is built — a radiance function baked with no assets (the studio
 * and the sunset here), `fromEquirectangular()` wrapping a loaded lat-long
 * panorama, `fromFaces()` resampling six cube faces, and `fromCubeMap()`
 * unpacking one sheet (a 4:3 or 3:4 cross, a 6:1 row or a 1:6 column).
 * `withGround(colour)` replaces everything below the horizon IN the
 * panorama, so the blurs and the irradiance see it too — what a
 * photographed sky wants when its lower half is a tripod and a car park.
 * The six faces here are drawn by the texture door and read twice: once
 * as faces, once laid into the 6:1 row the cube-map layout is named by.
 *
 * A frame holds ONE environment, so a sheet that compares them cannot be
 * one scene: each cell is its own frame, baked through
 * `Scene2DContext::bakeScene3D` at the size the cell will have and
 * painted as the picture inside this page.
 *
 * The dials divide cleanly. `diffuse` is how much of the map reaches a
 * surface as the light falling on it from everywhere and `specular` is
 * how much of it a surface mirrors — pushing one and not the other is a
 * LOOK and not a physical claim. `roughnessBias` is added to every
 * surface's roughness before it picks a prefiltered level, so a whole
 * scene softens without a material being edited. `crossfade` mixes a
 * second panorama over the first, both sampled rather than one rebuilt,
 * which is what lets a sky change while the frame is running. `shown` is
 * the sky SHOWN behind the scene at that strength, blurred in the same
 * roughness units a reflection reads. `exposure` is the camera's: what
 * every radiance is multiplied by before the tone curve, so doubling it
 * is one stop.
 *
 * EDIT THESE FIRST
 *   kFaceSide — the resolution each cube face is drawn at.
 *   kGround   — the colour withGround() replaces the lower half with.
 *   kBias     — the roughness added to every surface in its cell.
 *   kShown, kBlur — how much sky is shown, and how soft.
 */

// TAGS: Materials/Lighting

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <array>
#include <string>
#include <utility>

#include "shapeworks_lab/Environments.h"

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace shapes = sigil::geometry::shapes;
namespace mesh = sigil::geometry::mesh;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1350};
constexpr float kDoor = 240;      // a door cell's width
constexpr float kDoorTall = 180;  // …and its picture's height
constexpr float kDial = 328;      // a dial cell's width
constexpr float kDialTall = 236;  // …and its picture's height

constexpr int kFaceSide = 128;  // each cube face's resolution
constexpr material::Color kGround{0.30f, 0.20f, 0.10f, 1};
constexpr float kBias = 0.45f;  // roughness added to every surface
constexpr float kShown = 1.0f;  // how much sky is shown
constexpr float kBlur = 0.35f;  // …and how soft

constexpr material::Color kCellGround{0.06f, 0.065f, 0.08f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
  look.palette.cellGround = kCellGround;
  return look;
}

/** ONE CUBE FACE as a tree: a flat ground under a bar and a ring, in the
 *  face's own colour, so the resample and the unpack can be told apart by
 *  eye. */
Element face(material::Color tint, float bar) {
  const float side = (float)kFaceSide;
  const material::Color shade{tint.r * 0.45f, tint.g * 0.45f, tint.b * 0.45f,
                              1};
  return box()
      .width(side)
      .height(side)
      .fill(Fill::color(tint))
      .children(
          {kit::at(box().fill(Fill::color({1, 1, 1, 0.85f})), 0, side * bar,
                   side, side * 0.10f),
           kit::at(box()
                       .shape(shapes::circle())
                       .stroke(stroke(side * 0.08f, Fill::color(shade))),
                   side * 0.26f, side * 0.26f, side * 0.48f, side * 0.48f)});
}

/** The six faces in the order every graphics API names them: +x, -x, +y,
 *  -y, +z, -z, each looking outward with +y up. */
std::array<Element, 6> faceTrees() {
  return {face({0.86f, 0.32f, 0.28f, 1}, 0.15f),   // +x
          face({0.30f, 0.52f, 0.88f, 1}, 0.30f),   // -x
          face({0.94f, 0.86f, 0.40f, 1}, 0.45f),   // +y
          face({0.58f, 0.61f, 0.68f, 1}, 0.60f),   // -y
          face({0.36f, 0.80f, 0.56f, 1}, 0.75f),   // +z
          face({0.72f, 0.44f, 0.86f, 1}, 0.86f)};  // -z
}

/** The subject every cell bakes: one near-mirror body over a matte slab,
 *  so a reflection has something to be read against. */
world::Group subject() {
  return world::group(
             {world::mesh(mesh::superellipsoid({46, 46, 46}, 2.0f, 40, 26))
                  .key("body")
                  .at({0, 40, 0})
                  .fill(material::surface::program(
                      {.baseColor = {0.85f, 0.86f, 0.88f, 1},
                       .metallic = 1.0f,
                       .roughness = 0.12f})),
              world::mesh(mesh::superellipsoid({150, 12, 150}, 8.0f, 20, 10))
                  .key("slab")
                  .at({0, -14, 0})
                  .fill(material::surface::program(
                      {.baseColor = {0.20f, 0.21f, 0.24f, 1},
                       .roughness = 0.65f}))})
      .key("subject");
}

mesh::camera::Camera lens() {
  mesh::camera::Camera camera;
  camera.eye = {0, 78, 250};
  camera.target = {0, 30, 0};
  camera.up = {0, 1, 0};
  camera.verticalFieldOfViewDegrees = 40;
  camera.nearPlane = 4;
  camera.farPlane = 4000;
  return camera;
}

/** A door cell: the panorama itself, two to one, over the body wearing
 *  it. */
sketch::kit::ComparisonCase door(const char* title, const char* call,
                                 const char* note,
                                 const material::EnvironmentMap& panorama,
                                 sk_sp<SkImage> baked) {
  return {
      .title = title,
      .control = call,
      .figure = box().column().gap(8).children(
          {image(panorama.texture(0).source(), material::Fit::Stretch)
               .width(kDoor)
               .height(kDoor / 2),
           sketch::kit::well({.width = kDoor, .height = kDoorTall},
                             image(std::move(baked), material::Fit::Stretch))}),
      .note = note};
}

/** A dial cell: the bake IS the well's surface. */
sketch::kit::ComparisonCase dial(const char* title, const char* call,
                                 const char* note, sk_sp<SkImage> baked) {
  return {.title = title,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kDial, .height = kDialTall},
              image(std::move(baked), material::Fit::Stretch)),
          .note = note};
}

}  // namespace

struct EnvironmentMaps {
  /** Every cell is a scene baked on the device, so the sheet needs one and
   *  is unavailable, saying so, where none is installed. */
  static constexpr bool needsDevice = true;
  static bool available(std::string* why) {
    return sigil::sketch::runtimeAvailable(why);
  }

  void setup(sketch::Scene2DContext& ctx) {
    // every bake has already been taken
    sketch::kit::stage(
        ctx, {.theme = sheetTheme(), .size = kCanvas, .captureAt = 0.05});

    const material::EnvironmentMap studio = shapeworks_lab::studioEnvironment();
    const material::EnvironmentMap sunset = shapeworks_lab::sunsetEnvironment();

    // The six faces are drawn ONCE each and read twice — as faces, and
    // laid into the 6:1 row the cube-map layout is named by.
    const std::array<Element, 6> trees = faceTrees();
    material::EnvironmentMap::Faces six;
    for (size_t index = 0; index < trees.size(); ++index)
      six[index] = ctx.texture(trees[index], {kFaceSide, kFaceSide}).source();
    const material::EnvironmentMap resampled =
        material::EnvironmentMap::fromFaces(six);
    const material::EnvironmentMap unpacked =
        material::EnvironmentMap::fromCubeMap(
            ctx.texture(box().row().children({trees[0], trees[1], trees[2],
                                              trees[3], trees[4], trees[5]}),
                        {kFaceSide * 6, kFaceSide})
                .source());
    const material::EnvironmentMap grounded = resampled.withGround(kGround);

    /** One frame: the subject under @p panorama and @p dials, seen
     *  through a camera at @p exposure, at the size of the cell it fills. */
    const auto bake = [&](const material::EnvironmentMap& panorama, SkSize cell,
                          world::EnvironmentOptions dials = {},
                          float exposure = 1.0f) {
      world::Frame frame(
          world::group(
              {world::camera(lens(), {.exposure = exposure}), subject()})
              .key("set"));
      frame.environment(panorama, std::move(dials));
      return ctx.bakeScene3D(frame, {(int)cell.width(), (int)cell.height()},
                             {.camera = lens(), .background = kCellGround});
    };
    const SkSize doorCell{kDoor, kDoorTall};
    const SkSize dialCell{kDial, kDialTall};

    ctx.composer.render(sketch::kit::page(
        {.title = "From a sky to a reflection",
         .subtitle = "Four doors into one panorama, worn by one body; then "
                     "the dials that read it.",
         .footer = "Every picture is a separate world frame. The mesh, "
                   "camera and surface parameters never change."},
        box().column().gap(26).children(
            {sketch::kit::section(
                 {.label = "01  FOUR DOORS",
                  .note = "The panorama above, the chrome body wearing it "
                          "below"},
                 sketch::kit::comparison(
                     {.cases =
                          {door("BAKED", "EnvironmentMap::baked(studio)",
                                "A radiance function: sky, floor bounce and "
                                "three softboxes, with no assets.",
                                studio, bake(studio, doorCell)),
                           door("SIX FACES", "fromFaces(six)",
                                "Six coloured faces resampled into one "
                                "panorama.",
                                resampled, bake(resampled, doorCell)),
                           door("ONE SHEET", "fromCubeMap(6:1 row)",
                                "The same six faces unpacked from one strip, "
                                "named by its aspect.",
                                unpacked, bake(unpacked, doorCell)),
                           door("GROUND REPLACED", "withGround(warm)",
                                "The lower hemisphere replaced before "
                                "filtering; the slab's floor reads it.",
                                grounded, bake(grounded, doorCell))},
                      .measure = 1020,
                      .gap = 18})),
             sketch::kit::section(
                 {.label = "02  SURFACE RESPONSE",
                  .note = "The studio · one stop brighter · rougher "
                          "everywhere"},
                 sketch::kit::comparison(
                     {.cases =
                          {dial("THE REFERENCE", "studio · exposure 1",
                                "Near-mirror metal on a matte slab, lit only "
                                "by the studio.",
                                bake(studio, dialCell)),
                           dial("ONE STOP BRIGHTER", "exposure = 2",
                                "Double the radiance before the tone curve.",
                                bake(studio, dialCell, {}, 2.0f)),
                           dial("ROUGHER EVERYWHERE", "roughnessBias = 0.45",
                                "A bias softens every reflection without "
                                "editing the materials.",
                                bake(studio, dialCell,
                                     {.roughnessBias = kBias}))},
                      .measure = 1020,
                      .gap = 18})),
             sketch::kit::section(
                 {.label = "03  THE ENVIRONMENT AROUND IT",
                  .note = "The light's two parts, the next sky, and the sky "
                          "shown behind"},
                 sketch::kit::comparison(
                     {.cases =
                          {dial("DIFFUSE / SPECULAR",
                                "diffuse .15 · specular 2",
                                "A stronger reflection over a reduced "
                                "diffuse bounce.",
                                bake(studio, dialCell,
                                     {.diffuse = 0.15f, .specular = 2.0f})),
                           dial("CROSSFADE TO SUNSET",
                                "crossfade 0.75 to sunset",
                                "The next panorama contributes 75% of the "
                                "illumination.",
                                bake(studio, dialCell,
                                     {.next = sunset, .crossfade = 0.75f})),
                           dial("SHOW THE SKY", "shown 1.0 · blur 0.35",
                                "The sky becomes visible behind the set, "
                                "with its own blur.",
                                bake(studio, dialCell,
                                     {.shown = kShown, .blur = kBlur}))},
                      .measure = 1020,
                      .gap = 18}))})));
  }
};

SIGIL_SKETCH(EnvironmentMaps, "Kit · API",
             "four doors into one panorama — baked, six faces, one cube "
             "sheet, a replaced ground — each worn by one chrome body, then "
             "the same body under the environment's dials and the camera's "
             "exposure")
