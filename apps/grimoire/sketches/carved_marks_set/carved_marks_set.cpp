/** Carved stone and chased copper painted by Compose as a surface's maps
 *  and lit by the set: World's lights, environment and camera shade the
 *  page, and Compose shades nothing.
 */
// TAGS: Compose/Materials, Materials/Stone, Materials/Metal,
// Materials/Lighting, Geometry/3D, Typography/Relief

#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/light/Light.h>

#include <algorithm>
#include <cmath>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <string_view>

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
constexpr int kFieldWidth = 620, kFieldHeight = 292;
constexpr float kLoop = 12, kTurnStarts = 7, kPi = 3.14159265358979f;
constexpr auto kPaper = hexColor(0xe9e3d3);
constexpr auto kInk = hexColor(0x28372f);
constexpr auto kMuted = hexColor(0x3c463d);
constexpr auto kCopper = hexColor(0xb16d46);
constexpr auto kGold = hexColor(0xe6bb70);
constexpr material::Color kWhite{1, 1, 1, 1}, kBlack{0, 0, 0, 1};

/** Ordinary page content: flat type that takes no light. */
Element label(std::string_view content, float x, float y, float width,
              float size, material::Color colour = kMuted) {
  return compose::kit::at(compose::text(std::string(content)), x, y, width,
                          size * 1.6f)
      .fontSize(size)
      .letterSpacing(size * .08f)
      .ink(colour);
}

/** A mark of the shared die: a band swept along @p spine, laid into the
 *  height at @p grey. */
Element swept(const path::Outline& spine, path::Profile width, float grey) {
  return compose::kit::at(0, 0, kFieldWidth, kFieldHeight)
      .shape(compose::heldPath(spine))
      .stroke(compose::brush::ribbon(
          std::move(width), compose::Fill::color({grey, grey, grey, 1})));
}

/** THE LETTER DIE: lettering, a pressure ribbon and two fibre passes in
 *  one opaque grey height, white standing highest. */
Element die() {
  const auto pressure = path::Outline::svg(
      "M24 252 C136 204 225 275 315 239 C420 189 501 259 600 218");
  const auto fibre = path::Outline::svg(
      "M18 224 C151 184 220 270 325 221 C442 170 509 245 608 193");
  const auto crossing = path::Outline::svg(
      "M104 284 C168 221 332 199 491 256 C532 269 562 275 602 268");
  auto field = compose::stack()
                   .width(kFieldWidth)
                   .height(kFieldHeight)
                   .fill(kBlack)
                   .children({
                       compose::kit::at(compose::text("OB8"), 43, 10, 560, 222)
                           .fontFamily("Georgia, Times New Roman, serif")
                           .fontSize(196)
                           .fontWeight(700)
                           .letterSpacing(10)
                           .ink(kWhite)
                           .filter(material::Filter::blur(.9f)),
                       swept(pressure,
                             path::Profile{{0, 1},
                                           {.12f, 12},
                                           {.35f, 27},
                                           {.61f, 8},
                                           {.84f, 20},
                                           {1, 1}},
                             .86f)
                           .opacity(.67f)
                           .filter(material::Filter::blur(.55f)),
                   });
  const path::Profile strand{
      {0, .1f}, {.12f, .8f}, {.43f, 1.3f}, {.76f, .7f}, {1, .1f}};
  for (int pass = 0; pass < 9; ++pass) {
    const float offset = pass * 2.4f - 10;
    field.children(
        {swept(fibre, strand, .7f)
             .translateY(offset)
             .opacity(.40f + .035f * (pass % 6)),
         swept(crossing, strand, .7f).translateY(offset * .5f).opacity(.26f)});
  }
  return field;
}

material::Mask marked(const material::Material& height) {
  return {height, material::MaskChannel::Luminance};
}

/** Cut limestone: the die pressed in, pores and strata from noise, dark
 *  ink settled in the cut. */
material::Material limestone(const material::Material& height) {
  auto stone =
      material::from(hexColor(0xd9d0b8))
          .layer(material::noise(.016f, {.octaves = 3, .grain = true}),
                 {.blend = material::BlendMode::Multiply, .opacity = .35f})
          .layer(material::noise(
                     .4f, {.octaves = 2, .grain = true, .contrast = 1.6f}),
                 {.blend = material::BlendMode::Multiply, .opacity = .18f})
          .layer(hexColor(0x404b38), {.opacity = .62f, .mask = marked(height)});
  const auto normal = material::surface::blendNormals(
      material::surface::normalFromHeight(
          height, {.depth = -2.8f, .step = .75f, .directX = true}),
      material::surface::normalFromHeight(
          material::noise(.13f, {.octaves = 3, .grain = true}),
          {.depth = .45f, .directX = true}),
      {.baseDirectX = true, .detailDirectX = true, .outputDirectX = true});
  return stone.surface(
      {.roughness = .82f, .normal = normal, .normalDirectX = true});
}

