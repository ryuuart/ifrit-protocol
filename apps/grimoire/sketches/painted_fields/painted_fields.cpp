/** A painted field on a thick satin plate, lit by the scene: every brush
 *  stroke is written into a height, a wet coverage and a glow map, so dry
 *  pigment ridges catch a key light travelling round the plate on a rail,
 *  wet pools mirror a soft overhead panel, copper and gilt flash, and one
 *  stroke of alloy stands proud of the field, glows, blooms and casts a
 *  shadow across the paint.
 */
// TAGS: Compose/Materials, Materials/Paint, Materials/Metal,
// Materials/Lighting, Brushes/Painted data, Geometry/3D

#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Offset.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

namespace {

using compose::Element;
using material::hexColor;

constexpr int kWidth = 1440, kHeight = 900;
constexpr int kPageWidth = 1600, kPageHeight = 1000;
constexpr float kPlateDepth = 46, kAlloyDepth = 12;
constexpr float kLoop = 16, kPi = 3.14159265358979f;

constexpr material::Color kBlack{0, 0, 0, 1}, kWhite{1, 1, 1, 1};
constexpr auto kGesso = hexColor(0xd6cab0);
constexpr auto kUltramarine = hexColor(0x1f3f8f);
constexpr auto kPrussian = hexColor(0x15304f);
constexpr auto kRust = hexColor(0x9a4425);
constexpr auto kCopper = hexColor(0xc0703f);
constexpr auto kGilt = hexColor(0xe2b45c);
constexpr auto kAlloy = hexColor(0xe8913a);
constexpr auto kGlow = hexColor(0xff7a1c);

/** What a stroke is made of, which decides its colour, its finish and
 *  which maps it is written into. */
enum class Pigment { Ultramarine, Prussian, Rust, Copper, Medium, Alloy };

/** ONE BRUSH STROKE: a spine, the width law the brush pressed along it,
 *  its pigment and how high it stands, white being the highest. */
struct Stroke {
  std::string_view spine;
  std::vector<path::Stop> width;
  Pigment pigment;
  float lift;
};

/** THE PAINTING, back to front, in page pixels. */
const std::vector<Stroke>& strokes() {
  static const std::vector<Stroke> painting{
      // Blue oil dragged across the ground in long loaded passes.
      {"M130 190 C420 110 700 250 980 170 C1200 110 1360 160 1480 130",
       {{0, 10}, {.08f, 96}, {.45f, 128}, {.8f, 84}, {1, 12}},
       Pigment::Ultramarine,
       .8f},
      {"M110 330 C380 260 660 390 930 320 C1150 260 1310 330 1490 290",
       {{0, 8}, {.12f, 80}, {.5f, 104}, {.86f, 62}, {1, 6}},
       Pigment::Prussian,
       .72f},
      {"M180 500 C460 440 760 560 1040 490 C1230 445 1360 480 1470 460",
       {{0, 6}, {.1f, 70}, {.55f, 96}, {.9f, 54}, {1, 8}},
       Pigment::Ultramarine,
       .66f},
      // The copper overprint, pressed down and lifted in bands.
      {"M170 255 C520 225 900 260 1440 215",
       {{0, 4},
        {.1f, 26},
        {.2f, 9},
        {.32f, 30},
        {.44f, 10},
        {.58f, 34},
        {.7f, 12},
        {.84f, 28},
        {1, 4}},
       Pigment::Copper,
       .9f},
      // Rust scrubbed in short, dry strokes.
      {"M150 650 C230 600 330 640 420 590",
       {{0, 4}, {.3f, 44}, {.7f, 32}, {1, 4}},
       Pigment::Rust,
       .52f},
      {"M1120 600 C1210 555 1320 590 1450 540",
       {{0, 4}, {.4f, 38}, {1, 5}},
       Pigment::Rust,
       .48f},
      {"M1180 860 C1260 815 1350 845 1460 800",
       {{0, 3}, {.5f, 34}, {1, 4}},
       Pigment::Rust,
       .44f},
      {"M700 410 C760 390 830 405 880 385",
       {{0, 3}, {.5f, 26}, {1, 3}},
       Pigment::Rust,
       .84f},
      // Pools of oil medium, still wet, lying level where they settled.
      {"M380 505 C500 470 660 525 800 500",
       {{0, 20}, {.3f, 84}, {.7f, 90}, {1, 18}},
       Pigment::Medium,
       .72f},
      {"M860 515 C940 490 1030 520 1120 500",
       {{0, 16}, {.5f, 76}, {1, 14}},
       Pigment::Medium,
       .7f},
      {"M200 565 C270 548 350 568 420 556",
       {{0, 12}, {.5f, 48}, {1, 12}},
       Pigment::Medium,
       .1f},
      {"M1180 470 C1240 455 1320 470 1380 462",
       {{0, 12}, {.5f, 40}, {1, 10}},
       Pigment::Medium,
       .68f},
  };
  return painting;
}

/** THE ALLOY STROKE: the one mark that is also a body. */
const Stroke& alloyStroke() {
  static const Stroke alloy{
      "M1020 830 C1090 760 1170 790 1250 720 C1310 670 1370 680 1430 640",
      {{0, 3}, {.15f, 24}, {.55f, 32}, {.9f, 16}, {1, 2}},
      Pigment::Alloy,
      0};
  return alloy;
}

/** A stroke laid over the whole page, so a map painted across the page
 *  reads in the same place on it. */
Element laid(const Stroke& stroke, material::Material paint,
             float narrowed = 1) {
  std::vector<path::Stop> width = stroke.width;
  for (path::Stop& stop : width) stop.width *= narrowed;
  return compose::kit::at(0, 0, kPageWidth, kPageHeight)
      .shape(compose::heldPath(path::Outline::svg(stroke.spine)))
      .stroke(compose::brush::ribbon(path::Profile(std::move(width)),
                                     std::move(paint)));
}

/** Whether a stroke is a loaded brush dragged across the page, whose
 *  bristles leave strands of their own. */
bool dragged(const Stroke& stroke) {
  return stroke.pigment == Pigment::Ultramarine ||
         stroke.pigment == Pigment::Prussian || stroke.pigment == Pigment::Rust;
}

/** THE BRISTLE STRANDS a dragged stroke leaves: thin ribbons along rails
 *  across its width, each starting and lifting a little apart, so the
 *  ends of the stroke break into dry-brush trails. @p paint gives each
 *  strand its paint by its place across the stroke. */
template <typename PaintOf>
std::vector<Element> strands(const Stroke& stroke, PaintOf paint) {
  float widest = 0;
  for (const path::Stop& stop : stroke.width)
    widest = std::max(widest, stop.width);
  const auto spine = path::Outline::svg(stroke.spine);
  constexpr int kStrands = 9;
  std::vector<Element> out;
  for (int index = 0; index < kStrands; ++index) {
    const float across = (float(index) / (kStrands - 1) - .5f) * .78f;
    const auto rail = path::offset(spine, path::Profile(across * widest));
    const float length = rail.length();
    // A fixed scatter, so every map draws the same strands.
    const float start = .02f + .05f * float((index * 7) % 5) / 4;
    const float end = .98f - .06f * float((index * 3) % 4) / 3;
    out.push_back(compose::kit::at(0, 0, kPageWidth, kPageHeight)
                      .shape(compose::heldPath(
                          rail.segment(start * length, end * length)))
                      .stroke(compose::brush::ribbon(
                          path::Profile{{0, .3f},
                                        {.12f, 2.5f + float(index % 3)},
                                        {.88f, 2.0f + float(index % 2)},
                                        {1, .3f}},
                          paint(index))));
  }
  return out;
}

/** The lettering, in one place for every map it is written into. Its
 *  ink is stated on a box the size of the page and inherited by the
 *  type, so every map the ink paints lands where the other maps do. */
Element lettering(material::Material ink) {
  return compose::kit::at(0, 0, kPageWidth, kPageHeight)
      .ink(std::move(ink), compose::PaintBox::Subtree)
      .children({compose::kit::at(compose::text("FIELDS"), 110, 590, 1100, 300)
                     .fontFamily("Georgia, Times New Roman, serif")
                     .fontSize(230)
                     .fontWeight(700)
                     .letterSpacing(18)});
}

material::Color grey(float level) { return {level, level, level, 1}; }

/** THE HEIGHT MAP: every stroke at its lift, the oils and rust grooved by
 *  their bristles, the pools levelled smooth, the letters highest. */
Element heightMap() {
  auto field =
      compose::stack().width(kPageWidth).height(kPageHeight).fill(kBlack);
  // The gesso's own tooth: the broad brush that laid the ground.
  std::vector<Element> marks{
      compose::kit::at(0, 0, kPageWidth, kPageHeight)
          .fill(material::from(grey(.1f)).layer(
              material::noise(.012f, {.octaves = 4,
                                      .grain = true,
                                      .contrast = 1.6f,
                                      .stretch = 6}),
              {.blend = material::BlendMode::Multiply, .opacity = .9f}))};
  for (const Stroke& stroke : strokes()) {
    if (stroke.pigment == Pigment::Medium) {
      // A pool settles level and smooth.
      marks.push_back(
          laid(stroke, grey(stroke.lift)).filter(material::Filter::blur(6.0f)));
      continue;
    }
    // A loaded stroke stands as a body, thickest along its middle: three
    // passes, each narrower and higher, softened into one ridge and
    // grooved by the bristles that dragged it.
    const auto bristles = material::noise(.05f, {.octaves = 3,
                                                 .seed = stroke.lift * 17,
                                                 .grain = true,
                                                 .contrast = 1.8f,
                                                 .stretch = 16});
    const float passes[][2] = {{1, .5f}, {.72f, .78f}, {.42f, 1}};
    for (const auto& [narrowed, share] : passes)
      marks.push_back(
          laid(stroke,
               material::from(grey(stroke.lift * share))
                   .layer(bristles, {.blend = material::BlendMode::Multiply,
                                     .opacity = .45f}),
               narrowed)
              .filter(material::Filter::blur(3.0f)));
    for (Element& strand : strands(stroke, [&](int) {
           return material::Material(grey(std::min(1.0f, stroke.lift * 1.12f)));
         }))
      marks.push_back(std::move(strand).filter(material::Filter::blur(.8f)));
  }
  marks.push_back(lettering(kWhite).filter(material::Filter::blur(2.2f)));
  return field.children(std::move(marks));
}

/** THE WET MAP: white where the medium is still wet. */
Element wetMap() {
  auto field =
      compose::stack().width(kPageWidth).height(kPageHeight).fill(kBlack);
  std::vector<Element> pools;
  for (const Stroke& stroke : strokes())
    if (stroke.pigment == Pigment::Medium)
      pools.push_back(
          laid(stroke, kWhite).filter(material::Filter::blur(3.0f)));
  return field.children(std::move(pools));
}

/** THE GLOW MAP: the alloy stroke and the light it spills. */
Element glowMap() {
  return compose::stack()
      .width(kPageWidth)
      .height(kPageHeight)
      .fill(kBlack)
      .children(
          {laid(alloyStroke(), kWhite)
               .filter(material::Filter::blur(48))
               .opacity(.9f),
           laid(alloyStroke(), kWhite).filter(material::Filter::blur(12))});
}

material::Material image(const std::shared_ptr<compose::TextureScene>& scene) {
  return material::image(scene->texture()
                             .tile(material::Repeat::Pad)
                             .sampling(material::Sampling::Linear));
}

/** WHAT EVERY MARK ON THE PAGE SHARES: the normal of the one height, the
 *  wet map's mirror finish and clear coat over the dry roughness, and the
 *  alloy's glow. */
struct Maps {
  material::Material normal;
  material::Material wet;
  material::Material glow;

