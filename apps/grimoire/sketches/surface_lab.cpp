/** @file
 * surface_lab — World's one surface model, under a rig and under a sky.
 *
 * Every body on this sheet is shaded by the same metallic-roughness
 * surface on the device. What differs from body to body is only what
 * the surface is told — a number, a map, a mask, an emission — and what
 * reaches it: three lamps of the kit's rig, and a turning sky the
 * environment terms read. A card and a sphere are not two models; they
 * are two shapes that show the one model's terms best.
 *
 *   THE BACK ROW, five curved cards, each a different KIND of surface:
 *     plain; a STACK of two through a mask; a mapped normal; roughness and
 *     metallic sweeping down the card; and one that emits in a lattice.
 *     The cards are curved and lean back so the key's highlight lands
 *     somewhere on every face and sweeps across it.
 *   THE FRONT ROW, four spheres and a plate, each legible only because
 *     the sky is there: a mirror, a rough metal, a dielectric that picks
 *     the sky up at its rim, glass that bends the set behind it — and a
 *     flat chrome squircle whose rounded shoulder is a RELIEF a page
 *     paints from the outline's own coverage, so a flat plate mirrors the
 *     sky across a bevel no mesh carries.
 *
 * THE SKY IS THE FRAME'S ENVIRONMENT, a studio crossfaded toward a sunset,
 * both made by the World kit's environment maker. Its `rotation` is one
 * dial: every reflection turns with it while the lights and the bodies
 * stand still. The crossfade is HELD at one value — a dial that moved with
 * the time would be photographed at whatever value each lane's own route
 * to the capture reached — and the turn is what moves.
 *
 * EVERY MAP IS GENERATED. The floor's two maps are the pattern shelf's
 * checker handed through the same `texture::` door a scanned folder
 * arrives by; the normals are height fields differentiated on the device;
 * the mask is a noise; the packed sweep and the lattice are compose trees
 * painted once as textures. Nothing is read off the disk, so the plate is a
 * function of the declaration.
 *
 * EDIT THESE FIRST
 *   cards()       the five map surfaces of the back row
 *   spheres()     the four environment surfaces of the front row
 *   kBevel        the chrome plate's shoulder, in page pixels
 *   studio(), sunset()   the two skies the crossfade runs between
 */

// TAGS: Materials/Lighting

#include <include/core/SkMatrix.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/TextureSet.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <glm/vec3.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace field = sigil::material::field;
namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

