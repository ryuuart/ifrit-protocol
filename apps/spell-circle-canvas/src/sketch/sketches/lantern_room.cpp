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
 * WHAT THIS STUDY DOES NOT SHOW is a lamp's pool on the ground. Both
 * tiers shade PER VERTEX, so a falloff is only ever as detailed as the
 * surface it lands on, and the plinth below is small and dark rather
 * than a floor pretending to carry a gradient it cannot. The colours on
 * the bodies are the honest reading of where the light is.
 */

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <array>
#include <cmath>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;

using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;

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
  float bearingDeg;
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
  return material::kit::unlit({.baseColor = {color.r, color.g, color.b, 1.0f},
                               .emissive = {color.r, color.g, color.b, 1.0f},
                               .emissiveStrength = 2.6f});
}

}  // namespace

namespace {

struct LanternRoom final : sketch::Set {
  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(900, 600);
    ctx.background({0.010f, 0.012f, 0.020f, 1.0f});
    ctx.captureAt(1.6);
  }

  world::Frame describe(float seconds) override {
    Element room = Element().key("room");

    room.child(
        Element()
            .key("plinth")
            .at({0.0f, kFloor, 0.0f})
            .mesh(plinth())
            .fill(material::kit::surface(
                {.baseColor = {0.13f, 0.13f, 0.16f, 1.0f}, .roughness = 0.8f}))
            .tag("ground"));

    // A sun so faint it is an outline rather than a light: what keeps
    // the far side of every body from being nothing at all.
    room.child(Element().key("sun").light(
        sun({-0.35f, -0.85f, -0.4f}, {0.52f, 0.60f, 0.86f, 1.0f}, 0.22f)));

    // The spot: opening downward onto the middle of the cluster, so the
    // tallest body is picked out from above while the lanterns reach it
    // from the sides.
    room.child(Element().key("spot").light(
        spot({0.0f, 520.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 22.0f, 8.0f,
             {1.0f, 0.96f, 0.88f, 1.0f}, 0.9f, 900.0f)));

    for (const Lantern& lantern : kLanterns) {
      // Each lantern rides a slow bob of its own, so what it reaches
      // changes: a falloff is a function of distance, and the clearest
      // way to say so is to move one lamp and leave the rest.
      const float bearing = lantern.bearingDeg * kTwoPi / 360.0f;
      const float bob = std::sin(seconds * 0.75f + lantern.bearingDeg * 0.03f);
      const glm::vec3 at{kRing * std::sin(bearing),
                         kFloor + lantern.height + 42.0f * bob,
                         kRing * std::cos(bearing)};
      room.child(
          Element()
              .key(std::string(lantern.key) + "-shell")
              .at(at)
              .mesh(gm::superellipsoid({19.0f, 26.0f, 19.0f}, 1.4f, 24, 16))
              .fill(glow(lantern.color))
              .tag("lantern"));
      // …and the emitter at the same place, a sibling rather than a
      // child: a node that is only an emitter carries no geometry, and
      // nothing about a light is welded to a body.
      room.child(Element()
                     .key(std::string(lantern.key) + "-lamp")
                     .at(at)
                     .light(point({0, 0, 0}, lantern.color, 1.25f, kReach))
                     .tag("lamp"));
    }

    for (size_t i = 0; i < kBodies.size(); ++i) {
      const Body& body = kBodies[i];
      room.child(
          Element()
              .key("body" + std::to_string(i))
              .at({body.at.x, kFloor + body.at.y, body.at.z})
              .mesh(gm::superellipsoid(body.radii, body.exponent, 40, 26))
              .fill(material::kit::surface(
                  {.baseColor = {0.52f, 0.53f, 0.57f, 1.0f},
                   .roughness = 0.5f}))
              .tag("body"));
    }

    // The camera is the kit's turntable and nothing else is: this study
    // lights its own room, so the preset that puts three lamps over a
    // ground plane would be describing a second room on top of it.
    room.child(kit::turntable({.at = {0.0f, 0.0f, 0.0f},
                               .radius = 760.0f,
                               .height = 260.0f,
                               .period = 24.0f,
                               .fovYDeg = 44.0f},
                              seconds));
    return Frame(std::move(room));
  }
};

}  // namespace

SIGIL_SKETCH(LanternRoom, "Set",
             "The three emitters in one dark room \xe2\x80\x94 four coloured "
             "lanterns that are their own light and carry a lamp each, a "
             "spot from above, and a sun faint enough to be an outline")
