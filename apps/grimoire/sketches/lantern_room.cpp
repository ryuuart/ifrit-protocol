/** @file
 * lantern_room — a dark room, and the three kinds of emitter in it.
 *
 * A cluster of pale bodies stands on a plinth. Four lanterns ring them,
 * each an UNLIT shell — its own light, shaded by nothing — with a
 * coloured POINT light inside it. A SPOT opens downward onto the middle
 * of the cluster, and under everything a SUN so faint it describes the
 * room rather than lighting it. Those are the three emitters this
 * library has, standing in one picture, and the bodies between them are
 * what the picture is read on: each one takes warm from one side and
 * cold from another, which is a thing a single emitter cannot say.
 *
 * AN EMITTER STANDS WHERE ITS NODE STANDS and carries no geometry, so a
 * lantern here is two siblings — a shell that can be seen and a light
 * that cannot — placed together. Nothing binds them but the placement
 * they share, which is why moving one number moves both.
 *
 * WHAT THIS STUDY DOES NOT SHOW is a lamp's pool on the ground. The
 * device shades an emitter PER VERTEX, so a falloff is only ever as
 * detailed as the surface it lands on, and the plinth below is small and dark
 * rather than a floor pretending to carry a gradient it cannot. The colours on
 * the bodies are the honest reading of where the light is.
 */

// TAGS: Materials/Lighting

#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <array>
#include <cmath>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr float kTwoPi = 6.283185307179586f;
/** The room: where the plinth's top stands, how wide it is, and how far
 *  out the lanterns ring the cluster. */
constexpr float kFloor = -96.0f;
constexpr float kPlinth = 560.0f;
constexpr float kRing = 260.0f;
/** How far a lantern's light carries. Enough to cross the cluster and
 *  not the room, so a body reads which lantern it is nearest. */
constexpr float kReach = 620.0f;

/** WHERE EACH LANTERN STANDS AND WHAT COLOUR IT IS. Four of them, a
 *  quarter turn apart, so the arrangement says nothing and the colour
 *  says everything. */
struct Lantern {
  const char* key;
  float bearingDegrees;
  float height;
  glm::vec4 color;
};

constexpr std::array<Lantern, 4> kLanterns = {
    Lantern{"amber", 20.0f, 130.0f, {1.00f, 0.58f, 0.20f, 1.0f}},
    Lantern{"cyan", 110.0f, 190.0f, {0.22f, 0.78f, 1.00f, 1.0f}},
    Lantern{"rose", 200.0f, 150.0f, {1.00f, 0.28f, 0.50f, 1.0f}},
    Lantern{"lime", 290.0f, 210.0f, {0.44f, 0.95f, 0.40f, 1.0f}}};

/** THE CLUSTER: bodies of three heights, off-centre from each other, so
 *  every lantern reaches some of them squarely and the rest at a
 *  glancing angle. */
struct Body {
  glm::vec3 at;
  glm::vec3 radii;
  float exponent;
};

constexpr std::array<Body, 5> kBodies = {
    Body{{0.0f, 84.0f, 0.0f}, {46.0f, 96.0f, 46.0f}, 3.2f},
    Body{{-98.0f, 46.0f, 34.0f}, {38.0f, 58.0f, 38.0f}, 1.6f},
    Body{{92.0f, 54.0f, -28.0f}, {42.0f, 66.0f, 42.0f}, 5.0f},
    Body{{18.0f, 34.0f, 104.0f}, {34.0f, 46.0f, 34.0f}, 2.0f},
    Body{{-42.0f, 30.0f, -108.0f}, {30.0f, 42.0f, 30.0f}, 1.2f}};

/** The plinth: dark and small, so the bodies are what carries the light
 *  and the ground is what they stand on. */
gm::Mesh plinth() {
  return gm::superellipsoid({kPlinth * 0.5f, 16.0f, kPlinth * 0.5f}, 8.0f, 48,
                            16);
}

/** A lantern's shell: a body that says light does not reach it, so what
 *  it shows is its own colour and nothing the room does to it. */
material::Material glow(glm::vec4 color) {
  return material::surface::unlit(
      {.baseColor = {color.r, color.g, color.b, 1.0f},
       .emissive = {color.r, color.g, color.b, 1.0f},
       .emissiveStrength = 2.6f});
}

}  // namespace

