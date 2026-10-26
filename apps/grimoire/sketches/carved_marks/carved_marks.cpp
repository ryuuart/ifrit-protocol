/** One letter die worked into four bodies on a slate table — cut into
 *  limestone, raised and chased in bronze, impressed and gilt in porcelain,
 *  pierced clean through brass with its counters held on bridges and its
 *  under-stroke sunk and filled with enamel — under one low key light that
 *  travels across them, so every mark is read from the shadow it throws.
 */
// TAGS: Compose/Materials, Materials/Stone, Materials/Metal,
// Materials/Ceramic, Materials/Lighting, Geometry/3D, Typography/Relief,
// Geometry/Path Operations

#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Offset.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilgeometry/path/Transform.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilweave/layout/GlyphOutline.h>
#include <sigilworld/World.h>

#include <cmath>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <utility>

namespace compose = sigil::compose;
namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace world = sigil::world;

namespace {

using compose::Element;
using material::ColorChannel;
using material::hexColor;

constexpr float kLoop = 16, kPi = 3.14159265358979f;
constexpr material::Color kWhite{1, 1, 1, 1}, kBlack{0, 0, 0, 1};

/** THE FOUR FACES, in page pixels, which are also scene units: a block of
 *  limestone, a bronze plate, a porcelain tile and a brass plate, each as
 *  thick as it is. */
constexpr glm::vec2 kStoneFace{1040, 600}, kBronzeFace{700, 430},
    kTileFace{480, 480}, kBrassFace{440, 260};
constexpr float kStoneThickness = 120, kStoneCut = 16;
constexpr float kBronzeThickness = 16, kBronzeRaise = 9;
constexpr float kTileThickness = 26;
/** The brass is pierced through its whole thickness and its enamel lies
 *  `kBrassRecess` below the face. */
constexpr float kBrassThickness = 14, kBrassRecess = 3;

/** A rectangle of @p size with its corners rounded to @p radius. */
path::Outline slab(glm::vec2 size, float radius) {
  return shapes::outlineOf(
      shapes::rounded(
          [](glm::vec2 extent) {
            return path::Outline::rectangle(path::Rect::of({0, 0}, extent));
          },
          radius),
      size);
}

/** A mark laid into a depth map: @p outline filled at @p grey, softened. */
Element pressed(const path::Outline& outline, glm::vec2 face, float grey,
                float soften) {
  auto mark = compose::kit::at(0, 0, face.x, face.y)
                  .shape(compose::heldPath(outline))
                  .fill(material::Color{grey, grey, grey, 1});
  if (soften > 0) mark.filter(material::Filter::blur(soften));
  return mark;
}

/** A tool pass laid into a depth map: a band swept along @p spine. */
Element swept(const path::Outline& spine, glm::vec2 face, path::Profile width,
              float grey) {
  return compose::kit::at(0, 0, face.x, face.y)
      .shape(compose::heldPath(spine))
      .stroke(compose::brush::ribbon(
          std::move(width), compose::Fill::color({grey, grey, grey, 1})));
}

/** Where the die's under-stroke sits on a face @p face wide, scaled by
 *  @p scale with its top at @p top. */
path::Transform underStroke(glm::vec2 face, float top, float scale) {
  return path::Transform::translate({(face.x - 620 * scale) / 2, top}) *
         path::Transform::scale({scale, scale});
}

/** The under-stroke's pressure line, placed on its face. */
path::Outline pressureLine(glm::vec2 face, float top, float scale) {
  return path::Outline::svg(
             "M24 52 C136 4 225 75 315 39 C420 -11 501 59 600 18")
      .transformed(underStroke(face, top, scale));
}

/** The width the pressure line is laid at: thin at both ends, swelling
 *  twice along its run. */
path::Profile pressureWidth(float scale) {
  return path::Profile{{0, 1},
                       {.12f, 12 * scale},
                       {.35f, 26 * scale},
                       {.61f, 8 * scale},
                       {.84f, 19 * scale},
                       {1, 1}};
}

/** THE PRESSURE RIBBON AND ITS FIBRE PASSES, the die's under-stroke,
 *  scaled into a face @p width wide with its top at @p top. */
std::vector<Element> ribbon(glm::vec2 face, float top, float scale) {
  const auto fibre =
      path::Outline::svg("M18 24 C151 -16 220 70 325 21 C442 -30 509 45 608 -7")
          .transformed(underStroke(face, top, scale));
  std::vector<Element> marks{
      swept(pressureLine(face, top, scale), face, pressureWidth(scale), .8f)
          .filter(material::Filter::blur(1.2f))};
  const path::Profile strand{
      {0, .1f}, {.12f, .8f}, {.43f, 1.4f}, {.76f, .7f}, {1, .1f}};
  for (int pass = 0; pass < 9; ++pass)
    marks.push_back(swept(fibre, face, strand, .55f)
                        .translateY((pass * 2.6f - 11) * scale)
                        .opacity(.45f + .05f * (pass % 5)));
  return marks;
}

/** A depth map, black at the face and white at the bottom of a mark. */
Element depthMap(glm::vec2 face, std::vector<Element> marks) {
  return compose::stack().width(face.x).height(face.y).fill(kBlack).children(
      std::move(marks));
}

material::Material depthOf(const material::Texture& depth) {
  return material::image(material::Texture(depth)
                             .tile(material::Repeat::Pad)
                             .sampling(material::Sampling::Linear));
}

/** A normal from @p depth, with a second, finer one laid over it. */
material::Material normalOf(const material::Material& depth, float scale,
                            const material::Material& detail,
                            float detailScale) {
  return material::surface::blendNormals(
      material::surface::normalFromHeight(
          depth, {.depth = -scale, .step = .75f, .directX = true}),
      material::surface::normalFromHeight(
          detail, {.depth = detailScale, .directX = true}),
      {.baseDirectX = true, .detailDirectX = true, .outputDirectX = true});
}

/** LIMESTONE: a warm cream, bedded in faint strata, mottled and pored,
 *  its faces worn into a slow swell; the shoulders of every cut roll
 *  down into it, and grime settles where they do. */
material::Material limestone(const material::Material& depth) {
  const auto pores =
      material::noise(.32f, {.octaves = 2, .grain = true, .contrast = 2.4f});
  const auto worn = material::noise(.004f, {.octaves = 3, .grain = true});
  auto face =
      material::from(hexColor(0xe8dfc8))
          .layer(material::noise(.0035f,
                                 {.octaves = 3, .grain = true, .stretch = 5}),
                 {.blend = material::BlendMode::Multiply, .opacity = .14f})
          .layer(material::noise(.02f, {.octaves = 3, .grain = true}),
                 {.blend = material::BlendMode::Multiply, .opacity = .09f})
          .layer(pores,
                 {.blend = material::BlendMode::Multiply, .opacity = .14f})
          .layer(hexColor(0x8c8268),
                 {.opacity = .45f, .mask = {depth, ColorChannel::Luminance}});
  const auto texture = material::surface::blendNormals(
      material::surface::normalFromHeight(pores,
                                          {.depth = -.6f, .directX = true}),
      material::surface::normalFromHeight(worn,
                                          {.depth = 3.5f, .directX = true}),
      {.baseDirectX = true, .detailDirectX = true, .outputDirectX = true});
  return face.surface(
      {.roughness = .86f,
       .normal = material::surface::blendNormals(
           material::surface::normalFromHeight(
               depth, {.depth = -7, .step = .75f, .directX = true}),
           texture,
           {.baseDirectX = true, .detailDirectX = true, .outputDirectX = true}),
       .normalDirectX = true});
}

/** BRONZE: a warm cast metal clouded with a green-brown patina that
 *  gathers in the chasing, the field dimpled by the hammer. */
material::Material bronze(const material::Material& depth) {
  const auto clouds = material::noise(.006f, {.octaves = 4, .grain = true});
  const auto hammer =
      material::noise(.045f, {.octaves = 2, .grain = true, .contrast = 1.4f});
  auto face =
      material::from(hexColor(0xa8743f))
          .layer(clouds,
                 {.blend = material::BlendMode::Multiply, .opacity = .35f})
          .layer(hexColor(0x4f5f45),
                 {.opacity = .55f, .mask = {clouds, ColorChannel::Luminance}})
          .layer(hexColor(0x2c2a20),
                 {.opacity = .7f, .mask = {depth, ColorChannel::Luminance}});
  return face.surface(
      {.metallic = material::remap(depth, {.to = {.95f, .45f}}),
       .roughness = material::from(material::Color{.34f, .34f, .34f, 1})
                        .layer(material::Color{.6f, .6f, .6f, 1},
                               {.opacity = .5f,
                                .mask = {clouds, ColorChannel::Luminance}}),
       .normal = normalOf(depth, 4, hammer, 1.1f),
       .normalDirectX = true});
}

/** PORCELAIN AND GOLD over one held depth: the same map turns the white
 *  glaze into gold — colour, metalness and roughness — and takes the
 *  glaze's clear coat away where it does, so one continuous surface is
 *  ceramic on the top and metal in the hollow. */
material::Material porcelain(const material::Material& depth) {
  const auto gold = hexColor(0xe2b45c);
  auto face =
      material::from(hexColor(0xf3f1ea))
          .layer(material::noise(.01f, {.octaves = 2, .grain = true}),
                 {.blend = material::BlendMode::Multiply, .opacity = .05f})
          .layer(gold, {.mask = {depth, ColorChannel::Luminance}});
  return face.surface(
      {.metallic = material::remap(depth, {.to = {0, 1}}),
       .roughness = material::remap(depth, {.to = {.12f, .26f}}),
       .normal =
           normalOf(depth, 5,
                    material::noise(.08f, {.octaves = 2, .grain = true}), .25f),
       .normalDirectX = true,
       .clearcoat = material::remap(depth, {.to = {1, 0}}),
       .clearcoatRoughness = .04f});
}

/** SLATE for the table: a cold dark stone with a faint cleft. */
material::Material slate() {
  const auto cleft =
      material::noise(.01f, {.octaves = 4, .grain = true, .stretch = 2});
  return material::from(hexColor(0x2b2f33))
      .layer(cleft, {.blend = material::BlendMode::Multiply, .opacity = .4f})
      .surface({.roughness = .62f,
                .normal = material::surface::normalFromHeight(
                    cleft, {.depth = 1.2f, .directX = true}),
                .normalDirectX = true});
}

/** THE DIE CUT AS A STENCIL: @p letters with a bar @p bar tall left
 *  standing across them at each of @p rows, so every counter stays joined
 *  to the plate round it once the letters are cut through. */
path::Outline stencil(const path::Outline& letters, glm::vec2 face,
                      std::initializer_list<float> rows, float bar) {
  path::Outline cuts = letters;
  for (const float row : rows)
    cuts = cuts.subtracted(path::Outline::rectangle(
        path::Rect::of({0, row - bar / 2}, {face.x, bar})));
  return cuts;
}

/** A page whose one box wears @p surface over the whole @p face. */
Element facePage(glm::vec2 face, const material::Material& surface) {
  return compose::stack().width(face.x).height(face.y).fill(surface);
}

material::Material plain(material::Color colour, float roughness,
                         float metallic = 0) {
  return material::surface::program(
      {.baseColor = colour, .metallic = metallic, .roughness = roughness});
}

/** A solid of @p outline, @p depth thick, lying face up: its top wears
 *  @p top, its walls and underside @p sides. */
world::Group lying(std::string_view key, const path::Outline& outline,
                   float depth, const material::Material& top,
                   const material::Material& sides) {
  return world::group(
             {world::mesh(gm::extrude(outline, depth,
                                      {.backCap = false, .walls = false}))
                  .key(std::string(key) + "-face")
                  .fill(top),
              world::mesh(gm::extrude(outline, depth, {.frontCap = false}))
                  .key(std::string(key) + "-sides")
                  .fill(sides)})
      .key(key)
      .rotateX(-90);
}

/** THE KEY at a moment: where it comes from, and the bearing it comes
 *  from in degrees, which the room's own lamps turn with. */
struct Key {
  glm::vec3 from;
  float bearing;
};

/** The key at @p seconds: low over the table, from the left and in
 *  front, swinging round behind the bodies to the right and back again,
 *  rising a little as it passes behind. */
Key keyAt(double seconds) {
  const float t = float(std::fmod(seconds, double(kLoop))) / kLoop;
  const float swing = .5f - .5f * std::cos(2 * kPi * t);
  const float bearing = -112 + 224 * swing;
  const float azimuth = bearing * kPi / 180;
  const float elevation = (9 + 7 * std::sin(kPi * swing)) * kPi / 180;
  return {{std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
           -std::cos(azimuth) * std::cos(elevation) * .55f +
               std::cos(elevation) * .2f},
          bearing};
}

struct CarvedMarks {
  material::Texture stoneDepth, bronzeDepth, tileDepth;
  sketch::Page stonePage, bronzePage, tilePage, tablePage;
  Element stoneTree = compose::stack(), bronzeTree = compose::stack(),
          tileTree = compose::stack(), tableTree = compose::stack();
  world::EnvironmentMap room;
  world::Group stone, plate, tile, pierced;
  gm::Mesh table;

