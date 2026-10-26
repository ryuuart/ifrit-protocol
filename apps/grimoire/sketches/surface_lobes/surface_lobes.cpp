/** The optional lobes of the physically based surface, each worn by one
 *  ball beside a plain one: a sheen on cloth, a brushing at two turns on
 *  metal, a thin film at two thicknesses, a dielectric's specular strength
 *  and colour, a clear coat for reference — and a Compose page whose
 *  brushed letters stand on a sheened ground, lit by the same set.
 */
// TAGS: Materials/Lighting, Materials/Metal, Geometry/3D,
// Compose/Materials

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

namespace {

using material::hexColor;

constexpr int kWidth = 1280, kHeight = 720;
constexpr int kPageWidth = 720, kPageHeight = 480;
constexpr float kLoop = 8, kPi = 3.14159265358979f;
constexpr float kSpacing = 150, kRadius = 60;

/** A material that states a colour and a lit response. */
material::Material lit(material::Color colour,
                       const material::SurfaceOptions& response) {
  return material::from(colour).surface(response);
}

/** THE BALLS, in their rows: each lobe beside the same surface stating
 *  nothing more. */
struct Ball {
  std::string key;
  material::Material surface;
};

std::vector<Ball> upperRow() {
  const material::Color wine = hexColor(0x5a1626);
  const material::Color steel = hexColor(0xe8e8ee);
  const material::Color titanium = hexColor(0x9a9aa2);
  const material::SurfaceOptions cloth{.roughness = 0.9f};
  const material::SurfaceOptions metal{.metallic = 1.0f, .roughness = 0.32f};
  const material::SurfaceOptions polished{.metallic = 1.0f, .roughness = 0.12f};
  material::SurfaceOptions velvet = cloth;
  velvet.sheenColor = hexColor(0xffc4d0);
  velvet.sheenRoughness = 0.35f;
  material::SurfaceOptions brushed = metal;
  brushed.anisotropy = 0.9f;
  material::SurfaceOptions turned = brushed;
  turned.anisotropyRotation = 90;
  material::SurfaceOptions thinFilm = polished;
  thinFilm.iridescence = 1.0f;
  thinFilm.iridescenceIor = 1.33f;
  thinFilm.iridescenceThickness = 320;
  material::SurfaceOptions thickFilm = thinFilm;
  thickFilm.iridescenceThickness = 560;
  return {{"cloth", lit(wine, cloth)},
          {"sheen", lit(wine, velvet)},
          {"metal", lit(steel, metal)},
          {"brushed around", lit(steel, brushed)},
          {"brushed over", lit(steel, turned)},
          {"polished", lit(titanium, polished)},
          {"film thin", lit(titanium, thinFilm)},
          {"film thick", lit(titanium, thickFilm)}};
}

std::vector<Ball> lowerRow() {
  const material::Color blue = hexColor(0x2a5c99);
  const material::Color red = hexColor(0x9a2414);
  const material::SurfaceOptions plastic{.roughness = 0.3f};
  const material::SurfaceOptions matte{.roughness = 0.75f};
  material::SurfaceOptions dull = plastic;
  dull.specular = 0.1f;
  material::SurfaceOptions tinted = plastic;
  tinted.specularColor = hexColor(0xffb060);
  tinted.specular = 1.0f;
  material::SurfaceOptions coated = matte;
  coated.clearcoat = 1.0f;
  coated.clearcoatRoughness = 0.05f;
  return {{"plastic", lit(blue, plastic)},
          {"specular dull", lit(blue, dull)},
          {"specular tinted", lit(blue, tinted)},
          {"matte", lit(red, matte)},
          {"coated", lit(red, coated)}};
}

/** THE PAGE, in two bands, each a lobe over its plain neighbour: a
 *  velvet ground stating a sheen above the same ground stating none, and
 *  lettering of brushed steel, brushed along the page's width, above the
 *  same steel polished. */
compose::Element lobedPage() {
  const material::Color navy = hexColor(0x14204a);
  const material::Color steel = hexColor(0xdcdde4);
  const material::Material velvet = lit(navy, {.roughness = 0.9f,
                                               .sheenColor = hexColor(0xa8c0ff),
                                               .sheenRoughness = 0.4f});
  const material::Material felt = lit(navy, {.roughness = 0.9f});
  const material::Material brushedSteel =
      lit(steel, {.metallic = 1.0f, .roughness = 0.2f, .anisotropy = 0.95f});
  const material::Material polishedSteel =
      lit(steel, {.metallic = 1.0f, .roughness = 0.2f});
  const auto word = [](const char* content, float y,
                       const material::Material& finish) {
    return compose::kit::at(compose::text(content), 40, y, 640, 160)
        .fontFamily("Helvetica Neue, Arial, sans-serif")
        .fontSize(118)
        .fontWeight(800)
        .letterSpacing(4)
        .ink(finish, compose::PaintBox::Glyph);
  };
  return compose::stack()
      .width(kPageWidth)
      .height(kPageHeight)
      .children({
          compose::kit::at(0, 0, kPageWidth, kPageHeight / 2).fill(velvet),
          compose::kit::at(0, kPageHeight / 2, kPageWidth, kPageHeight / 2)
              .fill(felt),
          word("BRUSHED", 40, brushedSteel),
          word("POLISHED", 280, polishedSteel),
      });
}

/** A studio of two soft boxes over a graded room. */
world::EnvironmentMap studio() {
  return world::kit::environmentMap(
      {.zenith = {.55f, .58f, .66f},
       .nadir = {.10f, .10f, .12f},
       .softBoxes = {{.centre = {.30f, .40f},
                      .size = {.08f, .18f},
                      .radiance = {5.0f, 4.6f, 4.0f}},
                     {.centre = {.72f, .44f},
                      .size = {.05f, .24f},
                      .radiance = {1.4f, 1.9f, 2.6f}}}});
}

struct SurfaceLobes {
  sketch::Page page;
  world::EnvironmentMap room;
  compose::Element tree = compose::stack();
  gm::Mesh ball;
  gm::Mesh plate;