/** Chased copper: the same die reversed and reduced into a raised finish,
 *  brushed along its length, gilt where the marks stand. */
material::Material chasedCopper(const material::Material& height) {
  const auto brushing =
      material::noise(.05f, {.octaves = 3, .grain = true, .stretch = 9});
  auto copper =
      material::from(kCopper)
          .layer(brushing,
                 {.blend = material::BlendMode::Multiply, .opacity = .32f})
          .layer(hexColor(0x3b3027), {.opacity = .35f, .mask = marked(height)})
          .layer(kGold, {.opacity = .72f, .mask = marked(height)});
  const auto normal = material::surface::blendNormals(
      material::surface::normalFromHeight(
          height, {.depth = 1.6f, .step = .75f, .directX = true}),
      material::surface::normalFromHeight(brushing,
                                          {.depth = .35f, .directX = true}),
      {.baseDirectX = true, .detailDirectX = true, .outputDirectX = true});
  return copper.surface({.metallic = .9f,
                         .roughness = .34f,
                         .normal = normal,
                         .normalDirectX = true});
}

/** Gold leaf restarted on every glyph, hammered by a grain of its own. */
material::Material hammeredGold() {
  return material::from(kGold).surface(
      {.metallic = .95f,
       .roughness = .22f,
       .normal = material::surface::normalFromHeight(
           material::noise(.08f, {.octaves = 3, .grain = true}),
           {.depth = 2.2f, .directX = true}),
       .normalDirectX = true});
}

/** The page the set wears: two marked slabs, gilt glyphs, a relief and
 *  the captions that take no light. */
Element page(const material::Material& height) {
  const auto slab = [&](float x, std::string_view title, std::string_view note,
                        const material::Material& face, bool copper) {
    const auto stock =
        copper
            ? material::from(hexColor(0x8a5236))
                  .layer(
                      material::noise(
                          .05f, {.octaves = 3, .grain = true, .stretch = 9}),
                      {.blend = material::BlendMode::Multiply, .opacity = .3f})
                  .surface({.metallic = .85f, .roughness = .45f})
            : material::from(hexColor(0xcfc6ae))
                  .layer(
                      material::noise(.02f, {.octaves = 3, .grain = true}),
                      {.blend = material::BlendMode::Multiply, .opacity = .3f})
                  .surface({.roughness = .9f});
    const auto text = copper ? kPaper : kInk;
    return compose::kit::at(compose::stack(), x, 104, 668, 412)
        .fill(stock)
        .borderRadius(10)
        .children({label(title, 24, 18, 360, 12, text),
                   label(note, 408, 20, 236, 10, text),
                   compose::kit::at(24, 56, kFieldWidth, kFieldHeight)
                       .fill(face)
                       .borderRadius(3),
                   label(copper ? "RAISED +1.60 PX · GILT IN THE MARKS"
                                : "CUT −2.80 PX · INK IN THE CUT",
                         24, 366, 600, 10, text)});
  };
  const auto paper =
      material::from(kPaper)
          .layer(material::noise(.6f, {.octaves = 2, .grain = true}),
                 {.blend = material::BlendMode::Multiply, .opacity = .08f})
          .surface({.roughness = .95f});
  return compose::stack().width(kWidth).height(kHeight).fill(paper).children({
      label("CARVED / MARKS — THE PAGE AS A SURFACE", 40, 36, 900, 15, kInk),
      label("COMPOSE PAINTS THE MAPS · THE SET'S LIGHTS SHADE THEM", 40, 64,
            900, 11),
      slab(40, "01 / CUT LIMESTONE", "PORES / INKED INCISIONS",
           limestone(height), false),
      slab(732, "02 / CHASED COPPER", "BRUSHED / PARCEL GILT",
           chasedCopper(height), true),
      compose::kit::at(compose::text("GILT OB8"), 40, 560, 680, 150)
          .fontFamily("Georgia, Times New Roman, serif")
          .fontSize(112)
          .fontWeight(700)
          .ink(hammeredGold(), compose::PaintBox::Glyph),
      compose::kit::at(compose::text("CHASE"), 732, 560, 668, 150)
          .fontFamily("Georgia, Times New Roman, serif")
          .fontSize(118)
          .fontWeight(700)
          .ink(material::Color{0, 0, 0, 0})
          .decorationOutline(compose::Boundary::Glyphs)
          .foreground(compose::relief(material::from(kCopper).surface(
                                          {.metallic = .9f, .roughness = .3f}),
                                      {.shoulder = 4, .depth = 1.2f})),
      label("GLYPH INK · HAMMERED GOLD RESTARTS ON EVERY LETTER", 40, 724, 660,
            10),
      label("RELIEF · A BEVEL OFF THE GLYPH CONTOURS", 732, 724, 668, 10),
      compose::kit::at(40, 760, 1360, 1).fill(kMuted),
      label("ONE FLAT WORLD PLATE · SUN, RAKING POINT, STUDIO ENVIRONMENT · "
            "12 S: STRAIGHT ON, THEN THE PLATE TURNS",
            40, 776, 1360, 10),
  });
}