namespace {

/** The pitch both rows stand on, so each card has a body in front of it. */
constexpr float kPitch = 118.0f;
/** Where the two rows stand, front to back. */
constexpr float kFrontRow = 96.0f;
constexpr float kBackRow = -120.0f;

constexpr float kCardWidth = 96.0f;
constexpr float kCardHeight = 132.0f;
/** THE RADIUS THE CARDS ARE CURVED ON. A flat card presents one normal
 *  over its whole face, so its highlight is one value; at this radius a
 *  card's face turns through most of a right angle and the highlight
 *  lands somewhere on every one of them. */
constexpr float kCardCurve = 80.0f;
/** HOW FAR THE CARDS LEAN BACK. Normals that all lie flat never face
 *  halfway between an eye above the row and a light above the floor;
 *  leaning the cards back toward that direction is what puts the
 *  highlight on them, and the curve then sweeps it across each face. */
constexpr float kCardLean = 25.0f;

constexpr float kRadius = 50.0f;
/** The chrome plate: its page, in page pixels, and the shoulder its
 *  relief rounds the outline over. */
constexpr float kPlateSide = 240.0f;
constexpr float kBevel = 92.0f;
/** The plate's page pixels to scene units, so it stands a sphere wide. */
constexpr float kPlateScale = 2.0f * kRadius / kPlateSide;

// ---- the back row: what a surface is told -----------------------------

/** A card standing upright at column @p column, wearing @p surface. */
world::Mesh card(std::string_view key, int column,
                 material::Material surface) {
  return world::mesh(
             gm::cylinderPanel(kCardWidth, kCardHeight, kCardCurve, 24, 6))
      .key(key)
      .at({(float)(column - 2) * kPitch, 66.0f, kBackRow})
      .rotateX(-kCardLean)
      .fill(std::move(surface))
      .collection("card");
}

/** PLAIN: a colour and a roughness, and no map anywhere — the control. */
material::Material plain() {
  return material::surface::program(
      {.baseColor = {0.62f, 0.28f, 0.24f, 1.0f}, .roughness = 0.55f});
}

/** A field of domes, as a normal: cellular distance read as a height,
 *  differentiated on the device. */
material::Material domes(float frequency, float depth) {
  return material::surface::normalFromHeight(field::cellular(frequency),
                                             {.depth = depth});
}

/** A STACK: a dark rough patina, a pale glossy crust over it, and a noise
 *  that says where the crust shows. Every operand is an ordinary
 *  material; on the device the stack shades AS a stack, the two
 *  operands' colours and per-pixel terms mixed by the same coverage. */
material::Material stacked() {
  const material::Material base = material::surface::program(
      {.baseColor = {0.13f, 0.20f, 0.17f, 1.0f}, .roughness = 0.8f});
  material::Material crust = material::surface::program(
      {.baseColor = {0.80f, 0.72f, 0.56f, 1.0f}, .roughness = 0.3f});
  const material::Material mask = material::noise(
      2.5f, {.octaves = 3, .seed = 3, .grain = true, .contrast = 2.6f});
  return material::over(base, std::move(crust), mask);
}

/** MAPPED NORMAL: one flat colour, and a height field that makes the key
 *  land on a field of domes. Nothing but the normal varies over it. */
material::Material bumped() {
  return material::from(material::Color{0.42f, 0.47f, 0.58f, 1.0f})
      .surface({.roughness = 0.35f, .normal = domes(5.0f, -3.0f)});
}

/** ONE PACKED IMAGE holding occlusion, roughness and metallic at glTF's
 *  channel order, sweeping down the card: rough dielectric at the top,
 *  polished metal at the bottom — one gradient, painted once as a
 *  texture. Neither swept channel reaches one, because a map that is
 *  white everywhere is what "no map here" means. */
compose::Element packedSweep() {
  return compose::stack().width(64).height(256).fill(material::linearGradient(
      {0.5f, 0.0f}, {0.5f, 1.0f},
      {material::Color{1.0f, 0.94f, 0.02f, 1}, {1.0f, 0.06f, 0.82f, 1}}));
}

/** ROUGHNESS AND METALLIC FROM ONE IMAGE in two slots, read at two
 *  channels — the way a set that ships one occlusion-roughness-metallic
 *  image is wired. The scalars start at one so the map's own values are
 *  what reach the shading. */
material::Material sweep(const material::Texture& packed) {
  material::Material swept = material::surface::program({
      .baseColor = {0.78f, 0.76f, 0.70f, 1.0f},
      .metallic = 1.0f,
      .roughness = 1.0f,
      .roughnessChannel = 1.0f,
      .metallicChannel = 2.0f,
  });
  swept.slot(material::surface::kRoughnessSlot, packed);
  swept.slot(material::surface::kMetallicSlot, packed);
  return swept;
}

/** THE LATTICE the emissive card lights in: white filaments on black,
 *  five across and seven down, painted once as a texture. */
compose::Element lattice() {
  constexpr float kWide = 200.0f, kTall = 280.0f, kFilament = 5.0f;
  std::vector<compose::Element> filaments;
  for (int column = 0; column < 5; ++column)
    filaments.push_back(compose::kit::at(((float)column + 0.5f) * kWide / 5.0f -
                                             kFilament / 2.0f,
                                         0, kFilament, kTall)
                            .fill(material::Color{1, 1, 1, 1}));
  for (int row = 0; row < 7; ++row)
    filaments.push_back(
        compose::kit::at(0, ((float)row + 0.5f) * kTall / 7.0f - kFilament / 2.0f,
                         kWide, kFilament)
            .fill(material::Color{1, 1, 1, 1}));
  return compose::stack().width(kWide).height(kTall).children(
      std::move(filaments));
}

/** EMISSIVE: a dark surface that is also a light source in a pattern —
 *  the lattice as the emission map, multiplied by the emission's own
 *  colour and strength and added after the lighting. */
material::Material emitting(const material::Texture& filaments) {
  return material::from(material::Color{0.30f, 0.13f, 0.09f, 1.0f})
      .surface({.roughness = 0.6f,
                .emission = {1.0f, 0.62f, 0.24f, 1.0f},
                .emissionStrength = 2.4f,
                .emissionMap = material::image(filaments)});
}

world::Group cards(const material::Texture& packed,
                   const material::Texture& filaments) {
  return world::group({card("plain", 0, plain()),
                       card("stacked", 1, stacked()),
                       card("bumped", 2, bumped()),
                       card("sweep", 3, sweep(packed)),
                       card("emissive", 4, emitting(filaments))})
      .key("cards");
}

// ---- the front row: what a surface sees ---------------------------------

/** A sphere at column @p column, wearing @p surface. Finely tessellated, so
 *  the silhouette of a mirror reads as a ball rather than a polygon. */
world::Mesh ball(std::string_view key, int column,
                 material::Material surface) {
  return world::mesh(gm::superellipsoid({kRadius, kRadius, kRadius}, 2.0f, 64,
                                        40))
      .key(key)
      .at({(float)(column - 2) * kPitch, kRadius, kFrontRow})
      .fill(std::move(surface))
      .collection("ball");
}

/** A polished mirror. Steel is not white: a slight cool bias. */
material::Material chrome() {
  return material::from(material::Color{0.92f, 0.95f, 1.0f, 1})
      .surface({.metallic = 1.0f, .roughness = 0.04f});
}

/** Clear glass: what is behind it, refracted across the ball's depth,
 *  with a faint green where the light has crossed the most of it, which
 *  is what tells an eye it is glass rather than a hole. */
material::Material glass() {
  return material::from(material::Color{1, 1, 1, 1})
      .surface({.metallic = 0.0f,
                .roughness = 0.02f,
                .transmission = 1.0f,
                .ior = 1.5f,
                .thickness = 2.0f * kRadius,
                .attenuationColor = {0.90f, 0.98f, 0.94f, 1},
                .attenuationDistance = 2.0f * kRadius});
}

world::Group spheres() {
  return world::group(
             {ball("chrome", 0, chrome()),
              ball("rough", 1,
                   material::from(material::Color{0.85f, 0.86f, 0.88f, 1})
                       .surface({.metallic = 1.0f, .roughness = 0.35f})),
              ball("dielectric", 2,
                   material::from(material::Color{0.14f, 0.30f, 0.42f, 1})
                       .surface({.metallic = 0.0f, .roughness = 0.15f})),
              ball("glass", 3, glass())})
      .key("spheres");
}

/** The plate's outline, in its page's pixels. */
sigil::geometry::path::Outline squircle() {
  return shapes::squircle(3.4f).outline({kPlateSide, kPlateSide});
}

/** THE PAGE THE PLATE WEARS: the squircle in chrome, its shoulder a relief
 *  of the outline's own coverage, so the maps the page paints carry the
 *  normals of a rounded bevel over a flat face. */
compose::Element platePage() {
  return compose::stack()
      .width(kPlateSide)
      .height(kPlateSide)
      .children({compose::kit::at(0, 0, kPlateSide, kPlateSide)
                     .shape(compose::heldPath(squircle()))
                     .background(compose::relief(
                         chrome(), {.shoulder = kBevel, .depth = 0.8f}))});
}

// ---- the two skies -------------------------------------------------------

/** A graded neutral studio: a bright zenith over a dim floor, a key box, a
 *  fill box and a thin rim strip. */
world::EnvironmentMap studio() {
  return world::kit::environmentMap(
             {.width = 512,
              .zenith = {0.65f, 0.67f, 0.73f},
              .nadir = {0.10f, 0.11f, 0.13f},
              .exponent = 1.4f,
              .softBoxes = {{.centre = {0.30f, 0.30f},
                             .size = {0.055f, 0.09f},
                             .radiance = {3.2f, 3.2f, 3.2f}},
                            {.centre = {0.72f, 0.38f},
                             .size = {0.075f, 0.12f},
                             .radiance = {1.1f, 1.1f, 1.1f}},
                            {.centre = {0.02f, 0.22f},
                             .size = {0.02f, 0.20f},
                             .radiance = {2.2f, 2.2f, 2.2f}}}})
      .withGround({0.06f, 0.065f, 0.08f, 1});
}

/** A sunset: a deep blue zenith falling to a hot band at the horizon, a
 *  low sun and a thin flare along it. */
world::EnvironmentMap sunset() {
  return world::kit::environmentMap(
             {.width = 512,
              .zenith = {0.05f, 0.10f, 0.30f},
              .nadir = {0.16f, 0.05f, 0.16f},
              .exponent = 1.6f,
              .softBoxes = {{.centre = {0.5f, 0.47f},
                             .size = {4.0f, 0.07f},
                             .radiance = {1.05f, 0.45f, 0.15f}},
                            {.centre = {0.5f, 0.46f},
                             .size = {0.035f, 0.045f},
                             .radiance = {2.6f, 1.6f, 0.7f}},
                            {.centre = {0.5f, 0.52f},
                             .size = {4.0f, 0.008f},
                             .radiance = {1.6f, 0.9f, 0.45f}}}})
      .withGround({0.05f, 0.03f, 0.05f, 1});
}

/** The floor's two maps, keyed by the usage words a material tool tags
 *  its outputs with — the door a discovered folder's files arrive by. */
material::texture::TextureMaps floorMaps() {
  const auto check = [](material::Color first, material::Color second,
                        float cell) {
    return material::skia::image(
        material::pattern::checker(cell, first, second).texture());
  };
  std::map<std::string, sigil::media::PixelSource> byUsage;
  byUsage["baseColor"] =
      check({0.30f, 0.33f, 0.38f, 1}, {0.17f, 0.19f, 0.23f, 1}, 16.0f);
  byUsage["roughness"] =
      check({0.7f, 0.7f, 0.7f, 1}, {0.35f, 0.35f, 0.35f, 1}, 32.0f);
  return material::texture::fromUsageMap(byUsage, /*normalDirectX=*/false);
}

}  // namespace