  static gm::camera::Camera lens() {
    gm::camera::Camera camera;
    camera.eye = {0, 1040, 1360};
    camera.target = {90, 10, 60};
    camera.verticalFieldOfViewDegrees = 40;
    camera.nearPlane = 60;
    camera.farPlane = 6000;
    return camera;
  }

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(ctx, {.size = {1440, 900},
                             .captureAt = 3.0,
                             .background = hexColor(0x101214)});
    // THE DIE, set once per face at that face's scale and centred where
    // that face carries it, in the face's own page pixels.
    const auto die = [&](float size, float track, glm::vec2 centre) {
      return weave::glyphOutline(ctx.fonts, u8"OB8",
                                 {.family = "Optima",
                                  .style = {.weight = 700},
                                  .size = size,
                                  .track = track,
                                  .at = centre});
    };
    const auto stoneLetters = die(330, 18, {kStoneFace.x / 2, 250});
    const auto bronzeLetters = die(210, 12, {kBronzeFace.x / 2, 175});
    const auto tileLetters =
        die(165, 9, {kTileFace.x / 2, kTileFace.y / 2 - 20});
    const glm::vec2 brassCentre{kBrassFace.x / 2, 108};
    const auto brassLetters = die(128, 7, brassCentre);

    // The depth each face's marks are laid into, white deepest.
    {
      std::vector<Element> marks{pressed(stoneLetters, kStoneFace, .55f, 7)};
      for (auto& mark : ribbon(kStoneFace, 430, 1.35f))
        marks.push_back(std::move(mark));
      stoneDepth = ctx.texture(depthMap(kStoneFace, std::move(marks)),
                               {int(kStoneFace.x), int(kStoneFace.y)}, kBlack);
    }
    {
      auto chasing = compose::kit::at(0, 0, kBronzeFace.x, kBronzeFace.y)
                         .shape(compose::heldPath(bronzeLetters))
                         .stroke(kWhite, {.width = 3.5f});
      chasing.filter(material::Filter::blur(1.4f));
      std::vector<Element> marks{std::move(chasing)};
      for (auto& mark : ribbon(kBronzeFace, 300, .92f))
        marks.push_back(std::move(mark));
      bronzeDepth =
          ctx.texture(depthMap(kBronzeFace, std::move(marks)),
                      {int(kBronzeFace.x), int(kBronzeFace.y)}, kBlack);
    }
    {
      const auto border =
          slab(kTileFace - glm::vec2(64), 22)
              .transformed(path::Transform::translate({32, 32}));
      auto rule = compose::kit::at(0, 0, kTileFace.x, kTileFace.y)
                      .shape(compose::heldPath(border))
                      .stroke(kWhite, {.width = 4});
      rule.filter(material::Filter::blur(1.2f));
      std::vector<Element> marks{pressed(tileLetters, kTileFace, 1, 2.2f),
                                 std::move(rule)};
      for (auto& mark : ribbon(kTileFace, 330, .6f))
        marks.push_back(std::move(mark));
      tileDepth = ctx.texture(depthMap(kTileFace, std::move(marks)),
                              {int(kTileFace.x), int(kTileFace.y)}, kBlack);
    }