  material::Material finish(material::Material colour, float dryRoughness,
                            float metallic) const {
    const auto roughness =
        material::from(grey(dryRoughness))
            .layer(grey(.05f), {.mask = material::Mask{
                                    wet, material::ColorChannel::Luminance}});
    return colour.surface({.metallic = metallic,
                           .roughness = roughness,
                           .normal = normal,
                           .normalDirectX = true,
                           .emission = kGlow,
                           .emissionStrength = 1,
                           .emissionMap = glow,
                           .clearcoat = wet,
                           .clearcoatRoughness = .04f});
  }
};

/** Oil dragged by a brush: the colour streaked along the drag. */
material::Material oil(material::Color colour, float seed) {
  return material::from(colour).layer(
      material::noise(.04f, {.octaves = 3,
                             .seed = seed,
                             .grain = true,
                             .contrast = 1.4f,
                             .stretch = 14}),
      {.blend = material::BlendMode::Overlay, .opacity = .45f});
}

material::Color shaded(material::Color colour, float shade) {
  return {std::min(1.0f, colour.r * shade), std::min(1.0f, colour.g * shade),
          std::min(1.0f, colour.b * shade), colour.a};
}

/** A stroke's paint; @p shade lightens or darkens a dragged pigment, as
 *  the strands of one brushload differ. */
material::Material paintOf(const Stroke& stroke, const Maps& maps,
                           float shade = 1) {
  switch (stroke.pigment) {
    case Pigment::Ultramarine:
      return maps.finish(oil(shaded(kUltramarine, shade), stroke.lift * 9),
                         .62f, 0);
    case Pigment::Prussian:
      return maps.finish(oil(shaded(kPrussian, shade), stroke.lift * 9), .58f,
                         0);
    case Pigment::Rust:
      return maps.finish(oil(shaded(kRust, shade), stroke.lift * 9), .86f, 0);
    case Pigment::Copper:
      return maps.finish(oil(kCopper, 3), .3f, .92f);
    case Pigment::Medium:
      // A glaze of dark oil: it deepens what it lies over.
      return maps.finish(
          material::from(material::Color{.42f, .28f, .08f, .22f}), .05f, 0);
    case Pigment::Alloy:
      return maps.finish(material::from(kAlloy), .25f, .9f);
  }
  return maps.finish(material::from(kGesso), .9f, 0);
}

/** THE PAGE THE PLATE WEARS: bare satin steel at the margin, the gessoed
 *  field, the strokes, the gilt letters and a two-line caption. */
Element paintedPage(const Maps& maps) {
  const auto steel =
      material::from(hexColor(0x8c9097))
          .layer(material::noise(.03f, {.octaves = 3,
                                        .grain = true,
                                        .contrast = 1.3f,
                                        .stretch = 20}),
                 {.blend = material::BlendMode::Multiply, .opacity = .25f})
          .surface({.metallic = .9f, .roughness = .36f});
  const auto gesso =
      maps.finish(material::from(kGesso).layer(
                      material::noise(.15f, {.octaves = 2, .grain = true}),
                      {.blend = material::BlendMode::Multiply, .opacity = .1f}),
                  .9f, 0);
  std::vector<Element> content{
      compose::kit::at(0, 0, kPageWidth, kPageHeight)
          .shape(compose::heldPath(path::Outline::rectangle(
              {{70, 70}, {kPageWidth - 70, kPageHeight - 70}})))
          .fill(gesso)};
  for (const Stroke& stroke : strokes()) {
    content.push_back(laid(stroke, paintOf(stroke, maps)));
    if (!dragged(stroke)) continue;
    // The strands are the stroke's own paint, a shade lighter or darker
    // by turns, as a loaded brush lays it.
    for (Element& strand : strands(stroke, [&](int index) {
           return paintOf(stroke, maps, index % 2 ? 1.3f : .72f);
         }))
      content.push_back(std::move(strand));
  }
  content.push_back(laid(alloyStroke(), paintOf(alloyStroke(), maps)));
  content.push_back(lettering(
      maps.finish(material::from(kGilt).layer(
                      material::noise(.08f, {.octaves = 3, .grain = true}),
                      {.blend = material::BlendMode::Multiply, .opacity = .2f}),
                  .24f, .95f)));
  const auto caption = [](std::string_view line, float y, float size) {
    return compose::kit::at(compose::text(std::string(line)), 96, y, 900,
                            size * 1.6f)
        .fontSize(size)
        .letterSpacing(size * .12f)
        .ink(hexColor(0x3a3a36));
  };
  content.push_back(caption("PAINTED FIELDS", 892, 15));
  content.push_back(
      caption("dry ridges · wet pools · an alloy stroke that glows", 916, 12));
  return compose::stack()
      .width(kPageWidth)
      .height(kPageHeight)
      .fill(steel)
      .children(std::move(content));
}

/** A studio of one warm and one cool soft box for the metals to show. */
world::EnvironmentMap studio() {
  return world::kit::environmentMap(
      {.zenith = {.42f, .44f, .5f},
       .nadir = {.08f, .08f, .09f},
       .softBoxes = {{.centre = {.25f, .38f},
                      .size = {.08f, .14f},
                      .radiance = {4.5f, 4.0f, 3.2f}},
                     {.centre = {.72f, .4f},
                      .size = {.05f, .22f},
                      .radiance = {1.4f, 1.8f, 2.4f}}}});
}

/** The rail the key light travels: a low ring round the plate. */
const world::kit::Turntable kKeyRail{
    .at = {0, 0, 0}, .radius = 1350, .height = 300};

struct PaintedFields {
  std::shared_ptr<compose::TextureScene> heightScene, wetScene, glowScene;
  sketch::Page page;
  world::EnvironmentMap room;
  Element tree = compose::stack();
  gm::Mesh plate, face, alloy, bench;
  glm::vec3 alloyCentre{0};