/** A studio of two soft boxes over a graded room. */
material::EnvironmentMap studio() {
  return material::EnvironmentMap::baked(256, [](float u, float v) {
    const auto box = [u, v](float centre, float wide, float high) {
      const float across =
          std::min(std::abs(u - centre), 1 - std::abs(u - centre)) / wide;
      const float down = (v - .42f) / high;
      return std::exp(-.5f * (across * across + down * down));
    };
    glm::vec3 colour{.20f + .55f * (1 - v), .21f + .56f * (1 - v),
                     .24f + .60f * (1 - v)};
    colour += glm::vec3{5.0f, 4.4f, 3.4f} * box(.18f, .06f, .2f);
    colour += glm::vec3{1.6f, 2.0f, 2.5f} * box(.70f, .05f, .26f);
    return colour;
  });
}

/** How far the plate is turned at @p seconds: square on until the turn
 *  starts, then one full swing either way and back to square. */
float yawAt(double seconds) {
  const float t = float(std::fmod(seconds, double(kLoop)));
  if (t < kTurnStarts) return 0;
  return 26 * std::sin(2 * kPi * (t - kTurnStarts) / (kLoop - kTurnStarts));
}

struct CarvedMarksSet {
  std::shared_ptr<compose::TextureScene> heightScene;
  std::shared_ptr<compose::SurfaceScene> pageScene;
  material::EnvironmentMap room;
  Element tree = compose::stack();
  gm::Mesh plate;

  static gm::camera::Camera lens() {
    gm::camera::Camera camera;
    camera.eye = {0, 0, 1540};
    camera.target = {0, 0, 0};
    camera.fovYDeg = 38;
    camera.zNear = 40;
    camera.zFar = 4000;
    return camera;
  }

  void setup(sketch::SetContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.captureAt(2.0);
    ctx.background(hexColor(0x141716));
    ctx.camera(lens());
    heightScene = ctx.textureScene({kFieldWidth, kFieldHeight}, kBlack);
    heightScene->render(die());
    const auto height =
        material::image(heightScene->texture()
                            .tile(material::Repeat::Pad)
                            .sampling(material::Sampling::Linear));
    pageScene = ctx.surfaceScene({kWidth, kHeight});
    tree = page(height);
    room = studio();
    // One flat sheet, cut finely enough that a tier shading per vertex
    // and mapping a texture across each triangle without perspective
    // still reads it as a plane when it turns.
    plate = gm::grid(36, 24, [](float u, float v) {
      return glm::vec3{(u - .5f) * kWidth, (v - .5f) * kHeight, 0};
    });
  }

  world::Frame describe(double seconds) {
    const double now = std::isfinite(seconds) ? std::max(0.0, seconds) : 0;
    pageScene->render(tree, now);
    auto surface =
        material::surface::program(pageScene->maps(), {.baseColor = kWhite});
    const float phase = 2 * kPi * float(std::fmod(now, double(kLoop))) / kLoop;
    world::Element root;
    root.camera(lens()).children({
        world::Element().key("studio").environmentMap(
            {.map = room, .intensity = 1.4f, .diffuse = .5f, .exposure = 1.2f}),
        world::Element().key("key").light(
            world::light::sun({.45f, -.35f, -.82f}, {1, .93f, .8f, 1}, 1.8f)),
        world::Element().key("sweep").light(world::light::point(
            {640 * std::sin(phase), 260 * std::cos(phase), 220},
            {1, .97f, .9f, 1}, 2.2f, 3200)),
        world::Element()
            .key("plate")
            .rotateY(yawAt(now))
            .mesh(plate)
            .backface(material::Backface::Visible)
            .fill(std::move(surface)),
    });
    return world::Frame(root).extent({kWidth, kHeight});
  }
};

}  // namespace

SIGIL_SKETCH(CarvedMarksSet, "Study · Materials",
             "Carved stone and chased copper painted as a surface's maps and "
             "lit by the set's lights, environment and turning view")
