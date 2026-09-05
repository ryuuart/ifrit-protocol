/** @file
 * first_light — one of everything a description can hold.
 *
 * A lit set: a tube SWEPT along a closed loop, a comet of STAMPS riding
 * a moving window of that same loop, a plate under both, a sun and a
 * lamp, and a CAMERA on a rail of its own. The two curves are the
 * world kit's — `kit::wave` for the loop that rises and falls,
 * `kit::turntable` for the camera that circles the set and looks
 * inward — so what this file states is the arrangement and nothing
 * else. Everything is a function of the scene time, so the plate is a
 * function of the declared moment.
 */

#include <sigilgeometry/kit/Sections.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/curve/Pose.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <glm/vec3.hpp>
#include <vector>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace gm = sigil::geometry::mesh;
namespace sections = sigil::geometry::sections;

namespace {

/** The camera's rail, and the seconds it takes to go round once. The
 *  lens's target is written in the rail's own frame, so the aim depends
 *  on the radius and the height rather than on where the camera has
 *  got to. */
constexpr world::kit::Turntable kTable{
    .radius = 470.0f, .height = 190.0f, .period = 16.5f, .stations = 8};

/** The loop both the tube and the comet ride: the kit's wave at its own
 *  stations, which is a ring that stands high and low by turns, so it
 *  reads as a curve in space rather than as a circle seen at an angle. */
gm::curve::Spline3 ribbon() { return world::kit::wave({}); }

struct FirstLight final : sketch::Set {
  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(900, 640);
    ctx.background({0.035f, 0.04f, 0.055f, 1.0f});
    ctx.captureAt(1.4);
  }

  world::Frame describe(float seconds) override {
    const gm::curve::Spline3 loop = ribbon();
    const gm::curve::Spline3 track = world::kit::rail(kTable);

    const gm::Mesh tube =
        gm::pop::sweep(loop, sections::circle(16),
                         {.segments = 220,
                          .scale = 9.0f,
                          .normals = gm::pop::SweepOptions::Normals::Radial});

    const std::vector<glm::vec3> path = loop.sampleArcLength(96);
    const float head = motion::phase(seconds, 1.0 / 0.35);
    // Where the lens stands: the turntable's node carries it, so the
    // pose the rail puts that node in at the distance it has travelled
    // IS the eye, and the comet is turned onto the same point.
    const float travelled = motion::phase(seconds, kTable.period)
                            * track.length();
    const glm::vec3 eye = gm::curve::poseAlong(track, travelled).position;
    // What stands at every point of the comet is a FLAKE, and a flat
    // body reads as the bead it draws only while it faces the viewer —
    // so the direction lane the loop seeded with its tangent is
    // replaced by the gaze, and the square covers the area the bead's
    // disc did.
    const gm::pop::Chain comet =
        gm::pop::on(path)
            .count(1200)
            .spread(13.0f)
            .vary(0.5f, 1.0f)
            .fade({1.0f, 0.72f, 0.35f, 1.0f}, {0.25f, 0.55f, 1.0f, 1.0f})
            .lookAt(eye);

    return world::Element()
        .key("set")
        .child(world::Element().key("sun").light(world::light::sun(
            {-0.45f, -0.8f, -0.4f}, {1.0f, 0.96f, 0.9f, 1.0f}, 0.95f)))
        .child(world::Element()
                   .key("lamp")
                   .at({180, 150, 220})
                   .light(world::light::point({0, 0, 0}, {0.45f, 0.6f, 1.0f, 1.0f},
                                       0.9f, 900.0f)))
        .child(world::kit::turntable(kTable, seconds))
        .child(world::Element()
                   .key("plate")
                   .at({0, -150, 0})
                   .rotateX(-90.0f)
                   .mesh(gm::quad(900, 900))
                   .fill(material::kit::surface(
                       {.baseColor = {0.10f, 0.11f, 0.14f, 1.0f}}))
                   .tag("ground"))
        .child(world::Element()
                   .key("tube")
                   .mesh(tube)
                   .fill(material::kit::surface(
                       {.baseColor = {0.62f, 0.66f, 0.74f, 1.0f}}))
                   .tag("lit"))
        .child(world::Element()
                   .key("comet")
                   .chain(comet)
                   .stamp(gm::quad(7.0f, 7.0f))
                   .window(head, 0.28f)
                   .fill(material::kit::surface(
                       {.baseColor = {0.95f, 0.75f, 0.42f, 1.0f}}))
                   .tag("glow"));
  }
};

}  // namespace

SIGIL_SKETCH(
    FirstLight, "Set",
    "A lit set with one of everything a description can hold \xe2\x80\x94 a "
    "tube swept along the kit's wave, a comet of stamps riding a "
    "window of it, and a turntable camera on a rail of its own")