  static gm::camera::Camera lens() {
    gm::camera::Camera camera;
    camera.eye = {-420, 1000, 1120};
    camera.target = {0, -90, 70};
    camera.fovYDegrees = 42;
    camera.nearPlane = 40;
    camera.farPlane = 8000;
    return camera;
  }

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = 11.0,
                             .background = hexColor(0x0c0d0f)});
    const SkISize size{kPageWidth, kPageHeight};
    heightScene = ctx.textureScene(size, kBlack);
    heightScene->render(heightMap());
    wetScene = ctx.textureScene(size, kBlack);
    wetScene->render(wetMap());
    glowScene = ctx.textureScene(size, kBlack);
    glowScene->render(glowMap());
    const Maps maps{
        .normal = material::surface::normalFromHeight(
            image(heightScene), {.depth = 7, .step = .9f, .directX = true}),
        .wet = image(wetScene),
        .glow = image(glowScene)};
    page = ctx.page({kPageWidth, kPageHeight});
    tree = paintedPage(maps);
    room = studio();

    // The plate: a slab whose face is left open for the page.
    plate =
        gm::box({-kPageWidth / 2.0f, -kPageHeight / 2.0f, -kPlateDepth},
                {kPageWidth / 2.0f, kPageHeight / 2.0f, 0}, {.front = false});
    face = gm::quad(kPageWidth, kPageHeight);
    bench = gm::quad(9000, 6000);
    // The alloy stroke lifted into a solid, standing where it is painted.
    const auto band = path::band(path::Outline::svg(alloyStroke().spine),
                                 path::Profile(alloyStroke().width));
    alloy = gm::extrude(band, {.depth = kAlloyDepth, .tolerance = .2f});
    const auto bounds = band.bounds();
    alloyCentre = {bounds.centre().x - kPageWidth / 2.0f,
                   kPageHeight / 2.0f - bounds.centre().y,
                   kAlloyDepth / 2.0f + .5f};
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const auto rail = world::kit::rail(kKeyRail);
    const float lap = rail.length();
    // The alloy breathes slowly, never dropping below the bloom's reach.
    const float breath = .5f + .5f * std::sin(2 * kPi * seconds / kLoop * 2);
    const auto alloySurface =
        material::from(kAlloy).surface({.metallic = .85f,
                                        .roughness = .3f,
                                        .emission = kGlow,
                                        .emissionStrength = 7 + 5 * breath});
    return world::Frame(
               world::group(
                   {world::camera(lens(),
                                  {.exposure = .78f,
                                   .toneCurve = world::ToneCurve::Neutral}),
                    world::null().key("aim").at({0, 0, 0}),
                    // THE KEY: a spot riding a low ring round the plate,
                    // raking across the paint and casting.
                    world::spotLight(
                        {0, 0, -1},
                        {.color = hexColor(0xfff0dc),
                         .intensity = world::intensityAt(10.0f, 1400.0f),
                         .innerAngle = 26,
                         .outerAngle = 40,
                         .castShadow = true,
                         .shadow = {.bias = 2,
                                    .normalBias = 1.5f,
                                    .radius = 2.5f,
                                    .mapSize = 2048}})
                        .key("key")
                        .along(rail, lap * seconds / kLoop)
                        .lookAt("aim"),
                    // THE SOFT PANEL: overhead and behind, tipped toward
                    // the eye, so its rectangle lies in the wet pools.
                    world::kit::visibleLight(
                        world::rectLight({0, -.67f, .74f},
                                         {.width = 900,
                                          .height = 260,
                                          .color = hexColor(0xe8f0ff),
                                          .intensity = 5.0f})
                            .key("panel"))
                        .key("panel rig")
                        .at({0, 900, -995}),
                    // THE ALLOY'S OWN LIGHT, spilling onto the paint
                    // round it and breathing with it.
                    world::sphereLight({.radius = 30,
                                        .color = kGlow,
                                        .intensity = world::intensityAt(
                                            .6f + .5f * breath, 160.0f),
                                        .range = 620})
                        .key("alloy glow")
                        .at({alloyCentre.x, 40, -alloyCentre.y}),
                    world::ambientLight(
                        {.color = hexColor(0x9fb0c8), .intensity = .04f})
                        .key("fill"),
                    world::mesh(bench)
                        .key("bench")
                        .at({0, -kPlateDepth, 0})
                        .rotateX(-90)
                        .fill(material::from(hexColor(0x141312))
                                  .surface({.roughness = .85f})),
                    world::group({world::mesh(plate).key("plate").fill(
                                      material::from(hexColor(0x80848b))
                                          .surface({.metallic = .9f,
                                                    .roughness = .34f})),
                                  world::mesh(face)
                                      .key("face")
                                      .at({0, 0, .5f})
                                      .fill(page.surface()),
                                  world::mesh(alloy)
                                      .key("alloy")
                                      .at(alloyCentre)
                                      .fill(alloySurface)})
                        .key("plate rig")
                        .rotateX(-90)}))
        .environment(room, {.intensity = .5f, .diffuse = .3f})
        .pass(page.render(tree))
        .pass(
            world::bloom({.threshold = 1.0f, .intensity = .9f, .radius = 40}));
  }
};

}  // namespace

SIGIL_SKETCH(PaintedFields, "Study · Materials",
             "Brush strokes written into height, wet and glow maps on a "
             "satin plate: dry ridges catch a travelling key light, wet pools "
             "mirror a soft panel, and an alloy stroke glows and casts")
