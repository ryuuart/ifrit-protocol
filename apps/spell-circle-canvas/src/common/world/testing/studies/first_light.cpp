/** @file
 * first_light — the first thing this library drew.
 *
 * A lit set with one of everything the description can hold: a tube
 * SWEPT along a closed loop, a comet of STAMPS riding a moving window of
 * that same loop, a plate under both, a sun and a lamp, and a CAMERA on
 * a rail of its own. Everything is a function of the scene time, so the
 * plate is a function of the declared moment.
 */

#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Pop.h>

#include <cmath>
#include <glm/vec4.hpp>
#include <memory>
#include <vector>

#include "Studies.h"

namespace sigil::world::testing {

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr float kTwoPi = 6.283185307179586f;
/** How far out the camera's rail stands, and how high. The lens's target
 *  is written in the rail's own frame and depends on both. */
constexpr float kRailRadius = 470.0f;
constexpr float kRailHeight = 190.0f;

struct Paint {
  glm::vec4 baseColor{1, 1, 1, 1};
};

material::Material paint(glm::vec4 colour) {
  static const std::shared_ptr<const material::Recipe> recipe =
      std::make_shared<const material::Recipe>(
          material::Recipe::of<Paint>("world.study.paint"));
  return material::Material(recipe, Paint{colour});
}

/** The loop both the tube and the comet ride: a ring that rises and
 *  falls, so it reads as a curve in space rather than as a circle seen
 *  at an angle. */
Spline3 ribbon() {
  Spline3 spline;
  for (int i = 0; i < 6; ++i) {
    const float angle = (float)i * kTwoPi / 6.0f;
    const float radius = (i % 2 == 0) ? 210.0f : 130.0f;
    const float height = (i % 2 == 0) ? 80.0f : -60.0f;
    spline.points.emplace_back(radius * std::cos(angle), height,
                               radius * std::sin(angle));
  }
  spline.closed = true;
  return spline;
}

/** …and the rail the camera rides, a circle around the set. */
Spline3 rail() {
  Spline3 spline;
  for (int i = 0; i < 8; ++i) {
    const float angle = (float)i * kTwoPi / 8.0f;
    spline.points.emplace_back(kRailRadius * std::cos(angle), kRailHeight,
                               kRailRadius * std::sin(angle));
  }
  spline.closed = true;
  return spline;
}

}  // namespace

Study firstLight() {
  Study study;
  study.name = "first_light";
  study.canvas = {900, 640};
  study.captureSeconds = 1.4f;
  study.background = {0.035f, 0.04f, 0.055f, 1.0f};

  study.describe = [](float seconds) {
    const Spline3 loop = ribbon();
    const Spline3 track = rail();

    const gm::Mesh tube =
        gm::curve::sweep(loop, gm::curve::profile::circle(16),
                         {.segments = 220,
                          .scale = 9.0f,
                          .normals = gm::curve::SweepOptions::Normals::Radial});

    const std::vector<glm::vec3> path = loop.sampleArcLength(96);
    const float head = std::fmod(seconds * 0.35f, 1.0f);
    const Chain comet =
        gm::pop::on(path)
            .count(1200)
            .spread(13.0f)
            .vary(0.5f, 1.0f)
            .fade({1.0f, 0.72f, 0.35f, 1.0f}, {0.25f, 0.55f, 1.0f, 1.0f});

    // The camera's node rides the rail, so its eye and its target are
    // written in the rail's own moving frame: the binormal points inward
    // at every station, so the set's centre is one rail radius along it
    // and one rail height down, whatever the node has travelled.
    Camera lens;
    lens.eye = {0.0f, 0.0f, 0.0f};
    lens.target = {kRailRadius, -kRailHeight, 0.0f};
    lens.fovYDeg = 44.0f;
    const float travelled = seconds * 0.06f * track.length();

    return Element()
        .key("set")
        .child(Element().key("sun").light(
            sun({-0.45f, -0.8f, -0.4f}, {1.0f, 0.96f, 0.9f, 1.0f}, 0.95f)))
        .child(Element()
                   .key("lamp")
                   .at({180, 150, 220})
                   .light(point({0, 0, 0}, {0.45f, 0.6f, 1.0f, 1.0f}, 0.9f,
                                900.0f)))
        .child(Element().key("camera").along(track, travelled).camera(lens))
        .child(Element()
                   .key("plate")
                   .at({0, -150, 0})
                   .rotateX(-90.0f)
                   .mesh(gm::quad(900, 900))
                   .fill(paint({0.10f, 0.11f, 0.14f, 1.0f}))
                   .tag("ground"))
        .child(Element()
                   .key("tube")
                   .mesh(tube)
                   .fill(paint({0.62f, 0.66f, 0.74f, 1.0f}))
                   .tag("lit"))
        .child(Element()
                   .key("comet")
                   .chain(comet)
                   .stamp(gm::superellipsoid({4.0f, 4.0f, 4.0f}, 2.0f, 10, 8))
                   .window(head, 0.28f)
                   .fill(paint({0.95f, 0.75f, 0.42f, 1.0f}))
                   .tag("glow"));
  };
  return study;
}

}  // namespace sigil::world::testing
