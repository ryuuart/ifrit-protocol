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

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/element/Environment.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/kit/Kit.h>

#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace world = sigil::world;
namespace material = sigil::material;
namespace gm = sigil::geometry::mesh;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 424};
constexpr float kCell = 166;
constexpr float kPicture = 176;

constexpr float kExposure = 1.0f;  // the stop the reference is read at
constexpr float kBias = 0.45f;     // roughness added to every surface
constexpr float kBackdrop = 1.0f;  // how much sky is shown
constexpr float kBlur = 0.35f;     // …and how soft

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.06f, 0.065f, 0.08f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

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

/** The subject every cell bakes: one near-mirror body over a matte
 *  floor, so a reflection has something to be read against. */
world::Element subject() {
  world::Element set;
  set.key("subject")
      .child(world::Element()
                 .key("body")
                 .at({0, 40, 0})
                 .mesh(gm::superellipsoid({46, 46, 46}, 2.0f, 40, 26))
                 .fill(material::kit::surface(
                     {.baseColor = {0.85f, 0.86f, 0.88f, 1},
                      .metallic = 1.0f,
                      .roughness = 0.12f})))
      .child(world::Element()
                 .key("slab")
                 .at({0, -14, 0})
                 .mesh(gm::superellipsoid({150, 12, 150}, 8.0f, 20, 10))
                 .fill(material::kit::surface(
                     {.baseColor = {0.20f, 0.21f, 0.24f, 1},
                      .roughness = 0.65f})));
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

Element cell(const char* call, const char* note, sk_sp<SkImage> baked) {
  Element picture =
      baked ? image(std::make_shared<const sigil::image::ImageAsset>(
                  sigil::image::ImageAsset::wrap(std::move(baked))))
            : box();
  return kit::cell(voice(), toU8(call), toU8(note),
                   kit::well({.width = kCell,
                              .height = kPicture,
                              .ground = Fill::color(kCellGround)})
                       .child(std::move(picture).absolute().inset(0)));
}

}  // namespace

struct EnvLanes final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // every bake has already been taken

    const material::EnvironmentMap studio = material::EnvironmentMap::studio();
    const material::EnvironmentMap sunset = material::EnvironmentMap::sunset();

    /** One frame: the subject under an environment node carrying the
     *  cell's own dials, and nothing else different. */
    const auto bake = [&](world::Environment env) {
      world::Element root;
      root.key("set")
          .child(world::Element().key("sky").environmentMap(std::move(env)))
          .child(subject());
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

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("THE ENVIRONMENT'S DIALS \xc2\xb7 exposure, "
                           "roughnessBias, diffuse/specular, crossfade, "
                           "backdrop"),
             .subtitle = toU8("dials \xc2\xb7 one stop against two (1.0 and "
                              "2.0) \xc2\xb7 the roughness added to every "
                              "surface (0.45) \xc2\xb7 the crossfade (0.75) "
                              "\xc2\xb7 the sky's strength and blur"),
             .footer = toU8("a frame holds ONE environment node, so each "
                            "cell here is a frame of its own baked at the "
                            "pixels it will have \xe2\x80\x94 and exposure "
                            "is the only dial that still means something in "
                            "a set carrying no panorama at all"),
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
                     {cell("studio() \xc2\xb7 exposure 1",
                           "the reference \xc2\xb7 a near-mirror body over a "
                           "matte slab, lit by the panorama alone",
                           bake(base)),
                      cell("exposure = 2",
                           "one stop \xc2\xb7 every radiance multiplied "
                           "before the tone curve, so the shoulder falls "
                           "somewhere else",
                           bake(brighter)),
                      cell("roughnessBias = 0.45",
                           "added to every surface's roughness before it "
                           "picks a prefiltered level \xc2\xb7 the set "
                           "softens and no material was edited",
                           bake(softened)),
                      cell("diffuse .15 specular 2",
                           "a bright reflection over a dim bounce \xc2\xb7 "
                           "pushing one and not the other is a look, not a "
                           "physical claim",
                           bake(mirrored)),
                      cell("crossfade 0.75 to sunset",
                           "a second panorama mixed over the first \xc2\xb7 "
                           "both are sampled rather than one rebuilt, which "
                           "is what lets a sky change mid-frame",
                           bake(mixed)),
                      cell("backdrop 1.0 blur 0.35",
                           "the sky SHOWN rather than only reflected "
                           "\xc2\xb7 zero draws none of it, so the strength "
                           "is also the switch",
                           bake(shown))},
                 .gap = 10}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(EnvLanes, "Kit \xc2\xb7 API",
             "one chrome body baked six times under the environment node's "
             "own dials \xe2\x80\x94 the stop, the roughness bias, the two "
             "sides of the map, the crossfade and the shown sky")