namespace {

struct SurfaceLab {
  /** The rows, the skies and the floor, made once: none of them is a
   *  function of the time, and a sky is baked and prefiltered the first
   *  time it is asked for. */
  world::Group subject;
  world::EnvironmentMap studioSky, sunsetSky;
  std::optional<material::Material> floorSurface;
  sketch::Page plate;
  compose::Element plateTree = compose::stack();

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = {1000, 640},
              .captureAt = 1.6,
              .background = material::Color{0.02f, 0.024f, 0.035f, 1.0f}});
    plate = ctx.page({(int)kPlateSide, (int)kPlateSide});
    plateTree = platePage();
    const auto plateFace = gm::extrude(squircle(), 8.0f,
                                       {.backCap = false, .walls = false});
    subject =
        world::group(
            {cards(ctx.texture(packedSweep(), {64, 256}),
                   ctx.texture(lattice(), {200, 280},
                               material::Color{0, 0, 0, 1})),
             spheres(),
             world::mesh(plateFace)
                 .key("bevelled-plate")
                 .at({2.0f * kPitch, kRadius, kFrontRow})
                 .scale(kPlateScale)
                 .fill(plate.surface())
                 .collection("ball")})
            .key("study")
            // The rows run across the turntable's parked station rather
            // than along it, so every body is seen face on.
            .rotateY(90.0f);
    studioSky = studio();
    sunsetSky = sunset();
    // The floor is what scales the scene: a texture that repeats says how
    // large the room is.
    material::Material floor =
        material::surface::program(floorMaps(), {.baseColor = {1, 1, 1, 1}});
    if (const material::Texture* map =
            material::surface::map(floor, material::surface::kBaseColorSlot)) {
      material::Texture tiled = *map;
      // The baked tile is two cells across, so the repeat is set against
      // ITS size: this many tiles cover the floor.
      tiled.tile(material::Repeat::Repeat)
          .uv(material::skia::toMatrix(
              SkMatrix::Scale(1.0f / 20.0f, 1.0f / 20.0f)));
      floor.slot(material::surface::kBaseColorSlot, std::move(tiled));
    }
    floorSurface = std::move(floor);
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    // The rig is dimmer than a studio's, because the sky is much of the
    // light in the scene: a key at full strength would wash the
    // reflections out. The turntable is PARKED — a lab is read, not
    // watched — so what moves in the picture is the sky and nothing else.
    const world::kit::Set set{.rig = {.extent = 160.0f,
                                      .bearing = 72.0f,
                                      .elevation = 32.0f,
                                      .intensity = 1.5f,
                                      .fillIntensity = 0.5f,
                                      .backIntensity = 0.5f,
                                      .ambientIntensity = 0.05f},
                              .table = {.radius = 860.0f,
                                        .height = 560.0f,
                                        .period = sigil::motion::Duration(0.0),
                                        .fieldOfView = 36.0f},
                              .ground = 5.0f,
                              .drop = 0.0f,
                              .surface = floorSurface};
    // THE SKY. Its turn is one dial and turns every reflection at once;
    // the crossfade toward the sunset is held. It is shown, softly, so
    // the rows are read against the sky they reflect.
    return world::Frame(world::kit::litSet(subject, set, now))
        .environment(studioSky, {.intensity = 0.45f,
                                 .next = sunsetSky,
                                 .crossfade = 0.45f,
                                 .rotation = seconds * 26.0f,
                                 .shown = 0.85f,
                                 .blur = 0.35f})
        .pass(plate.render(plateTree));
  }
};

}  // namespace

SIGIL_SKETCH(SurfaceLab, "Scene3D",
             "World's one surface model under a rig and a turning sky — five "
             "cards told a colour, a masked stack, a normal, a roughness and "
             "metallic sweep and an emission, before a mirror, a rough "
             "metal, a dielectric, glass and a flat chrome plate bevelled by "
             "a page's relief")