namespace {

struct LanternRoom {
  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = {900, 600},
              .captureAt = 1.6,
              .background = material::Color{0.010f, 0.012f, 0.020f, 1.0f}});
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    std::vector<world::Node> lanterns;
    for (const Lantern& lantern : kLanterns) {
      // Each lantern rides a slow bob of its own, so what it reaches
      // changes: a falloff is a function of distance, and the clearest
      // way to say so is to move one lamp and leave the rest.
      const float bearing = lantern.bearingDegrees * kTwoPi / 360.0f;
      const float bob =
          std::sin(seconds * 0.75f + lantern.bearingDegrees * 0.03f);
      // A bearing is measured from +z and turns toward +x, so the
      // ellipse's two components land on z and on x in that order.
      const SkPoint on = sigil::geometry::path::toSk(
          arrange::onEllipse({0, 0}, {kRing, kRing}, bearing));
      const glm::vec3 at{on.fY, kFloor + lantern.height + 42.0f * bob, on.fX};
      lanterns.push_back(
          world::mesh(gm::superellipsoid({19.0f, 26.0f, 19.0f}, 1.4f, 24, 16))
              .key(std::string(lantern.key) + "-shell")
              .at(at)
              .fill(glow(lantern.color))
              .collection("lantern"));
      // …and the light at the same place, a sibling rather than a child:
      // a light carries no geometry, and nothing about a light is welded
      // to a body.
      lanterns.push_back(
          world::pointLight(
              {.color = material::Color{lantern.color.r, lantern.color.g,
                                        lantern.color.b, lantern.color.a},
               .intensity = world::intensityAt(2.0f, kRing),
               .range = kReach,
               .castShadow = true})
              .key(std::string(lantern.key) + "-lamp")
              .at(at)
              .collection("lamp"));
    }

    return world::group(
               {world::mesh(plinth())
                    .key("plinth")
                    .at({0.0f, kFloor, 0.0f})
                    .fill(material::surface::program(
                        {.baseColor = {0.13f, 0.13f, 0.16f, 1.0f},
                         .roughness = 0.8f}))
                    .collection("ground"),
                // A directional light so faint it is an outline rather
                // than a light: what keeps the far side of every body from
                // being nothing at all.
                world::directionalLight(
                    {-0.35f, -0.85f, -0.4f},
                    {.color = material::Color{0.52f, 0.60f, 0.86f, 1.0f},
                     .intensity = 0.55f})
                    .key("sun"),
                // The spot: opening downward onto the middle of the
                // cluster, so the tallest body is picked out from above
                // while the lanterns reach it from the sides.
                world::spotLight(
                    {0.0f, -1.0f, 0.0f},
                    {.color = material::Color{1.0f, 0.96f, 0.88f, 1.0f},
                     .intensity = world::intensityAt(1.4f, 600.0f),
                     .range = 900.0f,
                     .innerAngle = 8.0f,
                     .outerAngle = 22.0f})
                    .key("spot")
                    .at({0.0f, 520.0f, 0.0f}),
                lanterns,
                world::each(
                    kBodies,
                    [](const Body& body, size_t i) {
                      return world::mesh(gm::superellipsoid(
                                             body.radii, body.exponent, 40, 26))
                          .key("body" + std::to_string(i))
                          .at({body.at.x, kFloor + body.at.y, body.at.z})
                          .fill(material::surface::program(
                              {.baseColor = {0.52f, 0.53f, 0.57f, 1.0f},
                               .roughness = 0.5f}))
                          .collection("body");
                    }),
                // The camera is the kit's turntable and nothing else is:
                // this study lights its own room, so the preset that puts
                // three lamps over a ground plane would be describing a
                // second room on top of it.
                world::kit::turntable({.radius = 760.0f,
                                       .height = 260.0f,
                                       .period = sigil::motion::Duration(24.0),
                                       .fieldOfView = 44.0f},
                                      now)})
        .key("room");
  }
};

}  // namespace

SIGIL_SKETCH(LanternRoom, "Scene3D",
             "The three kinds of light in one dark room — four coloured "
             "lanterns that are their own light and carry a lamp each, a "
             "spot from above, and a directional light faint enough to be "
             "an outline")
