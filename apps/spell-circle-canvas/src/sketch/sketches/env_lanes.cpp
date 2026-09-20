/** @file
 * env_lanes — the dials on the environment node, one sphere, six bakes.
 *
 * A frame holds ONE environment node, so a sheet that compares its dials
 * cannot be one set: each cell is its own frame, baked through
 * `SketchContext::bakeSet` at the pixels the cell will have and painted
 * as the picture INSIDE this page.
 *
 * The dials divide cleanly. `diffuse` is how much of the map reaches a
 * surface as the light falling on it from everywhere and `specular` is
 * how much of it a surface mirrors — pushing one and not the other is a
 * LOOK and not a physical claim. `roughnessBias` is added to every
 * surface's roughness before it picks a prefiltered level, so a whole set
 * softens without a material being edited. `crossfade` mixes a second
 * panorama over the first, both sampled rather than one rebuilt, which is
 * what lets a sky change while the frame is running. `backdrop` is the
 * sky SHOWN behind the set at that strength — zero draws none of it, so
 * the dial is also the switch — blurred in the same roughness units a
 * reflection reads.
 *
 * `exposure` is the odd one: what every radiance is multiplied by before
 * the tone curve compresses it, so doubling it is one stop. It is the one
 * dial here that means something in a set carrying no panorama at all,
 * because a lit sum ends at that curve either way.
 *
 * EDIT THESE FIRST
 *   kExposure — the stop the reference cell is read at.
 *   kBias — the roughness added to every surface in its cell.
 *   kBackdrop, kBlur — how much sky is shown, and how soft.
 */

// TAGS: Materials/Lighting

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/kit/Environments.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/element/Environment.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/kit/Kit.h>

#include <string>
#include <utility>

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
constexpr float kBackdrop = 1.0f;  // how much sky is shown
constexpr float kBlur = 0.35f;     // …and how soft

constexpr SkColor4f kCellGround{0.06f, 0.065f, 0.08f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.06f, 0.065f, 0.08f, 1};
  return look;
}

/** The subject every cell bakes: one near-mirror body over a matte
 *  floor, so a reflection has something to be read against. */
world::Element subject() {
  world::Element set;
  set.key("subject").children(
      {world::Element()
           .key("body")
           .at({0, 40, 0})
           .mesh(gm::superellipsoid({46, 46, 46}, 2.0f, 40, 26))
           .fill(material::kit::surface({.baseColor = {0.85f, 0.86f, 0.88f, 1},
                                         .metallic = 1.0f,
                                         .roughness = 0.12f})),
       world::Element()
           .key("slab")
           .at({0, -14, 0})
           .mesh(gm::superellipsoid({150, 12, 150}, 8.0f, 20, 10))
           .fill(material::kit::surface(
               {.baseColor = {0.20f, 0.21f, 0.24f, 1}, .roughness = 0.65f}))});
  return set;
}

gm::camera::Camera lens() {
  gm::camera::Camera camera;
  camera.eye = {0, 78, 250};
  camera.target = {0, 30, 0};
  camera.up = {0, 1, 0};
  camera.fovYDeg = 40;
  camera.zNear = 4;
  camera.zFar = 4000;
  return camera;
}

/** One cell: the bake IS the well's surface, sized and grounded by the
 *  well itself, so nothing here places a picture inside a plate. */
sketch::kit::ComparisonCase cell(const char* caseTitle, const char* call,
                                 const char* note, sk_sp<SkImage> baked) {
  Element picture = image(std::move(baked), material::skia::Fit::Stretch);
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well({.width = kCell, .height = kPicture},
                                      std::move(picture)),
          .note = note};
}

}  // namespace

struct EnvLanes {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // every bake has already been taken
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const material::EnvironmentMap studio = material::kit::studioEnvironment();
    const material::EnvironmentMap sunset = material::kit::sunsetEnvironment();

    /** One frame: the subject under an environment node carrying the
     *  cell's own dials, and nothing else different. */
    const auto bake = [&](world::Environment environment) {
      world::Element root;
      root.key("set").children(
          {world::Element().key("sky").environmentMap(std::move(environment)),
           subject()});
      return ctx.bakeSet(world::Frame(std::move(root)), lens(),
                         {(int)kCell, (int)kPicture}, kCellGround);
    };

    world::Environment base;
    base.map = studio;
    base.exposure = kExposure;

    world::Environment brighter = base;
    brighter.exposure = kExposure * 2.0f;

    world::Environment softened = base;
    softened.roughnessBias = kBias;

    world::Environment mirrored = base;
    mirrored.diffuse = 0.15f;
    mirrored.specular = 2.0f;

    world::Environment mixed = base;
    mixed.next = sunset;
    mixed.crossfade = 0.75f;

    world::Environment shown = base;
    shown.backdrop.intensity = kBackdrop;
    shown.backdrop.blur = kBlur;

    ctx.composer.render(sketch::kit::page(
        {.title = "One room, six readings",
         .subtitle =
             "A fixed camera and material reveal the environment controls",
         .footer = "Every image is a separate world frame. The mesh, camera "
                   "and surface parameters remain fixed."},
        box().column().gap(26).children(
            {sketch::kit::sectionHeader(
                 {.label = "SURFACE RESPONSE",
                  .note = "Reference · brightness · reflection softness"}),
             sketch::kit::comparison(
                 {.cases = {cell("THE REFERENCE", "studio() · exposure 1",
                                 "Near-mirror metal on a matte slab, lit only "
                                 "by the studio.",
                                 bake(base)),
                            cell("ONE STOP BRIGHTER", "exposure = 2",
                                 "Double the radiance before the tone curve.",
                                 bake(brighter)),
                            cell("ROUGHER EVERYWHERE", "roughnessBias = 0.45",
                                 "A bias softens every reflection without "
                                 "editing the materials.",
                                 bake(softened))},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::sectionHeader(
                 {.label = "THE ENVIRONMENT AROUND IT",
                  .note = "Separate the light contribution, the next sky, and "
                          "the visible backdrop."}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("DIFFUSE / SPECULAR", "diffuse .15 specular 2",
                            "A stronger reflection over a reduced diffuse "
                            "bounce.",
                            bake(mirrored)),
                       cell("CROSSFADE TO SUNSET", "crossfade 0.75 to sunset",
                            "The next panorama contributes 75% of the "
                            "illumination.",
                            bake(mixed)),
                       cell("SHOW THE SKY", "backdrop 1.0 blur 0.35",
                            "The backdrop becomes visible, with its own blur.",
                            bake(shown))},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(EnvLanes, "Kit · API",
             "one chrome body baked six times under the environment node's "
             "own dials — the stop, the roughness bias, the two "
             "sides of the map, the crossfade and the shown sky")
