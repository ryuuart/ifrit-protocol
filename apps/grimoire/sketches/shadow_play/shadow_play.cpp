/** Forms on a ground before a back wall under three lights that cast: a
 *  sun turning overhead, a spot circling the scene and a lamp standing
 *  among the forms, so every shadow sweeps across the ground and up the
 *  wall. One slab is left out of the spot's shadow by its shadow link,
 *  and one floating cube casts nothing at all.
 */

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <utility>
#include <vector>

namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

namespace {

using material::hexColor;

constexpr int kWidth = 1280, kHeight = 720;
constexpr float kLoop = 8, kPi = 3.14159265358979f;

/** A matte surface of @p colour, rough enough that the shadows read as
 *  shapes rather than as missing highlights. */
material::Material matte(material::Color colour, float roughness = 0.8f) {
  return material::surface::program(
      {.baseColor = colour, .roughness = roughness});
}

/** SEVEN STONES IN AN ARC, one stamp repeated as instanced geometry, each
 *  turned a little more than the one before. */
gm::Instances stones() {
  gm::Instances out;
  out.stamp = gm::box({-26, 0, -26}, {26, 52, 26});
  for (int i = 0; i < 7; ++i) {
    const float angle = kPi * (0.15f + 0.7f * (float)i / 6.0f);
    out.positions.emplace_back(300 * std::cos(angle), 0,
                               120 - 210 * std::sin(angle) * 0.6f);
    out.orientations.push_back(
        glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f)));
    out.scales.emplace_back(1.0f, 0.7f + 0.15f * (float)(i % 3), 1.0f);
  }
  return out;
}

struct ShadowPlay {
  gm::Mesh ground, wall, column, ball, ring, slab, cube, bulb;
  gm::Instances arc;

  void setup(sketch::Scene3DContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.captureAt(1.0);
    ctx.background(hexColor(0x0b0c0f));
    ground = gm::quad(1800, 1300);
    wall = gm::quad(1800, 760);
    column = gm::box({-42, 0, -42}, {42, 280, 42});
    ball = gm::superellipsoid({70, 70, 70}, 2.0f, 72, 48);
    ring = gm::torus(62, 16);
    slab = gm::box({-80, 0, -14}, {80, 200, 14});
    cube = gm::box({-34, -34, -34}, {34, 34, 34});
    bulb = gm::superellipsoid({10, 10, 10}, 2.0f, 24, 16);
    arc = stones();
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const float phase = 2 * kPi * seconds / kLoop;

    gm::camera::Camera lens;
    lens.eye = {0, 430, 1020};
    lens.target = {0, 90, -40};
    lens.verticalFieldOfViewDegrees = 36;
    lens.nearPlane = 40;
    lens.farPlane = 5000;

    // THE SUN turns once round the scene each loop, high and to the left
    // at the start, so every shadow it casts sweeps a full turn.
    const glm::vec3 sun{0.55f * std::cos(phase + 2.4f), -0.8f,
                        0.55f * std::sin(phase + 2.4f) - 0.25f};
    // THE SPOT circles above the front of the scene the other way, always
    // aimed at the middle of the ground.
    const glm::vec3 spotAt{520 * std::cos(-phase + 0.6f), 560,
                           260 + 260 * std::sin(-phase + 0.6f)};

    return world::Frame(world::group(
        {world::camera(
             lens, {.exposure = 1.0f, .toneCurve = world::ToneCurve::Neutral}),
         world::null().key("aim").at({0, 0, -60}),
         world::directionalLight(
             sun, {.color = material::Color{1.0f, 0.95f, 0.86f, 1},
                   .intensity = 2.2f,
                   .castShadow = true,
                   // Three maps along the camera's depth, the nearest
                   // covering the front of the scene, from a sun a degree
                   // and a half across, so a shadow softens away from
                   // its caster.
                   .shadow = {.cascades = 3, .size = 1.5f}})
             .key("sun"),
         world::spotLight({0, -1, 0},
                          {.color = material::Color{0.62f, 0.78f, 1.0f, 1},
                           .intensity = world::intensityAt(2.4f, 700.0f),
                           .innerAngle = 16,
                           .outerAngle = 30,
                           .castShadow = true,
                           .shadow = {.radius = 1.5f, .size = 36}})
             .key("spot")
             .at(spotAt)
             .lookAt("aim")
             // The slab casts the sun's shadow and never the spot's.
             .shadowLink(world::Collection{.exclude = {"screen"}}),
         world::pointLight({.color = material::Color{1.0f, 0.62f, 0.32f, 1},
                            .intensity = world::intensityAt(2.6f, 160.0f),
                            .range = 900,
                            .castShadow = true,
                            // As wide as the bulb that stands for it.
                            .shadow = {.size = 20}})
             .key("lamp")
             .at({70, 60, 150}),
         world::ambientLight({.intensity = 0.06f}).key("fill"),
         // The lamp's own bulb, which is its own light and blocks none.
         world::mesh(bulb)
             .key("bulb")
             .at({70, 60, 150})
             .fill(material::from(material::Color{1.0f, 0.8f, 0.55f, 1})
                       .surface({.unlit = true})),
         world::mesh(ground).key("ground").rotateX(-90).fill(
             matte(hexColor(0xb9b2a6))),
         world::mesh(wall)
             .key("wall")
             .at({0, 380, -520})
             .fill(matte(hexColor(0x9ea4ad))),
         world::mesh(column)
             .key("column")
             .at({-420, 0, -200})
             .rotateY(20)
             .fill(matte(hexColor(0xc75b3c))),
         world::mesh(ball)
             .key("ball")
             .at({-250, 70, -110})
             .fill(matte(hexColor(0x3d6fc4), 0.45f)),
         // Held in the air, so its shadow stands apart from it.
         world::mesh(ring)
             .key("ring")
             .at({60, 250, -150})
             .rotateX(70)
             .rotateY(40 * seconds)
             .fill(matte(hexColor(0xe2b33a), 0.4f)),
         world::mesh(slab)
             .key("slab")
             .collection("screen")
             .at({300, 0, -260})
             .rotateY(-25)
             .fill(matte(hexColor(0x4e9a6a))),
         // Floating, and casting nothing from any light.
         world::mesh(cube)
             .key("ghost")
             .at({-360, 330, 120})
             .rotateY(30 + 30 * seconds)
             .rotateX(25)
             .castShadow(false)
             .fill(matte(hexColor(0xd8d8de), 0.3f)),
         world::mesh(arc)
             .key("stones")
             .at({0, 0, 60})
             .fill(matte(hexColor(0x8a7f74)))}));
  }
};

}  // namespace

SIGIL_SKETCH(ShadowPlay, "Study · Lighting",
             "Forms on a ground before a wall under a turning sun, a "
             "circling spot and a lamp among them, every one casting; one "
             "slab is left out of the spot's shadow and one cube casts "
             "nothing")
