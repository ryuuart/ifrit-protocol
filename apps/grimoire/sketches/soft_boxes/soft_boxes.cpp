/** Lights with a size over a row of forms from glossy to rough: a large
 *  rect softbox overhead, a narrow strip light at the side, a sphere light
 *  and a disk light, each made visible by a panel or a ball of its own
 *  shape; the softbox slides and turns, so its reflected rectangle slides
 *  across the glossy forms and stretches on the rough ones.
 */
// TAGS: Materials/Lighting, Materials/Metal, Geometry/3D

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/World.h>
#include <sigilworld/kit/Kit.h>

#include <array>
#include <cmath>
#include <string>
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
constexpr float kRadius = 80, kSpacing = 200;

/** The row's roughness, from a mirror to a matte. */
constexpr std::array<float, 5> kRoughness = {0.02f, 0.12f, 0.25f, 0.45f, 0.75f};

material::Material steel(float roughness) {
  return material::from(hexColor(0xd4d6dc))
      .surface({.metallic = 1.0f, .roughness = roughness});
}

/** A black lacquer under a clear coat. */
material::Material lacquer() {
  return material::from(hexColor(0x14151a))
      .surface(
          {.roughness = 0.6f, .clearcoat = 1.0f, .clearcoatRoughness = 0.03f});
}

struct SoftBoxes {
  gm::Mesh ball;
  gm::Mesh floor;

  static gm::camera::Camera lens() {
    gm::camera::Camera camera;
    camera.eye = {0, 640, 1800};
    camera.target = {0, 60, 0};
    camera.fovYDeg = 38.0f;
    camera.zNear = 40;
    camera.zFar = 6000;
    return camera;
  }

  void setup(sketch::SetContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.captureAt(2.0);
    ctx.background(hexColor(0x07080a));
    ball = gm::superellipsoid({kRadius, kRadius, kRadius}, 2.0f, 96, 64);
    floor = gm::quad(3600, 2400);
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const float phase = 2 * kPi * seconds / kLoop;
    // The softbox glides across the back of the row and returns, turning
    // a little toward where it is going.
    const float glide = 330.0f * std::sin(phase);
    const float swing = 18.0f * std::sin(phase);

    std::vector<world::Node> nodes{
        world::camera(
            lens(), {.exposure = 0.9f, .toneCurve = world::ToneCurve::Neutral}),
        world::ambientLight({.color = hexColor(0x9fb0c8), .intensity = 0.03f})
            .key("fill"),
        // THE SOFTBOX: a large panel above and behind the row, facing
        // down toward it and the camera, so the eye sees its face and its
        // reflection on the forms' tops and on the ground before them.
        world::kit::visibleLight(
            world::rectLight(
                {0, -0.62f, 0.78f},
                {.width = 460,
                 .height = 240,
                 .color = hexColor(0xfff3e4),
                 .intensity = 6.0f,
                 .castShadow = true,
                 .shadow = {.bias = 3, .normalBias = 2, .radius = 3}})
                .key("softbox"))
            .key("softbox rig")
            .at({glide, 470, -620})
            .rotateY(swing),
        // THE STRIP: a narrow upright panel at the front left, facing the
        // row and the eye.
        world::kit::visibleLight(
            world::rectLight({0, 0, -1}, {.width = 34,
                                          .height = 520,
                                          .color = hexColor(0xbcd4ff),
                                          .intensity = 9.0f})
                .key("strip"))
            .key("strip rig")
            .at({-780, 200, 480})
            .rotateY(-127),
        // A WARM BALL low at the right, among the forms' reflections.
        world::kit::visibleLight(
            world::sphereLight({.radius = 26,
                                .color = hexColor(0xffb070),
                                .intensity = world::intensityAt(1.8f, 320.0f)})
                .key("bulb"))
            .key("bulb rig")
            .at({600, 110, 300}),
        // A ROUND PANEL overhead at the back left, tipped toward the eye.
        world::kit::visibleLight(
            world::diskLight({0, -0.8f, 0.6f},
                             {.radius = 70,
                              .color = hexColor(0xd8ffe8),
                              .intensity = world::intensityAt(1.4f, 420.0f)})
                .key("disk"))
            .key("disk rig")
            .at({-520, 440, -300}),
        world::mesh(floor)
            .key("ground")
            .at({0, -kRadius, 0})
            .rotateX(-90)
            .fill(material::from(hexColor(0x8a8c90))
                      .surface({.roughness = 0.14f})),
        // The row ends in a lacquered ball, whose clear coat answers the
        // panels with a second, sharper reflection.
        world::mesh(ball)
            .key("lacquer")
            .at({kSpacing * 3.0f, 0, 0})
            .fill(lacquer()),
    };
    for (size_t i = 0; i < kRoughness.size(); ++i)
      nodes.push_back(world::mesh(ball)
                          .key("form " + std::to_string(i))
                          .at({kSpacing * (float(i) - 2.5f), 0, 0})
                          .fill(steel(kRoughness[i])));
    return world::Frame(world::group(std::move(nodes)));
  }
};

}  // namespace

SIGIL_SKETCH(SoftBoxes, "Study · Lighting",
             "A softbox, a strip, a sphere and a disk light, each seen as a "
             "panel or a ball, over a row of steel from mirror to matte and "
             "a lacquered ball; the softbox's rectangle slides across them")