  static gm::camera::Camera lens() {
    gm::camera::Camera camera;
    camera.eye = {0, 80, 1750};
    camera.target = {0, 0, 0};
    camera.verticalFieldOfViewDegrees = 24.5f;
    camera.nearPlane = 40;
    camera.farPlane = 4000;
    return camera;
  }

  void setup(sketch::Scene3DContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.captureAt(2.0);
    ctx.background(hexColor(0x101113));
    page = ctx.page({kPageWidth, kPageHeight});
    tree = lobedPage();
    room = studio();
    ball = gm::superellipsoid({kRadius, kRadius, kRadius}, 2.0f, 96, 64);
    plate = gm::quad(kPageWidth * 0.56f, kPageHeight * 0.56f);
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float phase =
        2 * kPi * float(std::fmod(now.count(), double(kLoop))) / kLoop;
    // The key swings across the front of the row and back; every ball
    // turns once about its own axis over the loop, carrying a brushing
    // round with it.
    const float swing = 0.75f * std::sin(phase);
    const float turn =
        360.0f * float(std::fmod(now.count(), double(kLoop))) / kLoop;
    std::vector<world::Node> nodes{
        world::camera(lens(), {.exposure = 0.85f,
                               .toneCurve = world::ToneCurve::Neutral}),
        world::directionalLight(
            {swing, -0.45f, -0.8f},
            {.color = material::Color{1, .95f, .88f, 1}, .intensity = 2.2f})
            .key("key"),
        // A light from behind and above the row, which a sheen sends back
        // along its rim and the laid-back page's velvet catches at a
        // grazing angle.
        world::directionalLight(
            {0.1f, -0.55f, 0.85f},
            {.color = material::Color{.85f, .9f, 1, 1}, .intensity = 1.6f})
            .key("rim"),
        // A lamp hanging over the far edge of the page, where the eye sees
        // it mirrored in the lettering: the brushing draws its highlight
        // out into a line across the page's width.
        world::pointLight({.color = material::Color{1, .97f, .92f, 1},
                           .intensity = world::intensityAt(2.0f, 260.0f)})
            .key("lamp")
            .at({375 + 160 * std::sin(phase), 70, -75}),
        world::mesh(plate)
            .key("page")
            .at({390, -165, 0})
            .rotateX(-58)
            .rotateY(-8 + 8 * std::sin(phase))
            .backface(material::Backface::Visible)
            .fill(page.surface())};
    const auto row = [&](std::vector<Ball> balls, float y) {
      for (size_t i = 0; i < balls.size(); ++i)
        nodes.push_back(world::mesh(ball)
                            .key(std::move(balls[i].key))
                            .at({-525 + kSpacing * float(i), y, 0})
                            .rotateY(turn)
                            .fill(std::move(balls[i].surface)));
    };
    row(upperRow(), 140);
    row(lowerRow(), -150);
    return world::Frame(world::group(std::move(nodes)))
        .environment(room, {.intensity = 0.8f, .diffuse = .6f})
        .pass(page.render(tree));
  }
};

}  // namespace

SIGIL_SKETCH(SurfaceLobes, "Study · Materials",
             "Sheen, brushed anisotropy, thin-film iridescence, a "
             "dielectric's specular and a clear coat, each beside a plain "
             "neighbour, and a page wearing two of them")
