/** @file
 * env_lanes — the environment's dials and the camera's exposure, one
 * sphere, six bakes.
 *
 * A frame holds ONE environment, so a sheet that compares its dials
 * cannot be one scene: each cell is its own frame, baked through
 * `Scene2DContext::bakeScene3D` at the size the cell will have and painted
 * as the picture INSIDE this page.
 *
 * The dials divide cleanly. `diffuse` is how much of the map reaches a
 * surface as the light falling on it from everywhere and `specular` is
 * how much of it a surface mirrors — pushing one and not the other is a
 * LOOK and not a physical claim. `roughnessBias` is added to every
 * surface's roughness before it picks a prefiltered level, so a whole scene
 * softens without a material being edited. `crossfade` mixes a second
 * panorama over the first, both sampled rather than one rebuilt, which is
 * what lets a sky change while the frame is running. `shown` is the sky
 * SHOWN behind the scene at that strength — zero draws none of it, so the
 * dial is also the switch — blurred in the same roughness units a
 * reflection reads.
 *
 * `exposure` is the camera's, not the environment's: what every radiance
 * is multiplied by before the tone curve compresses it, so doubling it is
 * one stop. It means something in a scene carrying no panorama at all,
 * because a lit sum ends at that curve either way.
 *
 * EDIT THESE FIRST
 *   kExposure — the stop the reference cell is read at.
 *   kBias — the roughness added to every surface in its cell.
 *   kShown, kBlur — how much sky is shown, and how soft.
 */

// TAGS: Materials/Lighting

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
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

#include <string>
#include <utility>

#include "shapeworks_lab/Environments.h"

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace gm = sigil::geometry::mesh;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 940};
constexpr float kCell = 328;
constexpr float kPicture = 236;

constexpr float kExposure = 1.0f;  // the stop the reference is read at
constexpr float kBias = 0.45f;     // roughness added to every surface
constexpr float kShown = 1.0f;     // how much sky is shown
constexpr float kBlur = 0.35f;     // …and how soft

constexpr material::Color kCellGround{0.06f, 0.065f, 0.08f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
  look.palette.cellGround = {0.06f, 0.065f, 0.08f, 1};
  return look;
}

/** The subject every cell bakes: one near-mirror body over a matte
 *  floor, so a reflection has something to be read against. */
world::Group subject() {
  return world::group(
             {world::mesh(gm::superellipsoid({46, 46, 46}, 2.0f, 40, 26))
                  .key("body")
                  .at({0, 40, 0})
                  .fill(material::surface::program(
                      {.baseColor = {0.85f, 0.86f, 0.88f, 1},
                       .metallic = 1.0f,
                       .roughness = 0.12f})),
              world::mesh(gm::superellipsoid({150, 12, 150}, 8.0f, 20, 10))
                  .key("slab")
                  .at({0, -14, 0})
                  .fill(material::surface::program(
                      {.baseColor = {0.20f, 0.21f, 0.24f, 1},
                       .roughness = 0.65f}))})
      .key("subject");
}

gm::camera::Camera lens() {
  gm::camera::Camera camera;
  camera.eye = {0, 78, 250};
  camera.target = {0, 30, 0};
  camera.up = {0, 1, 0};
  camera.fovYDegrees = 40;
  camera.nearPlane = 4;
  camera.farPlane = 4000;
  return camera;
}

/** One cell: the bake IS the well's surface, sized and grounded by the
 *  well itself, so nothing here places a picture inside a plate. */
sketch::kit::ComparisonCase cell(const char* caseTitle, const char* call,
                                 const char* note, sk_sp<SkImage> baked) {
  Element picture = image(std::move(baked), material::Fit::Stretch);
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well({.width = kCell, .height = kPicture},
                                      std::move(picture)),
          .note = note};
}

}  // namespace

struct EnvLanes {
  /** Every cell is a scene baked on the device, so the sheet needs one and
   *  is unavailable, saying so, where none is installed. */
  static constexpr bool needsDevice = true;
  static bool available(std::string* why) {
    return sigil::sketch::runtimeAvailable(why);
  }

  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // every bake has already been taken
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const world::EnvironmentMap studio = shapeworks_lab::studioEnvironment();
    const world::EnvironmentMap sunset = shapeworks_lab::sunsetEnvironment();

    /** One frame: the subject in the studio under the cell's own dials,
     *  seen through a camera at the cell's own exposure, and nothing else
     *  different. */
    const auto bake = [&](world::EnvironmentOptions dials,
                          float exposure = kExposure) {
      world::Frame frame(
          world::group(
              {world::camera(lens(), {.exposure = exposure}), subject()})
              .key("set"));
      frame.environment(studio, std::move(dials));
      return ctx.bakeScene3D(frame, {(int)kCell, (int)kPicture},
                             {.camera = lens(), .background = kCellGround});
    };

    const world::EnvironmentOptions base;
    const world::EnvironmentOptions softened{.roughnessBias = kBias};
    const world::EnvironmentOptions mirrored{.diffuse = 0.15f,
                                             .specular = 2.0f};
    const world::EnvironmentOptions mixed{.next = sunset, .crossfade = 0.75f};
    const world::EnvironmentOptions shown{.shown = kShown, .blur = kBlur};

    ctx.composer.render(sketch::kit::page(
        {.title = "One room, six readings",
         .subtitle =
             "A fixed camera and material reveal the environment controls",
         .footer = "Every image is a separate world frame. The mesh, camera "
                   "and surface parameters remain fixed."},
        box().column().gap(26).children(
            {sketch::kit::section(
                 {.label = "SURFACE RESPONSE",
                  .note = "Reference · brightness · reflection softness"},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("THE REFERENCE", "studio() · exposure 1",
                                "Near-mirror metal on a matte slab, lit only "
                                "by the studio.",
                                bake(base)),
                           cell("ONE STOP BRIGHTER", "exposure = 2",
                                "Double the radiance before the tone curve.",
                                bake(base, kExposure * 2.0f)),
                           cell("ROUGHER EVERYWHERE", "roughnessBias = 0.45",
                                "A bias softens every reflection without "
                                "editing the materials.",
                                bake(softened))},
                      .measure = 1020,
                      .gap = 18})),
             sketch::kit::section(
                 {.label = "THE ENVIRONMENT AROUND IT",
                  .note = "Separate the light contribution, the next sky, and "
                          "the sky shown behind."},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("DIFFUSE / SPECULAR", "diffuse .15 specular 2",
                                "A stronger reflection over a reduced diffuse "
                                "bounce.",
                                bake(mirrored)),
                           cell("CROSSFADE TO SUNSET",
                                "crossfade 0.75 to sunset",
                                "The next panorama contributes 75% of the "
                                "illumination.",
                                bake(mixed)),
                           cell("SHOW THE SKY", "shown 1.0 blur 0.35",
                                "The sky becomes visible behind the set, with "
                                "its own blur.",
                                bake(shown))},
                      .measure = 1020,
                      .gap = 18}))})));
  }
};

SIGIL_SKETCH(EnvLanes, "Kit · API",
             "one chrome body baked six times under the environment's own "
             "dials and the camera's exposure — the stop, the roughness bias, "
             "the two sides of the map, the crossfade and the shown sky")