    stonePage = ctx.page({int(kStoneFace.x), int(kStoneFace.y)});
    bronzePage = ctx.page({int(kBronzeFace.x), int(kBronzeFace.y)});
    tilePage = ctx.page({int(kTileFace.x), int(kTileFace.y)});
    tablePage = ctx.page({1024, 640});
    // The floor of every cut is painted on the same page, inside the
    // letters where the skin above is open: chiselled into a V, darker
    // with the dust the chisel left.
    const auto chiselled =
        material::from(hexColor(0xbcb197))
            .layer(material::noise(.05f, {.octaves = 2, .grain = true}),
                   {.blend = material::BlendMode::Multiply, .opacity = .18f})
            .surface({.roughness = .95f});
    stoneTree =
        facePage(kStoneFace, limestone(depthOf(stoneDepth)))
            .children({compose::kit::at(0, 0, kStoneFace.x, kStoneFace.y)
                           .shape(compose::heldPath(stoneLetters))
                           .background(compose::relief(
                               chiselled, {.shoulder = 24, .depth = -1.1f}))});
    bronzeTree = facePage(kBronzeFace, bronze(depthOf(bronzeDepth)));
    tileTree = facePage(kTileFace, porcelain(depthOf(tileDepth)));
    tableTree = facePage({1024, 640}, slate());

    const auto stoneSides = plain(hexColor(0xd9cfb6), .9f);
    const auto bronzeSides = plain(hexColor(0x8a5a30), .45f, .9f);
    const auto polished = plain(hexColor(0xc8955a), .22f, 1);
    const auto glaze =
        material::surface::program({.baseColor = hexColor(0xefece4),
                                    .roughness = .2f,
                                    .clearcoat = 1,
                                    .clearcoatRoughness = .05f});

    // THE BLOCK: a body of stone and, on it, the skin the letters are cut
    // through, so a cut has real walls and a floor the walls shade.
    const auto stoneOutline = slab(kStoneFace, 6);
    const float body = kStoneThickness - kStoneCut;
    stone =
        world::group({lying("stone-body", stoneOutline, body,
                            stonePage.surface(), stoneSides)
                          .at({0, body / 2, 0}),
                      lying("stone-skin", stoneOutline.subtracted(stoneLetters),
                            kStoneCut, stonePage.surface(), stoneSides)
                          .at({0, body + kStoneCut / 2, 0})})
            .key("limestone");

    // THE PLATE, with the die standing off it.
    const auto letterCentre = bronzeLetters.bounds().centre();
    plate =
        world::group({lying("bronze-plate", slab(kBronzeFace, 10),
                            kBronzeThickness, bronzePage.surface(), bronzeSides)
                          .at({0, kBronzeThickness / 2, 0}),
                      world::mesh(gm::extrude(bronzeLetters, kBronzeRaise))
                          .key("bronze-letters")
                          .at({letterCentre.x - kBronzeFace.x / 2,
                               kBronzeThickness + kBronzeRaise / 2,
                               letterCentre.y - kBronzeFace.y / 2})
                          .rotateX(-90)
                          .fill(polished)})
            .key("bronze");

    tile = world::group({lying("porcelain-tile", slab(kTileFace, 24),
                               kTileThickness, tilePage.surface(), glaze)
                             .at({0, kTileThickness / 2, 0})})
               .key("porcelain");

    // THE PIERCED BRASS: the die cut clean through the plate as a
    // stencil, so the slate and the shadow of every letter show through
    // it, and the under-stroke sunk into the face with enamel laid in the
    // floor of the recess. A skin over a body, like the stone's, gives the
    // recess walls of its own.
    const auto brass = plain(hexColor(0xcc963d), .24f, 1);
    const auto enamel = plain(hexColor(0x12382e), .14f);
    const auto brassPlate =
        slab(kBrassFace, 10)
            .subtracted(stencil(
                brassLetters, kBrassFace,
                {brassCentre.y - 32, brassCentre.y, brassCentre.y + 32}, 6));
    const auto recess =
        path::band(pressureLine(kBrassFace, 176, .6f), pressureWidth(.6f));
    const auto recessCentre = recess.bounds().centre();
    const float brassBody = kBrassThickness - kBrassRecess;
    pierced = world::group(
                  {lying("brass-body", brassPlate, brassBody, brass, brass)
                       .at({0, brassBody / 2, 0}),
                   lying("brass-skin", brassPlate.subtracted(recess),
                         kBrassRecess, brass, brass)
                       .at({0, brassBody + kBrassRecess / 2, 0}),
                   lying("brass-enamel", recess, 1.6f, enamel, enamel)
                       .at({recessCentre.x - kBrassFace.x / 2, brassBody + .8f,
                            recessCentre.y - kBrassFace.y / 2})})
                  .key("brass");

    table = gm::quad(4200, 2800);
    room = world::kit::environmentMap(
        {.zenith = {.42f, .44f, .48f},
         .nadir = {.05f, .05f, .06f},
         .softBoxes = {{.centre = {.22f, .40f},
                        .size = {.07f, .18f},
                        .radiance = {3.6f, 3.2f, 2.6f}},
                       {.centre = {.68f, .44f},
                        .size = {.05f, .24f},
                        .radiance = {1.2f, 1.5f, 1.9f}}}});
  }

  world::Frame describe(sigil::motion::Duration now) {
    const Key key = keyAt(now.count());
    return world::Frame(
               world::group(
                   {world::camera(lens(),
                                  {.exposure = 1.0f,
                                   .toneCurve = world::ToneCurve::Neutral}),
                    world::directionalLight(
                        -key.from, {.color = material::Color{1, .9f, .76f, 1},
                                    .intensity = 5.0f,
                                    .castShadow = true,
                                    .shadow = {.radius = 1.2f, .cascades = 3}})
                        .key("key"),
                    world::hemisphereLight(
                        {.color = material::Color{.55f, .62f, .75f, 1},
                         .groundColor = material::Color{.12f, .1f, .09f, 1},
                         .intensity = .14f})
                        .key("fill"),
                    world::mesh(table).key("table").rotateX(-90).fill(
                        tablePage.surface()),
                    world::Group(stone).at({-190, 0, -200}).rotateY(4),
                    world::Group(plate).at({190, 0, 360}).rotateY(-9),
                    world::Group(tile).at({660, 0, -170}).rotateY(14),
                    world::Group(pierced).at({-470, 0, 410}).rotateY(10)}))
        .environment(
            room, {.intensity = .9f, .diffuse = .3f, .rotation = key.bearing})
        .pass(stonePage.render(stoneTree))
        .pass(bronzePage.render(bronzeTree))
        .pass(tilePage.render(tileTree))
        .pass(tablePage.render(tableTree));
  }
};

}  // namespace

SIGIL_SKETCH(CarvedMarks, "Study · Materials",
             "One letter die cut into limestone, raised and chased in bronze, "
             "impressed in gilt porcelain and pierced through brass over an "
             "enamelled recess, under a low key light that travels across "
             "them so every mark reads from its shadow")
